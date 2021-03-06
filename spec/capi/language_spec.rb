require File.dirname(__FILE__) + '/spec_helper'

load_extension('language')

describe "CApiBlockSpecs" do
  before :each do
    @f = CApiBlockSpecs.new
  end
  
  it "identifies blocks using rb_block_given_p" do
    @f.block_given?.should == false
    (@f.block_given? { puts "FOO" } ).should == true
  end

  it "correctly utilizes rb_yield" do
    (@f.do_yield { |x| x+1 }).should == 6
    lambda { @f.do_yield }.should raise_error(LocalJumpError)
  end

end

describe "CApiCallSuperSpecs" do
  before :each do
    @s = CApiCallSuperSpecs.new
  end

  class TheSuper
    # Set an instance variable to ensure we are doing rb_call_super on this
    # instance.
    def initialize(value)
      @value = value.to_i
    end

    def a_method
      return @value
    end
  end

  class TheSub < TheSuper
  end

  it "rb_call_super should call the method in the superclass" do
    instance = TheSub.new(1)
    instance.a_method.should == 1
    @s.override_method(TheSub) # Override the method, use rb_call_super
    instance.a_method.should == 2
  end

  it "should create a subclass and create a new instance" do
    @s.test_subclass_and_instantiation().should == true
    (instance = ModuleTest::ClassSuper.new()).should_not == nil
    (instance = ModuleTest::ClassSub.new()).should_not == nil
  end
end
