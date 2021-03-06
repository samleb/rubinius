#include "vm/test/test.hpp"

#include "instruments/profiler.hpp"

#include "builtin/symbol.hpp"

class TestProfiler : public CxxTest::TestSuite, public VMTest {
public:

  void setUp() {
    create();
  }

  void tearDown() {
    destroy();
  }

  void test_method_name_normal() {
    Symbol* meth = state->symbol("meth");
    Symbol* klass = state->symbol("Object");

    profiler::Method method(0U, meth, klass, profiler::kNormal);
    String* name = method.name(state);

    TS_ASSERT_SAME_DATA("Object#meth", name->byte_address(), 11);
  }

  void test_method_name_singleton() {
    Symbol* meth = state->symbol("meth");
    Symbol* klass = state->symbol("Object");

    profiler::Method method(0U, meth, klass, profiler::kSingleton);
    String* name = method.name(state);

    TS_ASSERT_SAME_DATA("Object.meth", name->byte_address(), 11);
  }

  void test_method_name_block() {
    Symbol* meth = state->symbol("meth");
    Symbol* klass = state->symbol("Object");

    profiler::Method method(0U, meth, klass, profiler::kBlock);
    String* name = method.name(state);

    TS_ASSERT_SAME_DATA("Object#meth {}", name->byte_address(), 14);
  }

  void test_module_name() {
    Symbol* name = state->symbol("ModName");

    Module* mod = Module::create(state);
    mod->name(state, name);

    IncludedModule* im = IncludedModule::create(state);
    im->module(state, mod);

    profiler::Profiler prof(state);

    TS_ASSERT_EQUALS(name, prof.module_name(im));
    TS_ASSERT_EQUALS(name, prof.module_name(mod));
  }

  void test_enter_block() {
    /*
    Symbol* meth = state->symbol("meth");
    Symbol* klass = state->symbol("Object");

    MethodContext* ctx = MethodContext::create(state, 10);
    ctx->name(state, meth);
    ctx->module(state, G(object));

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->name(state, meth);

    profiler::Profiler prof;

    prof.enter_block(state, ctx, cm);
    TS_ASSERT_EQUALS(prof.depth(), 1U);
    TS_ASSERT_EQUALS(prof.number_of_entries(), 1U);

    profiler::Key key(meth, klass, profiler::kBlock);
    profiler::Method* mo = prof.find_key(key);
    TS_ASSERT(mo);
    */
  }

  void test_enter_method() {
    Symbol* meth = state->symbol("meth");
    Symbol* klass = state->symbol("Object");

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->name(state, meth);

    profiler::Profiler prof(state);

    Dispatch dis(meth, G(object), cm);
    Arguments args;

    prof.enter_method(dis, args, cm);
    TS_ASSERT_EQUALS(prof.depth(), 1U);
    TS_ASSERT_EQUALS(prof.number_of_entries(), 1U);

    profiler::Key key(meth, klass, profiler::kNormal);
    TS_ASSERT(prof.find_key(key));

    dis.module = G(object)->metaclass(state);

    prof.enter_method(dis, args, cm);
    TS_ASSERT_EQUALS(prof.depth(), 2U);
    TS_ASSERT_EQUALS(prof.number_of_entries(), 2U);

    profiler::Key key1(meth, klass, profiler::kNormal);
    TS_ASSERT(prof.find_key(key1));
  }

  void test_enter_method_with_no_compiledmethod() {
    Symbol* meth = state->symbol("meth");
    Symbol* klass = state->symbol("Object");

    CompiledMethod* cm = CompiledMethod::create(state);
    cm->name(state, meth);

    profiler::Profiler prof(state);

    Dispatch dis(meth, G(object), cm);
    Arguments args;

    prof.enter_method(dis, args);
    TS_ASSERT_EQUALS(prof.depth(), 1U);
    TS_ASSERT_EQUALS(prof.number_of_entries(), 1U);

    profiler::Key key(meth, klass, profiler::kNormal);
    TS_ASSERT(prof.find_key(key));

    dis.module = G(object)->metaclass(state);

    prof.enter_method(dis, args);
    TS_ASSERT_EQUALS(prof.depth(), 2U);
    TS_ASSERT_EQUALS(prof.number_of_entries(), 2U);

    profiler::Key key1(meth, klass, profiler::kNormal);
    TS_ASSERT(prof.find_key(key1));
  }

  void test_record_method() {
    Symbol* meth = state->symbol("blah");
    Symbol* klass = state->symbol("Sweet");

    CompiledMethod* cm = CompiledMethod::create(state);

    profiler::Profiler prof(state);

    prof.record_method(cm, meth, klass);
    TS_ASSERT_EQUALS(prof.depth(), 1U);
    TS_ASSERT_EQUALS(prof.number_of_entries(), 1U);

    profiler::Key key(meth, klass);
    profiler::Method* mo = prof.find_key(key);
    TS_ASSERT(mo);

    TS_ASSERT_EQUALS(mo->container(), klass);
    TS_ASSERT_EQUALS(mo->method(), meth);
    TS_ASSERT_EQUALS(mo->total_time(), 0ULL);

    prof.record_method(cm, meth, klass);
    TS_ASSERT_EQUALS(prof.number_of_entries(), 1U);
    TS_ASSERT_EQUALS(prof.depth(), 2U);

    Symbol* meth2 = state->symbol("woo");
    prof.record_method(cm, meth2, klass);
    TS_ASSERT_EQUALS(prof.number_of_entries(), 2U);

    TS_ASSERT_EQUALS(prof.depth(), 3U);
  }

  void test_leave() {
    Symbol* meth = state->symbol("blah");
    Symbol* klass = state->symbol("Sweet");

    CompiledMethod* cm = CompiledMethod::create(state);

    profiler::Profiler prof(state);

    prof.record_method(cm, meth, klass);
    profiler::Key key(meth, klass);
    profiler::Method* mo = prof.find_key(key);
    TS_ASSERT_EQUALS(mo->total_time(), 0ULL);

    usleep(10000);
    prof.leave();

    TS_ASSERT_EQUALS(prof.depth(), 0U);
    TS_ASSERT(mo->total_time() > 100);
  }

  void test_leave_adds_leaves() {
    Symbol* meth = state->symbol("blah");
    Symbol* klass = state->symbol("Sweet");

    CompiledMethod* cm = CompiledMethod::create(state);

    profiler::Profiler prof(state);

    profiler::Method* outer = prof.record_method(cm, meth, klass);
    TS_ASSERT_EQUALS(prof.current_method(), outer);

    Symbol* meth2 = state->symbol("fun");

    profiler::Method* inner = prof.record_method(cm, meth2, klass);
    TS_ASSERT_EQUALS(prof.current_method(), inner);

    prof.leave();

    prof.leave();

    profiler::Key key(meth, klass);
    profiler::Method* mo = prof.find_key(key);

    TS_ASSERT_EQUALS(mo->number_of_leaves(), 1U);
    profiler::Leaves::iterator i = mo->leaves_begin();
    profiler::Leaf* leaf = i->second;

    profiler::Key key2(meth2, klass);
    profiler::Method* mo2 = prof.find_key(key2);

    TS_ASSERT_EQUALS(leaf->method(), mo2);

    TS_ASSERT(leaf->total_time() > 0);
    TS_ASSERT_EQUALS(leaf->total_time(), mo2->total_time());
  }

  void test_results() {
    Symbol* meth = state->symbol("blah");
    Symbol* meth2 = state->symbol("foo");
    Symbol* meth3 = state->symbol("done");
    Symbol* klass = state->symbol("Sweet");
    Symbol* called = state->symbol("called");
    Symbol* name = state->symbol("name");

    CompiledMethod* cm = CompiledMethod::create(state);

    profiler::ProfilerCollection collection(state);

    profiler::Profiler* prof = new profiler::Profiler(state);
    collection.add_profiler(state, prof);

    profiler::Method* top = prof->record_method(cm, meth, klass);
    prof->record_method(cm, meth2, klass);
    prof->leave();

    TS_ASSERT_EQUALS(prof->current_method(), top);
    prof->record_method(cm, meth3, klass);
    prof->leave();

    TS_ASSERT_EQUALS(prof->current_method(), top);

    prof->record_method(cm, meth2, klass);
    prof->record_method(cm, meth3, klass);
    prof->leave();
    prof->leave();

    prof->leave();

    LookupTable* results = collection.results(state);

    TS_ASSERT(!results->nil_p());
    TS_ASSERT(!results->fetch(state, state->symbol("method"))->nil_p());

    LookupTable* methods = as<LookupTable>(
        results->fetch(state, state->symbol("methods")));
    TS_ASSERT_EQUALS(Integer::from(state, 3), methods->entries());

    LookupTable* method = as<LookupTable>(methods->fetch(state, Fixnum::from(1)));
    TS_ASSERT_EQUALS(Integer::from(state, 6), method->entries());
    TS_ASSERT_EQUALS(Fixnum::from(1), method->fetch(state, called));
    TS_ASSERT_SAME_DATA("Sweet#blah", as<String>(method->fetch(state,
          name))->byte_address(), 10);
    TS_ASSERT(kind_of<Array>(method->fetch(state, state->symbol("leaves"))));

    method = as<LookupTable>(methods->fetch(state, Fixnum::from(2)));
    TS_ASSERT_EQUALS(Fixnum::from(2), method->fetch(state, called));
    TS_ASSERT_SAME_DATA("Sweet#foo", as<String>(method->fetch(state,
          name))->byte_address(), 9);

    method = as<LookupTable>(methods->fetch(state, Fixnum::from(3)));
    TS_ASSERT_EQUALS(Fixnum::from(2), method->fetch(state, called));
    TS_ASSERT_SAME_DATA("Sweet#done", as<String>(method->fetch(state,
          name))->byte_address(), 10);

    TS_ASSERT(method->has_key(state, state->symbol("total"))->true_p());
    TS_ASSERT(method->has_key(state, state->symbol("file"))->true_p());
    TS_ASSERT(method->has_key(state, state->symbol("line"))->true_p());
  }
};
