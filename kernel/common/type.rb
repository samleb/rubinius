##
# Namespace for coercion functions between various ruby objects.

module Type

  ##
  # Returns an object of given class. If given object already is one, it is
  # returned. Otherwise tries obj.meth and returns the result if it is of the
  # right kind. TypeErrors are raised if the conversion method fails or the
  # conversion result is wrong.
  #
  # Uses Type.obj_kind_of to bypass type check overrides.
  #
  # Equivalent to MRI's rb_convert_type().

  def self.coerce_to(obj, cls, meth)
    return obj if self.obj_kind_of?(obj, cls)

    begin
      ret = obj.__send__(meth)
    rescue Exception => e
      raise TypeError, "Coercion error: #{obj.inspect}.#{meth} => #{cls} failed:\n" \
                       "(#{e.message})"
    end

    return ret if self.obj_kind_of?(ret, cls)

    raise TypeError, "Coercion error: obj.#{meth} did NOT return a #{cls} (was #{ret.class})"
  end

  def self.coerce_to_symbol(obj)
    if obj.kind_of? Fixnum
      raise ArgumentError, "Fixnums (#{obj}) cannot be used as symbols"
    end
    obj = obj.to_str if obj.respond_to?(:to_str)

    coerce_to(obj, Symbol, :to_sym)
  end
end
