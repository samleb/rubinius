# depends on: class.rb numeric.rb regexp.rb string.rb

def BigDecimal(string, _precs=0)
  BigDecimal.new(string, _precs)
end

class BigDecimal < Numeric
  # See stdlib/ext/bigdecimal for MatzRuby implementation.
  
  #############
  # Constants #
  #############
  
  SIGN_POSITIVE_ZERO = 1
  SIGN_NEGATIVE_ZERO = -1
  SIGN_POSITIVE_FINITE = 2
  SIGN_NEGATIVE_FINITE = -2
  SIGN_POSITIVE_INFINITE = 3
  SIGN_NEGATIVE_INFINITE = -3
  SIGN_NaN = 0 # is this correct?

  # call-seq:
  #   BigDecimal("3.14159")   => big_decimal
  #   BigDecimal("3.14159", 10)   => big_decimal
  def initialize(_val, _precs=0)
    # set up defaults
    @sign = '+'
    @digits = 0 # decimal point is assumed at beginning; exp is assigned on this basis
    @exp = 0
    @special = nil # 'n' for NaN, 'i' for Infinity, nil otherwise

    v = _val.strip
    if v == "NaN"
      @special = 'n'
      @precs = 0
    elsif v =~ /[-+]?Infinity/
      @special = 'i'
      @sign = '-' if v =~ /-/
      @precs = 0
    else
      v = _val.gsub('_', '')
      m = /^\s*(([-+]?)(\d*)(?:\.(\d*))?(?:[EeDd]([-+]?\d+))?).*$/.match(v)
      if !m.nil?
        @sign = m[2] unless m[2].to_s.empty?
        int = m[3].to_s.gsub(/^0*/, '')
        frac = m[4].to_s
        fraczeros = /^0*/.match(frac)[0]
        @exp = m[5].to_i + int.length
        if int.to_i == 0 
          @exp -= fraczeros.length
        end
        @digits = (int + frac).gsub(/0*$/, '').to_i
      end
      @precs = [v.length, _precs].max
    end
  end

  # As for Float.finite? .
  # call-seq:
  #   BigDecimal.new("Infinity").finite?  => false
  #   BigDecimal.new("NaN").finite?  => true
  def finite?
    @special != 'i'
  end

  # As for Float.nan? .
  # call-seq:
  #   BigDecimal.new("NaN").nan?  => true
  #   BigDecimal.new("123").nan?  => false
  def nan?
    @special == 'n'
  end
  
  # True if positive or negative zero; false otherwise.
  # call-seq:
  #   BigDecimal.new("0").zero?   =>true
  #   BigDecimal.new("-0").zero?  =>true
  def zero?
    @digits.to_i == 0 and !self.nan? and self.finite?
  end

  def precs
    if !self.finite? or self.nan?
      sigfigs = 0
    else
      sigfigs = @digits.to_s.length
    end
    [sigfigs, @precs]
  end

  def to_s
    radix = '.'
    e = 'E'
    nan = 'NaN'
    infinity = 'Infinity'

    if self.nan?
      return nan
    end

    if @sign == '+'
      str = ''
    else
      str = '-'
    end

    if self.finite?
      str << "0#{radix}"
      str << @digits.to_s
      if @exp != 0
        str << e
        str << @exp.to_s
      end
    else
      str << infinity
    end
    return str
  end
  
  def inspect
    str = '#<BigDecimal:'
    str << [nil, self.to_s, "#{precs[0]}(#{precs[1]})"].join(',')
    str << '>'
    return str
  end

  def coerce(other)
    Ruby.primitive :numeric_coerce
    [BigDecimal(other.to_s), self]
  end

  #########################
  # Arithmetic operations #
  #########################

  # These are stubbed out until we implement them so that their respective specfiles don't crash.

  def +(other)
    if self.nan? or other.nan?
      return BigDecimal('NaN')
    end
  end

  def -(other)
  end

  def quo(other)
  end
  alias / quo

  def remainder(other)
  end
  alias % remainder

  def >=(other)
  end

  def <=(other)
  end

  # This will need to be refactored
  def <=>(other)
    if other != 0 or self.nan?
      raise
    elsif self.finite? and @int == '0' and @frac == '0'
      return 0
    else
      case @sign
      when '+'
        return 1
      when '-'
        return -1
      end
    end
  end

  def <(other)
    return self.<=>(other) == -1
  end

  def eql?(other)
    if self.nan?
      return false
    elsif other.respond_to?(:nan?) and other.nan?
      return false
    elsif self.zero? and other.respond_to?(:zero?)
      return other.zero?
    elsif self.to_s == other.to_s
      return true
    elsif !other.kind_of?(BigDecimal)
      return self == BigDecimal(other.to_s)
    else
      return false
    end
  end
  alias == eql?

  def >(other)
    return self.<=>(other) == 1
  end
  
  ####################
  # Other operations #
  ####################
  
  # I'm trying to keep these in alphabetical order unless a good reason develops to do otherwise.
  
  def abs
    if self.nan? or @sign == '+'
      return self
    else
      s = self.to_s.sub(/^-/, '') # strip minus sign
      BigDecimal(s)
    end
  end
  
  def sign
    if self.nan?
      SIGN_NaN
    elsif self.zero?
      @sign == '+' ? SIGN_POSITIVE_ZERO : SIGN_NEGATIVE_ZERO
    elsif self.finite?
      @sign == '+' ? SIGN_POSITIVE_FINITE : SIGN_NEGATIVE_FINITE
    else # infinite
      @sign == '+' ? SIGN_POSITIVE_INFINITE : SIGN_NEGATIVE_INFINITE
    end
  end
end