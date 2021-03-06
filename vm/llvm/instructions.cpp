/** \file   Base template for generating vm/gen/instructions.cpp. */

#include "builtin/object.hpp"
#include "builtin/array.hpp"
#include "builtin/autoload.hpp"
#include "builtin/block_environment.hpp"
#include "builtin/class.hpp"
#include "builtin/compiledmethod.hpp"
#include "builtin/exception.hpp"
#include "builtin/fixnum.hpp"
#include "builtin/sendsite.hpp"
#include "builtin/string.hpp"
#include "builtin/symbol.hpp"
#include "builtin/taskprobe.hpp"
#include "builtin/tuple.hpp"
#include "builtin/iseq.hpp"
#include "builtin/staticscope.hpp"
#include "builtin/nativemethod.hpp"
#include "builtin/lookuptable.hpp"
#include "builtin/proc.hpp"
#include "builtin/thread.hpp"
#include "builtin/system.hpp"

#include "call_frame.hpp"

#include "objectmemory.hpp"
#include "arguments.hpp"
#include "dispatch.hpp"
#include "instructions.hpp"
#include "instruments/profiler.hpp"

#include "helpers.hpp"

using namespace rubinius;

// HACK: sassert is stack protection
#ifdef RBX_DEBUG

#define stack_push(val) ({ Object* _v = (val); sassert(_v && (int)_v != 0x10 && call_frame->js.stack < call_frame->js.stack_top); *++call_frame->js.stack = _v; })
#define stack_pop() ({ assert(call_frame->js.stack >= call_frame->stk); *call_frame->js.stack--; })
#define stack_set_top(val) ({ Object* _v = (val); assert((int)(_v) != 0x10); *call_frame->js.stack = _v; })

#else

/** We have to use the local here we need to evaluate val before we alter
 * the stack. The reason is evaluating val might throw an exception. The
 * old code used an undefined behavior, this forces the order. */
#define stack_push(val) ({ Object* __stack_v = (val); *++call_frame->js.stack = __stack_v; })
#define stack_pop() *call_frame->js.stack--
#define stack_set_top(val) *call_frame->js.stack = (val)

#define USE_JUMP_TABLE

#endif

#define stack_top() *call_frame->js.stack
#define stack_back(count) *(call_frame->js.stack - count)
#define stack_clear(count) call_frame->clear_stack(count)

#define SHOW(obj) (((NormalObject*)(obj))->show(state))

#define both_fixnum_p(_p1, _p2) ((uintptr_t)(_p1) & (uintptr_t)(_p2) & TAG_FIXNUM)

#define cache_ip()

extern "C" {
  Object* send_slowly(STATE, VMMethod* vmm, CallFrame* const call_frame, Symbol* name, size_t args);

#define HANDLE_EXCEPTION(val) if(val == NULL) return NULL
#define RUN_EXCEPTION() return NULL

#define RETURN(val) return val

#define SET_CALL_FLAGS(val) is.call_flags = (val)
#define CALL_FLAGS() is.call_flags

#define SET_ALLOW_PRIVATE(val) is.allow_private = (val)
#define ALLOW_PRIVATE() is.allow_private

#define DISPATCH return NULL

#ruby <<CODE
require 'stringio'
require 'vm/instructions.rb'
si = Instructions.new
impl = si.decode_methods
io = StringIO.new
si.generate_functions impl, io
puts io.string
CODE

  Object* send_slowly(STATE, VMMethod* vmm, CallFrame* const call_frame, Symbol* name, size_t count) {
    Object* recv = stack_back(count);
    Arguments args(recv, count, call_frame->stack_back_position(count - 1));
    Dispatch dis(name);

    return dis.send(state, call_frame, args);
  }
}

/* A simple interface to the instructions by directly
 * executing an opcode stream. This is used primarly to
 * debug instructions. */

#define next_op() *stream++
#define next_int (opcode)(next_op())

#undef RETURN
#define RETURN(val) (void)val; return;

#if 0

Object* rubinius::Task::execute_stream(CallFrame* call_frame, opcode* stream) {
  opcode op;
  int call_flags = 0;
  VMMethod* const vmm = call_frame->vmm;

  op = next_op();

#ruby <<CODE
io = StringIO.new
si.generate_decoder_switch impl, io
puts io.string
CODE

  return Qnil;
}

#endif

/* Use a simplier next_int */
#undef next_int
#define next_int ((opcode)(stream[call_frame->ip++]))

#undef RETURN
#define RETURN(val) (void)val; DISPATCH;

Object* VMMethod::interpreter(STATE, VMMethod* const vmm, CallFrame* const call_frame) {
  opcode* stream = vmm->opcodes;
  InterpreterState is;
#ifdef USE_JUMP_TABLE

#undef DISPATCH
#define DISPATCH goto *insn_locations[stream[call_frame->ip++]];

#ruby <<CODE
io = StringIO.new
impl2 = si.decode_methods
si.inject_superops impl2
si.generate_jump_implementations impl2, io, true
puts io.string
CODE

#else
  opcode op;

#undef DISPATCH
#define DISPATCH continue;
  for(;;) {
    op = stream[call_frame->ip++];

#if FLAG_FIRE_PROBE_INSTRUCTION
/** @todo Probe is in VM now, fix. --rue */
//    if(!task->probe()->nil_p()) {
//      task->probe()->execute_instruction(task, call_frame, op);
//    }
#endif

#ruby <<CODE
io = StringIO.new
si.generate_decoder_switch impl, io, true
puts io.string
CODE
  }
#endif // USE_JUMP_TABLE
}


/* The debugger interpreter loop is used to run a method when a breakpoint
 * has been set. It has additional overhead, since it needs to inspect
 * each opcode for the breakpoint flag. It is installed on the VMMethod when
 * a breakpoint is set on compiled method.
 */
Object* VMMethod::debugger_interpreter(STATE, VMMethod* const vmm, CallFrame* const call_frame) {
  opcode* stream = vmm->opcodes;
  InterpreterState is;

  opcode op;
#ifdef USE_JUMP_TABLE

#undef DISPATCH
#define DISPATCH op = stream[call_frame->ip++]; \
  if(unlikely(op & cBreakpoint)) { \
    call_frame->ip--; \
    Helpers::yield_debugger(state, call_frame); \
    call_frame->ip++; \
    op &= 0x00ffffff; \
  } \
  goto *insn_locations[op];

#ruby <<CODE
io = StringIO.new
impl2 = si.decode_methods
si.inject_superops impl2
si.generate_jump_implementations impl2, io, true
puts io.string
CODE

#else
#error "not supported"

  for(;;) {
    op = stream[call_frame->ip++];

#if FLAG_FIRE_PROBE_INSTRUCTION
/** @todo Probe is in VM now, fix. --rue */
//    if(!task->probe()->nil_p()) {
//      task->probe()->execute_instruction(task, call_frame, op);
//    }
#endif

#ruby <<CODE
io = StringIO.new
si.generate_decoder_switch impl, io, true
puts io.string
CODE
  }
#endif // USE_JUMP_TABLE
}

namespace rubinius {
namespace instructions {
#ruby <<CODE
puts si.generate_implementation_info
CODE
}
}
