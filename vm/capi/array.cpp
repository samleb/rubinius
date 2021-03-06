#include "builtin/array.hpp"
#include "builtin/fixnum.hpp"
#include "builtin/object.hpp"

#include "arguments.hpp"
#include "dispatch.hpp"

#include "capi/capi.hpp"
#include "capi/ruby.h"

using namespace rubinius;
using namespace rubinius::capi;

namespace rubinius {
  namespace capi {
    // internal helper method
    static void flush_array(NativeMethodEnvironment* env,
        Array* array, struct RArray* ary) {
      size_t size = array->size();

      if(size != ary->len) {
        Tuple* tuple = Tuple::create(env->state(), ary->len);
        array->tuple(env->state(), tuple);
        array->start(env->state(), Fixnum::from(0));
        array->total(env->state(), Fixnum::from(ary->len));
      }

      for(size_t i = 0; i < size; i++) {
        array->set(env->state(), i, env->get_object(ary->ptr[i]));
      }
    }

    Array* capi_get_array(NativeMethodEnvironment* env, VALUE ary_handle) {
      if(!env) env = NativeMethodEnvironment::get();

      Array* array = c_as<Array>(env->get_object(ary_handle));

      CApiStructs& arrays = env->arrays();
      CApiStructs::iterator iter = arrays.find(ary_handle);
      if(iter != arrays.end()) {
        flush_array(env, array, (struct RArray*)iter->second);
      }

      return array;
    }

    void capi_rarray_flush(NativeMethodEnvironment* env,
        CApiStructs& arrays, bool release_memory) {
      Array* array;
      struct RArray* ary = 0;

      for(CApiStructs::iterator iter = arrays.begin();
          iter != arrays.end();
          iter++) {
        array = c_as<Array>(env->get_object(iter->first));
        ary = (struct RArray*)iter->second;

        flush_array(env, array, ary);

        if(release_memory) {
          delete[] ary->dmwmb;
          delete ary;
        }
      }
    }

    // internal helper method
    static void update_array(NativeMethodEnvironment* env, Array* array, struct RArray* ary) {
      size_t size = array->size();

      if(ary->len != size) {
        delete[] ary->dmwmb;
        ary->dmwmb = ary->ptr = new VALUE[size];
        ary->aux.capa = ary->len = size;
      }

      for(size_t i = 0; i < size; i++) {
        ary->ptr[i] = env->get_handle(array->get(env->state(), i));
      }
    }

    void capi_update_array(NativeMethodEnvironment* env, VALUE ary_handle) {
      if(!env) env = NativeMethodEnvironment::get();

      CApiStructs& arrays = env->arrays();
      CApiStructs::iterator iter = arrays.find(ary_handle);
      if(iter != arrays.end()) {
        Array* array = c_as<Array>(env->get_object(ary_handle));
        update_array(env, array, (struct RArray*)iter->second);
      }
    }

    void capi_rarray_update(NativeMethodEnvironment* env, CApiStructs& arrays) {
      for(CApiStructs::iterator iter = arrays.begin();
          iter != arrays.end();
          iter++) {
        Array* array = c_as<Array>(env->get_object(iter->first));
        update_array(env, array, (struct RArray*)iter->second);
      }
    }
  }
}

extern "C" {
  struct RArray* capi_rarray_struct(VALUE ary_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    CApiStructs& arrays = env->arrays();
    CApiStructs::iterator iter = arrays.find(ary_handle);
    if(iter != arrays.end()) {
      return (struct RArray*)iter->second;
    }

    Array* array = c_as<Array>(env->get_object(ary_handle));
    size_t size = array->size();

    struct RArray* ary = new struct RArray;
    VALUE* ptr = new VALUE[size];
    for(size_t i = 0; i < size; i++) {
      ptr[i] = env->get_handle(array->get(env->state(), i));
    }

    ary->dmwmb = ary->ptr = ptr;
    ary->aux.capa = ary->len = size;
    ary->aux.shared = Qfalse;

    arrays[ary_handle] = ary;

    return ary;
  }

  VALUE rb_Array(VALUE obj_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Object* obj = env->get_object(obj_handle);

    if (kind_of<Array>(obj)) {
      return obj_handle;
    }

    Array* array = Array::create(env->state(), 1);
    array->set(env->state(), 0, obj);

    return env->get_handle(array);
  }

  VALUE rb_ary_clear(VALUE self_handle) {
    return rb_funcall2(self_handle, rb_intern("clear"), 0, NULL);
  }

  VALUE rb_ary_dup(VALUE self_handle) {
    return rb_funcall2(self_handle, rb_intern("dup"), 0, NULL);
  }

  /* @todo Check 64-bit? */
  VALUE rb_ary_entry(VALUE self_handle, int index) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Array* self = capi_get_array(env, self_handle);
    return env->get_handle(self->aref(env->state(), Fixnum::from(index)));
  }

  VALUE rb_ary_join(VALUE self_handle, VALUE separator_handle) {
    return rb_funcall(self_handle, rb_intern("join"), 1, separator_handle);
  }

  /** By default, Arrays have space for 16 elements. */
  static const unsigned long cCApiArrayDefaultCapacity = 16;

  VALUE rb_ary_new() {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Array* array = Array::create(env->state(), cCApiArrayDefaultCapacity);
    return env->get_handle(array);
  }

  VALUE rb_ary_new2(unsigned long length) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Array* array = Array::create(env->state(), length);

    return env->get_handle(array);
  }

  VALUE rb_ary_new4(unsigned long length, const VALUE* object_handles) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Array* array = Array::create(env->state(), length);
    array->start(env->state(), Fixnum::from(0));
    array->total(env->state(), Fixnum::from(length));

    if (object_handles) {
      for(std::size_t i = 0; i < length; ++i) {
        // @todo determine if we need to check these objects for whether
        // they are arrays and flush any caches
        Object* object = env->get_object(object_handles[i]);
        array->set(env->state(), i, object);
      }
    }

    return env->get_handle(array);
  }

  VALUE rb_ary_pop(VALUE self_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Array* self = capi_get_array(env, self_handle);
    Object* obj = self->pop(env->state());
    capi_update_array(env, self_handle);

    return env->get_handle(obj);
  }

  VALUE rb_ary_push(VALUE self_handle, VALUE object_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Array* self = capi_get_array(env, self_handle);
    self->append(env->state(), env->get_object(object_handle));
    capi_update_array(env, self_handle);

    return self_handle;
  }

  VALUE rb_ary_reverse(VALUE self_handle) {
    return rb_funcall2(self_handle, rb_intern("reverse"), 0, NULL);
  }

  VALUE rb_ary_shift(VALUE self_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Array* self = capi_get_array(env, self_handle);
    Object* obj = self->shift(env->state());
    capi_update_array(env, self_handle);

    return env->get_handle(obj);
  }

  size_t rb_ary_size(VALUE self_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Array* self = capi_get_array(env, self_handle);

    return self->size();
  }

  void rb_ary_store(VALUE self_handle, long int index, VALUE object_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Array* self = capi_get_array(env, self_handle);
    size_t total = self->size();

    if(index < 0) {
      index += total;
    }

    if(index < 0) {
      std::ostringstream error;
      error << "Index " << (index - total) << " out of range!";
      rb_raise(rb_eIndexError, error.str().c_str());
    }

    self->set(env->state(), index, env->get_object(object_handle));
    capi_update_array(env, self_handle);
  }

  VALUE rb_ary_unshift(VALUE self_handle, VALUE object_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Array* self = capi_get_array(env, self_handle);
    self->unshift(env->state(), env->get_object(object_handle));
    capi_update_array(env, self_handle);

    return self_handle;
  }

  VALUE rb_attr_get(VALUE obj_handle, ID attr_name) {
    return rb_ivar_get(obj_handle, attr_name);
  }
}
