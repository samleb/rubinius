#include "builtin/class.hpp"
#include "builtin/module.hpp"
#include "builtin/symbol.hpp"

#include "helpers.hpp"

#include "capi/capi.hpp"
#include "capi/ruby.h"

using namespace rubinius;
using namespace rubinius::capi;

extern "C" {
  VALUE rb_class_new_instance(int arg_count, VALUE* args, VALUE class_handle) {
    return rb_funcall2(class_handle, rb_intern("new"), arg_count, args);
  }

  VALUE rb_class_of(VALUE object_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();
    Class* class_object = env->get_object(object_handle)->class_object(env->state());
    return env->get_handle_global(class_object);
  }

  VALUE rb_class_name(VALUE class_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();
    Class* class_object = c_as<Class>(env->get_object(class_handle));
    return env->get_handle(class_object->name()->to_str(env->state()));
  }

  char* rb_class2name(VALUE class_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();
    Class* class_object = c_as<Class>(env->get_object(class_handle));

    return ::strdup(class_object->name()->c_str(env->state()));
  }

  VALUE rb_path2class(const char* name) {
    return rb_funcall(rb_mKernel, rb_intern("const_lookup"), 1, rb_str_new2(name));
  }

  VALUE rb_cv_get(VALUE module_handle, const char* name) {
    return rb_cvar_get(module_handle, rb_intern(name));
  }

  VALUE rb_cv_set(VALUE module_handle, const char* name, VALUE value) {
    return rb_cvar_set(module_handle, rb_intern(name), value, 0);
  }

  VALUE rb_cvar_defined(VALUE module_handle, ID name) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    return rb_funcall(module_handle, rb_intern("class_variable_defined?"),
                      1,
                      env->get_handle(prefixed_by("@@", name)));
  }

  VALUE rb_cvar_get(VALUE module_handle, ID name) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    return rb_funcall(module_handle, rb_intern("class_variable_set"),
                      1,
                      env->get_handle(prefixed_by("@@", name)));
  }

  VALUE rb_cvar_set(VALUE module_handle, ID name, VALUE value, int unused) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    return rb_funcall(module_handle, rb_intern("class_variable_set"),
                      2,
                      env->get_handle(prefixed_by("@@", name)),
                      value);
  }

  VALUE rb_define_class(const char* name, VALUE superclass_handle) {
    return rb_define_class_under(rb_cObject, name, superclass_handle);
  }

  /** @note   Shares code with rb_define_module_under, change there too. --rue */
  VALUE rb_define_class_under(VALUE parent_handle, const char* name, VALUE superclass_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Module* parent = c_as<Module>(env->get_object(parent_handle));
    Class* superclass = c_as<Class>(env->get_object(superclass_handle));
    Symbol* constant = env->state()->symbol(name);

    bool created = false;
    Class* cls = rubinius::Helpers::open_class(env->state(),
        env->current_call_frame(), parent, superclass, constant, &created);

    return env->get_handle_global(cls);
  }

  /** @todo   Should this be a global handle? Surely not.. --rue */
  VALUE rb_singleton_class(VALUE object_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Class* metaclass = env->get_object(object_handle)->metaclass(env->state());
    return env->get_handle(metaclass);
  }
}
