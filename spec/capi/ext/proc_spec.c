#include <ruby.h>
#include <string.h>

VALUE concat_func(VALUE args) {
  int i;
  char buffer[500] = {0};
  for(i = 0; i < RARRAY(args)->len; ++i) {
    VALUE v = RARRAY(args)->ptr[i];
    strcat(buffer, StringValuePtr(v));
    strcat(buffer, "_");
  }
  buffer[strlen(buffer) - 1] = 0;
  return rb_str_new2(buffer);

}

VALUE sp_underline_concat_proc(VALUE self) {
  return rb_proc_new(concat_func, Qnil);
}

void Init_proc_spec() {
  VALUE cls;
  cls = rb_define_class("CApiProcSpecs", rb_cObject);

  rb_define_method(cls, "underline_concat_proc", sp_underline_concat_proc, 0);
}
