#include "capi/capi.hpp"
#include "capi/ruby.h"

#include "builtin/float.hpp"

using namespace rubinius;
using namespace rubinius::capi;

extern "C" {
  const char *rb_id2name(ID sym) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    return reinterpret_cast<Symbol*>(sym)->c_str(env->state());
  }

  int rb_scan_args(int argc, const VALUE* argv, const char* spec, ...) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();

    int n, i = 0;
    const char *p = spec;
    VALUE *var;
    va_list vargs;

    va_start(vargs, spec);

    if (*p == '*') goto rest_arg;

    if (ISDIGIT(*p)) {
      n = *p - '0';
      if (n > argc)
        rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", argc, n);
      for (i=0; i<n; i++) {
        var = va_arg(vargs, VALUE*);
        if (var) *var = argv[i];
      }
      p++;
    }
    else {
      goto error;
    }

    if (ISDIGIT(*p)) {
      n = i + *p - '0';
      for (; i<n; i++) {
        var = va_arg(vargs, VALUE*);
        if (argc > i) {
          if (var) *var = argv[i];
        }
        else {
          if (var) *var = Qnil;
        }
      }
      p++;
    }

    if(*p == '*') {
      rest_arg:
      var = va_arg(vargs, VALUE*);
      if (argc > i) {
        if (var) *var = rb_ary_new4(argc-i, argv+i);
        i = argc;
      }
      else {
        if (var) *var = rb_ary_new();
      }
      p++;
    }

    if (*p == '&') {
      var = va_arg(vargs, VALUE*);
      *var = env->get_handle(env->block());
      p++;
    }
    va_end(vargs);

    if (*p != '\0') {
      goto error;
    }

    if (argc > i) {
      rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", argc, i);
    }

    return argc;

    error:
    rb_raise(rb_eFatal, "bad scan arg format: %s", spec);
    return 0;
  }

  int rb_safe_level() {
    return FIX2INT(rb_gv_get("$SAFE"));
  }

  /** Some arbitrary high max for $SAFE. */
  static const int RBX_ARBITRARY_MAX_SAFE_LEVEL = 256;

  /**
   *  @todo   Error out if level is invalid?
   *          Or should it fail silently as
   *          a security measure? --rue
   */
  void rb_set_safe_level(int new_level) {
    int safe_level = rb_safe_level();

    if(new_level > safe_level && new_level < RBX_ARBITRARY_MAX_SAFE_LEVEL) {
      rb_gv_set("$SAFE", INT2FIX(new_level));
    }
  }

  void rb_secure(int level) {
    if (level <= rb_safe_level())
    {
      rb_raise(rb_eSecurityError, "Insecure operation at level %d", rb_safe_level());
    }
  }

  VALUE rb_marshal_load(VALUE string) {
    return rb_funcall(rb_path2class("Marshal"), rb_intern("load"), 1, string);
  }

  VALUE rb_float_new(double val) {
    NativeMethodEnvironment* env = NativeMethodEnvironment::get();
    return env->get_handle(Float::create(env->state(), val));
  }
}
