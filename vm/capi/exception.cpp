#include "capi/capi.hpp"
#include "capi/ruby.h"

extern "C" {
  VALUE rb_exc_new(VALUE etype, const char *ptr, long len) {
    return rb_funcall(etype, rb_intern("new"), 1, rb_str_new(ptr, len));
  }

  VALUE rb_exc_new2(VALUE etype, const char *s) {
    return rb_exc_new(etype, s, strlen(s));
  }

  VALUE rb_exc_new3(VALUE etype, VALUE str) {
    StringValue(str);
    return rb_funcall(etype, rb_intern("new"), 1, str);
  }
}
