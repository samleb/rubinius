#include "vm.hpp"
#include "objectmemory.hpp"
#include "event.hpp"
#include "global_cache.hpp"
#include "llvm.hpp"

#include "vm/object_utils.hpp"

#include "builtin/class.hpp"
#include "builtin/fixnum.hpp"
#include "builtin/list.hpp"
#include "builtin/lookuptable.hpp"
#include "builtin/symbol.hpp"
#include "builtin/thread.hpp"
#include "builtin/tuple.hpp"
#include "builtin/string.hpp"
#include "builtin/taskprobe.hpp"

#include "instruments/profiler.hpp"

#include "config_parser.hpp"
#include "config.h"
#include "vm_manager.hpp"

#include "native_thread.hpp"

#include <iostream>
#include <signal.h>

// Reset macros since we're inside state
#undef G
#undef GO
#define G(whatever) globals.whatever.get()
#define GO(whatever) globals.whatever

namespace rubinius {

  int VM::cStackDepthMax = 655300;

  SharedState::~SharedState() {
    if(!initialized_) return;

    delete om;
    delete global_cache;
    delete user_config;

#ifdef ENABLE_LLVM
    if(!reuse_llvm) llvm_cleanup();
#endif
  }

  /** @see VMManager::create_vm */
  VM* SharedState::new_vm() {
    VM* vm = manager_.create_vm(this);
    vms_[vm->id()] = vm;
    cf_locations_.push_back(vm->call_frame_location());
    return vm;
  }

  /** @see VMManager::destroy_vm. */
  void SharedState::remove_vm(VM* vm) {
    VMMap::iterator i = vms_.find(vm->id());
    assert(i != vms_.end());
    vms_.erase(i);

    cf_locations_.remove(vm->call_frame_location());
  }

  void SharedState::enable_profiling(VM* vm) {
    profiler_collection_ = new profiler::ProfilerCollection(vm);
    profiling_ = true;
  }

  LookupTable* SharedState::disable_profiling(VM* vm) {
    if(profiler_collection_) {
      LookupTable* profile = profiler_collection_->results(vm);
      delete profiler_collection_;
      profiler_collection_ = 0;
      profiling_ = false;
      return profile;
    } else {
      return reinterpret_cast<LookupTable*>(Qnil);
    }
  }

  void SharedState::add_profiler(VM* vm, profiler::Profiler* profiler) {
    if(profiler_collection_) {
      profiler_collection_->add_profiler(vm, profiler);
    }
  }

  void SharedState::remove_profiler(VM* vm, profiler::Profiler* profiler) {
    if(profiler_collection_) {
      profiler_collection_->remove_profiler(vm, profiler);
    }
  }

  VM::VM(SharedState& shared, int id)
    : id_(id)
    , saved_call_frame_(0)
    , alive_(true)
    , profiler_(NULL)
    , shared(shared)
    , waiter_(NULL)
    , user_config(shared.user_config)
    , globals(shared.globals)
    , om(shared.om)
    , global_cache(shared.global_cache)
    , config(shared.config)
    , interrupts(shared.interrupts)
    , symbols(shared.symbols)
    , check_local_interrupts(false)
    , thread_state_(this)
    , thread(this, (Thread*)Qnil)
    , current_mark(NULL)
    , reuse_llvm(true)
    , use_safe_position(false)
  {}

  void VM::discard() {
    alive_ = false;
    saved_call_frame_ = 0;
    if(profiler_) {
      shared.remove_profiler(this, profiler_);
    }
  }

  void VM::initialize(size_t bytes)
  {
    config.compile_up_front = false;
    config.jit_enabled = false;
    config.dynamic_interpreter_enabled = false;

    VM::register_state(this);

    if(user_config) {
      if(ConfigParser::Entry* entry = user_config->find("rbx.gc.young_space")) {
        bytes = entry->to_i();
      }
    }

    om = new ObjectMemory(this, bytes);
    shared.om = om;

    probe.set(Qnil, &globals.roots);

    global_cache = new GlobalCache;
    shared.global_cache = global_cache;

    /** @todo Done by Environment::boot_vm(), and Thread::s_new()
     *        does not boot at all. Should this be removed? --rue */
//    this->boot();

    shared.set_initialized();
  }

  void VM::boot() {
#ifdef USE_USAGE_JIT
    if(user_config->find("rbx.jit")) {
      config.jit_enabled = true;
    }
#endif

#ifdef USE_DYNAMIC_INTERPRETER
    if(user_config->find("rbx.dyni")) {
      config.dynamic_interpreter_enabled = true;
    }
#endif

//    MethodContext::initialize_cache(this);
    TypeInfo::auto_learn_fields(this);

    bootstrap_ontology();

    /* @todo Using a single default loop, revisit when many loops.
     * @todo This needs to be handled through the environment.
     * (disabled epoll backend as it frequently caused hangs on epoll_wait)
     */
    // signal_events = new event::Loop(EVFLAG_FORKCHECK | EVBACKEND_SELECT | EVBACKEND_POLL);
    // events = signal_events;

    // signal_events->start(new event::Child::Event(this));

    events = 0;
    signal_events = 0;

    VMMethod::init(this);

#ifdef ENABLE_LLVM
    VMLLVMMethod::init("vm/instructions.bc");
#endif

    /** @todo Should a thread be starting a VM or is it the other way around? */
    boot_threads();

    // Force these back to false because creating the default Thread
    // sets them to true.
    interrupts.use_preempt = false;
    interrupts.enable_preempt = false;
  }

  // HACK so not thread safe or anything!
  static VM* __state = NULL;

  VM* VM::current_state() {
    return __state;
  }

  void VM::register_state(VM *vm) {
    __state = vm;
  }

  void VM::boot_threads() {
    thread.set(Thread::create(this, this), &globals.roots);
    thread->sleep(this, Qfalse);
  }

  Object* VM::new_object_typed(Class* cls, size_t bytes, object_type type) {
    return om->new_object_typed(cls, bytes, type);
  }

  Object* VM::new_object_typed_mature(Class* cls, size_t bytes, object_type type) {
    return om->new_object_typed_mature(cls, bytes, type);
  }

  Object* VM::new_object_from_type(Class* cls, TypeInfo* ti) {
    return om->new_object_typed(cls, ti->instance_size, ti->type);
  }

  Class* VM::new_basic_class(Class* sup) {
    Class *cls = new_object<Class>(G(klass));
    if(sup->nil_p()) {
      cls->instance_type(this, Fixnum::from(ObjectType));
      cls->set_type_info(find_type(ObjectType));
    } else {
      cls->instance_type(this, sup->instance_type()); // HACK test that this is always true
      cls->set_type_info(sup->type_info());
    }
    cls->superclass(this, sup);

    return cls;
  }

  Class* VM::new_class(const char* name) {
    return new_class(name, G(object), G(object));
  }

  Class* VM::new_class(const char* name, Class* super_class) {
    return new_class(name, super_class, G(object));
  }

  Class* VM::new_class(const char* name, Class* sup, Module* under) {
    Class* cls = new_basic_class(sup);
    cls->setup(this, name, under);

    // HACK test that we've got the MOP setup properly
    MetaClass::attach(this, cls, sup->metaclass(this));
    return cls;
  }

  Class* VM::new_class_under(const char* name, Module* under) {
    return new_class(name, G(object), under);
  }

  Module* VM::new_module(const char* name, Module* under) {
    Module *mod = new_object<Module>(G(module));
    mod->setup(this, name, under);
    return mod;
  }


  Symbol* VM::symbol(const char* str) {
    return symbols.lookup(this, str);
  }

  Symbol* VM::symbol(String* str) {
    return symbols.lookup(this, str);
  }

  Symbol* VM::symbol(std::string str) {
    return symbols.lookup(this, str);
  }

  void type_assert(STATE, Object* obj, object_type type, const char* reason) {
    if((obj->reference_p() && obj->type_id() != type)
        || (type == FixnumType && !obj->fixnum_p())) {
      Exception::type_error(state, type, obj, reason);
    }
  }

  void VM::raise_stack_error() {
    thread_state()->raise_exception(G(stack_error));
  }

  void VM::init_stack_size() {
    struct rlimit rlim;
    if (getrlimit(RLIMIT_STACK, &rlim) == 0) {
      unsigned int space = rlim.rlim_cur/5;

      if (space > 1024*1024) space = 1024*1024;
      cStackDepthMax = (rlim.rlim_cur - space);
    }
  }

  TypeInfo* VM::find_type(int type) {
    return om->type_info[type];
  }

  Thread *VM::current_thread() {
    return globals.current_thread.get();
  }

  void VM::run_gc_soon() {
    om->collect_young_now = true;
    om->collect_mature_now = true;
    interrupts.check = true;
  }

  void VM::collect(CallFrame* call_frame) {
    this->set_call_frame(call_frame);

    GCData gc_data(this);

    om->collect_young(gc_data);
    om->collect_mature(gc_data);
    global_cache->clear();
  }

  void VM::collect_maybe(CallFrame* call_frame) {
    this->set_call_frame(call_frame);
    GCData gc_data(this);

    if(om->collect_young_now) {
      om->collect_young_now = false;

      om->collect_young(gc_data);

      global_cache->clear();
    }

    if(om->collect_mature_now) {
      om->collect_mature_now = false;

      om->collect_mature(gc_data);

      global_cache->clear();

      shared.manager().prune();
    }
  }

  void VM::set_const(const char* name, Object* val) {
    globals.object->set_const(this, (char*)name, val);
  }

  void VM::set_const(Module* mod, const char* name, Object* val) {
    mod->set_const(this, (char*)name, val);
  }

  void VM::print_backtrace() {
    abort();
  }

  void VM::raise_exception_safely(Exception* exc) {
    abort();
    safe_position_data.exc = exc;
    siglongjmp(safe_position, cReasonException);
    // Never reached.
  }

  void VM::raise_typeerror_safely(TypeError* err) {
    abort();
    safe_position_data.type_error = err;
    siglongjmp(safe_position, cReasonTypeError);
    // Never reached.
  }

  void VM::raise_assertion_safely(Assertion* err) {
    abort();
    safe_position_data.assertion = err;
    siglongjmp(safe_position, cReasonAssertion);
    // Never reached.
  }

  // Trampoline to call scheduler_loop()
  static void* __thread_tramp__(void* arg) {
    VM* state = static_cast<VM*>(arg);
    state->scheduler_loop();
    return NULL;
  }

  // Create the preemption thread and call scheduler_loop() in the new thread
  void VM::setup_preemption() {
    if(pthread_create(&preemption_thread, NULL, __thread_tramp__, this) != 0) {
      std::cout << "Unable to create preemption thread!\n";
    }
  }

  // Runs forever, telling the VM to reschedule threads ever 10 milliseconds
  void VM::scheduler_loop() {
    // First off, we don't want this thread ever receiving a signal.
    sigset_t mask;
    sigfillset(&mask);
    if(pthread_sigmask(SIG_SETMASK, &mask, NULL) != 0) {
      abort();
    }

    struct timespec requested;
    struct timespec actual;

    requested.tv_sec = 0;
    requested.tv_nsec = 10000000; // 10 milliseconds

    for(;;) {
      nanosleep(&requested, &actual);
      if(interrupts.enable_preempt) {
        interrupts.reschedule = true;
        interrupts.check_events = true;
        interrupts.timer = true;
      }
    }
  }

  void VM::install_waiter(Waiter& waiter) {
    waiter_ = &waiter;
  }

  bool VM::wakeup() {
    if(waiter_) {
      waiter_->wakeup();
      waiter_ = NULL;
      return true;
    }

    return false;
  }

  void VM::clear_waiter() {
    waiter_ = NULL;
  }

  void VM::send_async_signal(int sig) {
    mailbox_.add(ASyncMessage(ASyncMessage::cSignal, sig));
    check_local_interrupts = true;

    // TODO I'm worried there might be a race calling
    // wakeup without the lock held...
    wakeup();
  }

  bool VM::process_async(CallFrame* call_frame) {
    while(!mailbox_.empty_p()) {
      ASyncMessage msg = mailbox_.pop();
      switch(msg.type()) {
      case ASyncMessage::cSignal: {
        Array* args = Array::create(this, 1);
        args->set(this, 0, Fixnum::from(msg.data()));

        Object* ret = G(rubinius)->send(this, call_frame,
            symbol("received_signal"), args, Qnil);

        if(!ret) return false;
      }
      }
    }

    check_local_interrupts = false;

    if(thread_state_.raise_reason() != cNone) {
      return false;
    }

    return true;
  }

  void VM::register_raise(Exception* exc) {
    thread_state_.raise_exception(exc);
    check_local_interrupts = true;
  }

  profiler::Profiler* VM::profiler() {
    if(unlikely(!profiler_)) {
      profiler_ = new profiler::Profiler(this);
      shared.add_profiler(this, profiler_);
    }

    return profiler_;
  }

  void VM::remove_profiler() {
    profiler_ = 0;
  }
};
