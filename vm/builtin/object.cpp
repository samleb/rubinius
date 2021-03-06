#include <iostream>
#include <sstream>

#include <cstdarg>

#include "builtin/object.hpp"
#include "builtin/bignum.hpp"
#include "builtin/class.hpp"
#include "builtin/compactlookuptable.hpp"
#include "builtin/fixnum.hpp"
#include "builtin/lookuptable.hpp"
#include "builtin/symbol.hpp"
#include "builtin/string.hpp"
#include "builtin/tuple.hpp"
#include "builtin/array.hpp"
#include "builtin/selector.hpp"
#include "builtin/float.hpp"
#include "objectmemory.hpp"
#include "arguments.hpp"
#include "dispatch.hpp"
#include "lookup_data.hpp"

#include "vm/object_utils.hpp"

namespace rubinius {

  Object* Object::change_class_to(STATE, Class* other_klass) {
    this->klass(state, other_klass);

    return this;
  }

  Class* Object::class_object(STATE) const {
    if(reference_p()) {
      Module* mod = klass_;
      while(!mod->nil_p() && !instance_of<Class>(mod)) {
        mod = as<Module>(mod->superclass());
      }

      if(mod->nil_p()) {
        Exception::assertion_error(state, "Object::class_object() failed to find a class");
      }
      return as<Class>(mod);
    }

    return state->globals.special_classes[((uintptr_t)this) & SPECIAL_CLASS_MASK].get();
  }

  void Object::cleanup(STATE) {
    type_info(state)->cleanup(this);
  }

  Object* Object::clone(STATE) {
    Object* other = dup(state);

    other->copy_internal_state_from(state, this);

    return other;
  }

  void Object::copy_internal_state_from(STATE, Object* original) {
    if(MetaClass* mc = try_as<MetaClass>(original->klass())) {
      LookupTable* source_methods = mc->method_table()->dup(state);
      LookupTable* source_constants = mc->constants()->dup(state);

      this->metaclass(state)->method_table(state, source_methods);
      this->metaclass(state)->constants(state, source_constants);

      // This allows us to preserve included modules
      this->metaclass(state)->superclass(state, mc->superclass());
    }
  }

  Object* Object::dup(STATE) {
    Object* other = state->om->allocate_object(this->total_size(state));

#ifdef RBX_GC_STATS
    // This counter is only valid if the line above allocates in the
    // young object space.
    stats::GCStats::get()->young_object_types[this->type_id()]++;
#endif

    other->initialize_copy(this, age);
    other->copy_body(state, this);

    // Set the dup's class this's class
    other->klass(state, class_object(state));

    // HACK: If other is mature, remember it.
    // We could inspect inspect the references we just copied to see
    // if there are any young ones if other is mature, then and only
    // then remember other. The up side to just remembering it like
    // this is that other is rarely mature, and the remember_set is
    // flushed on each collection anyway.
    if(other->zone == MatureObjectZone) {
      state->om->remember_object(other);
    }

    // Copy ivars.
    if(ivars_->reference_p()) {
      // NOTE Don't combine these 2 branches even though they both just call
      // ::dup. There is a special LookupTable::dup that can only be seen
      // when the receiver is of LookupTable* type. Without the explicit cast
      // and call, the wrong one will be called.
      if(LookupTable* lt = try_as<LookupTable>(ivars_)) {
        other->ivars_ = lt->dup(state);
        LookupTable* ld = as<LookupTable>(other->ivars_);

        // We store the object_id in the ivar table, so nuke it.
        ld->remove(state, G(sym_object_id));
        ld->remove(state, state->symbol("frozen"));
      } else {
        // Use as<> so that we throw a TypeError if there is something else
        // here.
        CompactLookupTable* clt = as<CompactLookupTable>(ivars_);
        other->ivars_ = clt->dup(state);
        CompactLookupTable* ld = as<CompactLookupTable>(other->ivars_);

        // We store the object_id in the ivar table, so nuke it.
        ld->remove(state, G(sym_object_id));
        ld->remove(state, state->symbol("frozen"));
      };
    }

    return other;
  }

  Object* Object::equal(STATE, Object* other) {
    return this == other ? Qtrue : Qfalse;
  }

  Object* Object::freeze(STATE) {
    if(reference_p()) {
      set_ivar(state, state->symbol("frozen"), Qtrue);
    }
    return this;
  }

  Object* Object::frozen_p(STATE) {
    if(reference_p()) {
      if(get_ivar(state, state->symbol("frozen"))->nil_p()) return Qfalse;
      return Qtrue;
    } else {
      return Qfalse;
    }
  }

  Object* Object::get_field(STATE, size_t index) {
    return type_info(state)->get_field(state, this, index);
  }

  Object* Object::get_ivar(STATE, Symbol* sym) {
    /* Implements the external ivars table for objects that don't
       have their own space for ivars. */
    if(!reference_p()) {
      LookupTable* tbl = try_as<LookupTable>(G(external_ivars)->fetch(state, this));

      if(tbl) return tbl->fetch(state, sym);
      return Qnil;
    }

    // We might be trying to access a slot, so try that first.

    TypeInfo* ti = state->om->find_type_info(this);
    if(ti) {
      TypeInfo::Slots::iterator it = ti->slots.find(sym->index());
      if(it != ti->slots.end()) {
        return ti->get_field(state, this, it->second);
      }
    }

    if(CompactLookupTable* tbl = try_as<CompactLookupTable>(ivars_)) {
      return tbl->fetch(state, sym);
    } else if(LookupTable* tbl = try_as<LookupTable>(ivars_)) {
      return tbl->fetch(state, sym);
    }

    return Qnil;
  }

  /*
   * Returns a LookupTable or a CompactLookupTable.  Below a certain number of
   * instance variables a CompactTable is used to save memory.  See
   * Object::get_ivar for how to fetch an item out of get_ivars depending upon
   * storage type.
   */
  Object* Object::get_ivars(STATE) {
    if(!reference_p()) {
      LookupTable* tbl = try_as<LookupTable>(G(external_ivars)->fetch(state, this));

      if(tbl) return tbl;
      return Qnil;
    }

    return ivars_;
  }

  object_type Object::get_type() const {
    if(reference_p()) return type_id();
    if(fixnum_p()) return FixnumType;
    if(symbol_p()) return SymbolType;
    if(nil_p()) return NilType;
    if(true_p()) return TrueType;
    if(false_p()) return FalseType;
    return ObjectType;
  }

  hashval Object::hash(STATE) {
    if(!reference_p()) {
      if(Fixnum* fix = try_as<Fixnum>(this)) {
        return fix->to_native();
      } else if(Symbol* sym = try_as<Symbol>(this)) {
        return sym->index();
      } else {
        return (native_int)this;
      }
    } else {
      if(String* string = try_as<String>(this)) {
        return string->hash_string(state);
      } else if(Bignum* bignum = try_as<Bignum>(this)) {
        return bignum->hash_bignum(state);
      } else if(Float* flt = try_as<Float>(this)) {
        return String::hash_str((unsigned char *)(&(flt->val)), sizeof(double));
      } else {
        return id(state)->to_native();
      }
    }
  }

  Integer* Object::hash_prim(STATE) {
    return Integer::from(state, hash(state));
  }

  Integer* Object::id(STATE) {
    if(reference_p()) {
      Object* id = get_ivar(state, G(sym_object_id));

      /* Lazy allocate object's ids, since most don't need them. */
      if(id->nil_p()) {
        /* All references have an even object_id. last_object_id starts out at 0
         * but we don't want to use 0 as an object_id, so we just add before using */
        id = Fixnum::from(state->om->last_object_id += 2);
        set_ivar(state, G(sym_object_id), id);
      }

      return as<Integer>(id);
    } else {
      /* All non-references have an odd object_id */
      return Fixnum::from(((uintptr_t)this << 1) | 1);
    }
  }

  void Object::infect(STATE, Object* other) {
    if(this->tainted_p(state) == Qtrue) {
      other->taint(state);
    }
  }

  bool Object::kind_of_p(STATE, Object* module) {
    Module* found = NULL;

    if(!reference_p()) {
      found = state->globals.special_classes[((uintptr_t)this) & SPECIAL_CLASS_MASK].get();
    } else {
      found = try_as<Module>(klass_);
    }

    while(found) {
      if(found == module) return true;

      if(IncludedModule* im = try_as<IncludedModule>(found)) {
        if(im->module() == module) return true;
      }

      found = try_as<Module>(found->superclass());
    }

    return false;
  }

  Object* Object::kind_of_prim(STATE, Module* klass) {
    return kind_of_p(state, klass) ? Qtrue : Qfalse;
  }

  Class* Object::metaclass(STATE) {
    if(reference_p()) {
      if(kind_of<MetaClass>(klass_)) {
        return as<MetaClass>(klass_);
      }
      return MetaClass::attach(state, this);
    }

    return class_object(state);
  }

  Object* Object::send(STATE, CallFrame* caller, Symbol* name, Array* ary,
      Object* block, bool allow_private) {
    LookupData lookup(this, this->lookup_begin(state), allow_private);
    Dispatch dis(name);

    Arguments args(ary);
    args.set_block(block);
    args.set_recv(this);

    return dis.send(state, caller, lookup, args);
  }

  Object* Object::send(STATE, CallFrame* caller, Symbol* name, bool allow_private) {
    LookupData lookup(this, this->lookup_begin(state), allow_private);
    Dispatch dis(name);

    Arguments args;
    args.set_block(Qnil);
    args.set_recv(this);

    return dis.send(state, caller, lookup, args);
  }

  Object* Object::send_prim(STATE, Executable* exec, CallFrame* call_frame, Dispatch& msg,
                            Arguments& args) {
    Object* meth = args.shift(state);
    Symbol* sym = try_as<Symbol>(meth);

    if(!sym) {
      sym = as<String>(meth)->to_sym(state);
    }

    Dispatch dis(sym);
    LookupData lookup(this, this->lookup_begin(state), true);

    return dis.send(state, call_frame, lookup, args);
  }

  void Object::set_field(STATE, size_t index, Object* val) {
    type_info(state)->set_field(state, this, index, val);
  }

  Object* Object::set_ivar(STATE, Symbol* sym, Object* val) {
    LookupTable* tbl;

    /* Implements the external ivars table for objects that don't
       have their own space for ivars. */
    if(!reference_p()) {
      tbl = try_as<LookupTable>(G(external_ivars)->fetch(state, this));

      if(!tbl) {
        tbl = LookupTable::create(state);
        G(external_ivars)->store(state, this, tbl);
      }
      tbl->store(state, sym, val);
      return val;
    }

    /* We might be trying to access a field, so check there first. */
    TypeInfo* ti = state->om->find_type_info(this);
    if(ti) {
      TypeInfo::Slots::iterator it = ti->slots.find(sym->index());
      if(it != ti->slots.end()) {
        ti->set_field(state, this, it->second, val);
        return val;
      }
    }

    /* Lazy creation of a lookuptable to store instance variables. */
    if(ivars_->nil_p()) {
      CompactLookupTable* tbl = CompactLookupTable::create(state);
      ivars(state, tbl);
      tbl->store(state, sym, val);
      return val;
    }

    if(CompactLookupTable* tbl = try_as<CompactLookupTable>(ivars_)) {
      if(tbl->store(state, sym, val) == Qtrue) {
        return val;
      }

      /* No more room in the CompactLookupTable. */
      ivars(state, tbl->to_lookuptable(state));
    }

    try_as<LookupTable>(ivars_)->store(state, sym, val);
    return val;
  }

  Object* Object::del_ivar(STATE, Symbol* sym) {
    LookupTable* tbl;

    /* Implements the external ivars table for objects that don't
       have their own space for ivars. */
    if(!reference_p()) {
      tbl = try_as<LookupTable>(G(external_ivars)->fetch(state, this));

      if(tbl) tbl->remove(state, sym);
      return this;
    }

    /* We might be trying to access a field, so check there first. */
    TypeInfo* ti = state->om->find_type_info(this);
    if(ti) {
      TypeInfo::Slots::iterator it = ti->slots.find(sym->index());
      // Can't remove a slot, so just bail.
      if(it != ti->slots.end()) return this;
    }

    /* No ivars, we're done! */
    if(ivars_->nil_p()) return this;

    if(CompactLookupTable* tbl = try_as<CompactLookupTable>(ivars_)) {
      tbl->remove(state, sym);
    } else if(LookupTable* tbl = try_as<LookupTable>(ivars_)) {
      tbl->remove(state, sym);
    }
    return this;
  }

  String* Object::to_s(STATE, bool address) {
    std::stringstream name;

    name << "#<";
    if(Module* mod = try_as<Module>(this)) {
      if(mod->name()->nil_p()) {
        name << "Class";
      } else {
        name << mod->name()->c_str(state);
      }
      name << "(" << this->class_object(state)->name()->c_str(state) << ")";
    } else {
      if(this->class_object(state)->name()->nil_p()) {
        name << "Object";
      } else {
        name << this->class_object(state)->name()->c_str(state);
      }
    }

    name << ":";
    if(address) {
      name << reinterpret_cast<void*>(this);
    } else {
      name << "0x" << std::hex << this->id(state)->to_native();
    }
    name << ">";

    return String::create(state, name.str().c_str());
  }

  Object* Object::show(STATE) {
    return show(state, 0);
  }

  Object* Object::show(STATE, int level) {
    type_info(state)->show(state, this, level);
    return Qnil;
  }

  Object* Object::show_simple(STATE) {
    return show_simple(state, 0);
  }

  Object* Object::show_simple(STATE, int level) {
    type_info(state)->show_simple(state, this, level);
    return Qnil;
  }

  Object* Object::taint(STATE) {
    if(reference_p()) {
      set_ivar(state, state->symbol("tainted"), Qtrue);
    }
    return this;
  }

  Object* Object::tainted_p(STATE) {
    if(reference_p()) {
      Object* b = get_ivar(state, state->symbol("tainted"));
      if(b->nil_p()) return Qfalse;
      return Qtrue;
    } else {
      return Qfalse;
    }
  }

  TypeInfo* Object::type_info(STATE) const {
    return state->om->type_info[get_type()];
  }

  Object* Object::untaint(STATE) {
    if(reference_p()) {
      del_ivar(state, state->symbol("tainted"));
    }
    return this;
  }

  /**
   *  We use void* as the type for obj to work around C++'s type system
   *  that requires full definitions of classes to be present for it
   *  figure out if you can properly pass an object (the superclass
   *  has to be known).
   *
   *  If we have Object* obj here, then we either have to cast to call
   *  write_barrier (which means we lose the ability to have type specific
   *  write_barrier versions, which we do), or we have to include
   *  every header up front. We opt for the former.
   */
  void Object::write_barrier(STATE, void* obj) {
    state->om->write_barrier(this, reinterpret_cast<Object*>(obj));
  }

}
