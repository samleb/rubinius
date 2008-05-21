#include "builtin.hpp"
#include "ffi.hpp"
#include "marshal.hpp"
#include "primitives.hpp"

namespace rubinius {
  CompiledMethod* CompiledMethod::create(STATE) {
    CompiledMethod* cm = (CompiledMethod*)state->new_object(G(cmethod));
    return cm;
  }

  CompiledMethod* CompiledMethod::generate_tramp(STATE) {
    CompiledMethod* cm = CompiledMethod::create(state);

    // Use a stack of 1 so that the return value of the executed method
    // has a place to go
    SET(cm, stack_size, Object::i2n(1));
    SET(cm, required_args, Object::i2n(0));
    SET(cm, total_args, cm->required_args);

    SET(cm, iseq, InstructionSequence::create(state, 1));
    cm->iseq->opcodes->put(state, 0, Object::i2n(InstructionSequence::insn_halt));

    StaticScope* ss = StaticScope::create(state);
    SET(ss, module, G(object));
    SET(cm, scope, ss);
    return cm;
  }

  MethodVisibility* MethodVisibility::create(STATE) {
    return (MethodVisibility*)state->new_object(G(cmethod_vis));
  }

  VMMethod* CompiledMethod::vmmethod(STATE) {
    if(compiled->nil_p()) {
      if(!primitive->nil_p()) {
        if(SYMBOL name = try_as<Symbol>(primitive)) {
          std::cout << "resolving: "; inspect(state, name);
          primitive_func func = Primitives::resolve_primitive(state, name);

          VMPrimitiveMethod* vmm = new VMPrimitiveMethod(state, this, func);
          compiled = MemoryPointer::create(state, vmm);
          return vmm;
        } else {
          std::cout << "Invalid primitive id (not a symbol)" << std::endl;
        }
      }
      VMMethod* vmm = new VMMethod(state, this);
      compiled = MemoryPointer::create(state, vmm);
      return vmm;
    }

    return (VMMethod*)compiled->pointer;
  }

  void CompiledMethod::post_marshal(STATE) {

  }

  size_t CompiledMethod::number_of_locals() {
    return 0;
  }
  void CompiledMethod::set_scope(StaticScope* scope) {
  }

}