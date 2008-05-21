#ifndef RBX_FLOAT_HPP
#define RBX_FLOAT_HPP

/* Begin borrowing from MRI 1.8.6 stable */
#if defined(__FreeBSD__) && __FreeBSD__ < 4
#include <floatingpoint.h>
#endif

#include <float.h>

namespace rubinius {
  class Float : public BuiltinType {
    public:
    const static size_t fields = 0;
    const static object_type type = FloatType;

    static bool is_a(OBJECT obj) {
      return obj->obj_type == FloatType;
    }

    double val;

    static Float* create(STATE, double val);
    static Float* coerce(STATE, OBJECT value);
    double to_double(STATE) { return val; }
    double to_double() { return val; }
    void into_string(STATE, char* buf, size_t sz);
    OBJECT compare(STATE, Float* other);

    Float* add(STATE, Float* other);
    Float* add(STATE, INTEGER other);
    Float* sub(STATE, Float* other);
    Float* sub(STATE, INTEGER other);
    Float* mul(STATE, Float* other);
    Float* mul(STATE, INTEGER other);
    Float* divide(STATE, Float* other);
    Float* divide(STATE, INTEGER other);
    Float* div(STATE, Float* other);
    Float* div(STATE, INTEGER other);
    Float* mod(STATE, Float* other);
    Float* mod(STATE, INTEGER other);
    Float* neg(STATE);

    static int radix()      { return FLT_RADIX; }
    static int rounds()     { return FLT_ROUNDS; }
    static double min()     { return DBL_MIN; }
    static double max()     { return DBL_MAX; }
    static int min_exp()    { return DBL_MIN_EXP; }
    static int max_exp()    { return DBL_MAX_EXP; }
    static int min_10_exp() { return DBL_MIN_10_EXP; }
    static int max_10_exp() { return DBL_MAX_10_EXP; }
    static int dig()        { return DBL_DIG; }
    static int mant_dig()   { return DBL_MANT_DIG; }
    static double epsilon() { return DBL_EPSILON; }

    class Info : public TypeInfo {
    public:
      Info(object_type type) : TypeInfo(type) { }
      virtual void mark(OBJECT t, ObjectMark& mark);
    };
  };
}

#endif