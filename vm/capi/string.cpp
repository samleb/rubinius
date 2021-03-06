#include "builtin/bytearray.hpp"
#include "builtin/fixnum.hpp"
#include "builtin/integer.hpp"
#include "builtin/nativemethod.hpp"
#include "builtin/object.hpp"
#include "builtin/string.hpp"

#include "capi/capi.hpp"
#include "capi/ruby.h"

using namespace rubinius;
using namespace rubinius::capi;

namespace rubinius {
  namespace capi {
    // internal helper method
    static void flush_string(STATE, String* string, struct RString* str) {
      if(string->size() != str->len) {
        ByteArray* ba = ByteArray::create(state, str->len+1);
        string->data(state, ba);
        string->num_bytes(state, Fixnum::from(str->len));
        string->characters(state, Fixnum::from(str->len));
        string->hash_value(state, reinterpret_cast<Integer*>(RBX_Qnil));
      }
      std::memcpy(string->byte_address(), str->ptr, str->len);
      string->byte_address()[str->len] = 0;
    }

    String* capi_get_string(NativeMethodEnvironment* env, VALUE str_handle) {
      if(!env) env = NativeMethodEnvironment::get();

      String* string = c_as<String>(env->get_object(str_handle));

      CApiStructs& strings = env->strings();
      CApiStructs::iterator iter = strings.find(str_handle);
      if(iter != strings.end()) {
        flush_string(env->state(), string, (struct RString*)iter->second);
      }

      return string;
    }

    void capi_rstring_flush(NativeMethodEnvironment* env,
        CApiStructs& strings, bool release_memory) {
      String* string;
      struct RString* str = 0;

      for(CApiStructs::iterator iter = strings.begin();
          iter != strings.end();
          iter++) {
        string = c_as<String>(env->get_object(iter->first));
        str = (struct RString*)iter->second;

        flush_string(env->state(), string, str);

        if(release_memory) {
          delete[] str->dmwmb;
          delete str;
        }
      }
    }

    // internal helper method
    static void update_string(STATE, String* string, struct RString* str) {
      size_t size = string->size();

      if(str->len != size) {
        delete[] str->dmwmb;
        str->dmwmb = str->ptr = new char[size+1];
        str->aux.capa = str->len = size;
      }

      std::memcpy(str->ptr, string->byte_address(), size);
      str->ptr[size] = 0;
    }

    void capi_update_string(NativeMethodEnvironment* env, VALUE str_handle) {
      if(!env) env = NativeMethodEnvironment::get();

      CApiStructs& strings = env->strings();
      CApiStructs::iterator iter = strings.find(str_handle);
      if(iter != strings.end()) {
        String* string = c_as<String>(env->get_object(str_handle));
        update_string(env->state(), string, (struct RString*)iter->second);
      }
    }

    void capi_rstring_update(NativeMethodEnvironment* env, CApiStructs& strings) {
      for(CApiStructs::iterator iter = strings.begin();
          iter != strings.end();
          iter++) {
        String* string = c_as<String>(env->get_object(iter->first));
        update_string(env->state(), string, (struct RString*)iter->second);
      }
    }
  }
}

extern "C" {
  struct RString* capi_rstring_struct(VALUE str_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    CApiStructs& strings = env->strings();
    CApiStructs::iterator iter = strings.find(str_handle);
    if(iter != strings.end()) {
      return (struct RString*)iter->second;
    }

    String* string = c_as<String>(env->get_object(str_handle));
    string->unshare(env->state());
    size_t size = string->size();

    struct RString* str = new struct RString;
    char* ptr = new char[size+1];
    std::memcpy(ptr, string->byte_address(), size);
    ptr[size] = 0;

    str->dmwmb = str->ptr = ptr;
    str->aux.capa = str->len = size;
    str->aux.shared = Qfalse;

    strings[str_handle] = str;

    return str;
  }

  VALUE rb_String(VALUE object_handle) {
    return rb_convert_type(object_handle, 0, "String", "to_s");
  }

  VALUE rb_str_append(VALUE self_handle, VALUE other_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    String* self = capi_get_string(env, self_handle);
    self->append(env->state(), capi_get_string(env, other_handle));
    capi_update_string(env, self_handle);

    return self_handle;
  }

  VALUE rb_str_buf_new(long capacity) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    String* str = String::create(env->state(), Fixnum::from(capacity));
    str->num_bytes(env->state(), Fixnum::from(0));

    return env->get_handle(str);
  }

  VALUE rb_str_buf_append(VALUE self_handle, VALUE other_handle) {
    return rb_str_append(self_handle, other_handle);
  }

  VALUE rb_str_buf_cat(VALUE self_handle, const char* other, size_t size) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    String* string = capi_get_string(env, self_handle);
    string->append(env->state(), other, size);
    capi_update_string(env, self_handle);

    return self_handle;
  }

  VALUE rb_str_buf_cat2(VALUE self_handle, const char* other) {
    return rb_str_buf_cat(self_handle, other, std::strlen(other));
  }

  VALUE rb_str_cat(VALUE self_handle, const char* other, size_t length) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    String* self = capi_get_string(env, self_handle);
    self->append(env->state(), other, length);
    capi_update_string(env, self_handle);

    return self_handle;
  }

  VALUE rb_str_cat2(VALUE self_handle, const char* other) {
    return rb_str_cat(self_handle, other, std::strlen(other));
  }

  int rb_str_cmp(VALUE self_handle, VALUE other_handle) {
    return NUM2INT(rb_funcall(self_handle, rb_intern("<=>"), 1, other_handle));
  }

  VALUE rb_str_concat(VALUE self_handle, VALUE other_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    Object* other = env->get_object(other_handle);

    /* Could be a character code. Only up to 256 supported. */
    if(Fixnum* character = try_as<Fixnum>(other)) {
      char byte = character->to_uint();

      return rb_str_cat(self_handle, &byte, 1);
    }

    return rb_str_append(self_handle, other_handle);
  }

  VALUE rb_str_dup(VALUE self_handle) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    String* self = capi_get_string(env, self_handle);
    return env->get_handle(self->string_dup(env->state()));
  }

  VALUE rb_str_new(const char* string, size_t length) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    return env->get_handle(String::create(env->state(), string, length));
  }

  VALUE rb_str_new2(const char* string) {
    if(string == NULL) {
      rb_raise(rb_eArgError, "NULL pointer given");
    }

    return rb_str_new(string, std::strlen(string));
  }

  VALUE rb_str_plus(VALUE self_handle, VALUE other_handle) {
    return rb_str_append(rb_str_dup(self_handle), other_handle);
  }

  VALUE rb_str_split(VALUE self_handle, const char* separator) {
    return rb_funcall(self_handle, rb_intern("split"), 1, rb_str_new2(separator));
  }

  VALUE rb_str_substr(VALUE self_handle, size_t starting_index, size_t length) {
    return rb_funcall(self_handle, rb_intern("slice"), 2,
                      LONG2NUM(starting_index), LONG2NUM(length) );
  }

  VALUE rb_str_times(VALUE self_handle, VALUE times) {
    return rb_funcall(self_handle, rb_intern("*"), 1, times);
  }

  VALUE rb_str2inum(VALUE self_handle, int base) {
    return rb_funcall(self_handle, rb_intern("to_i"), 1, INT2NUM(base));
  }

  VALUE rb_str_to_str(VALUE object_handle) {
    return rb_convert_type(object_handle, 0, "String", "to_str");
  }

  VALUE rb_string_value(VALUE* object_variable) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    if(!kind_of<String>(env->get_object(*object_variable))) {
      *object_variable = rb_str_to_str(*object_variable);
    }

    return *object_variable;
  }

  char* rb_string_value_ptr(VALUE* object_variable) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    VALUE str = rb_string_value(object_variable);
    String* string = c_as<String>(env->get_object(str));

    return const_cast<char*>(string->c_str());
  }

  char* rb_string_value_cstr(VALUE* object_variable) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    VALUE str = rb_string_value(object_variable);
    String* string = capi_get_string(env, str);

    if(string->size() != strlen(string->c_str())) {
      rb_raise(rb_eArgError, "string contains NULL byte");
    }

    return const_cast<char*>(string->c_str());
  }

  VALUE rb_tainted_str_new2(const char* string) {
    if(string == NULL) {
      rb_raise(rb_eArgError, "NULL pointer given");
    }

    return rb_tainted_str_new(string, std::strlen(string));
  }

  VALUE rb_tainted_str_new(const char* string, long size) {
    if(string == NULL) {
      rb_raise(rb_eArgError, "NULL pointer given");
    }

    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    String* str = String::create(env->state(), string, size);
    str->taint(env->state());

    return env->get_handle(str);
  }
}
