require File.dirname(__FILE__) + '/spec_helper'

load_extension('string')

describe "C-API String function" do
  before :each do
    @s = CApiStringSpecs.new
  end

  class ValidTostrTest
    def to_str
      "ruby"
    end
  end

  class InvalidTostrTest
    def to_str
      []
    end
  end

  describe "rb_str_new" do
    it "returns a new string object" do
      # Hardcoded to pass const char * = "hello"
      @s.rb_str_new.should == "hello"
    end
  end

  describe "rb_str_new2" do
    it "returns a new string object calling strlen on the passed C string" do
      # Hardcoded to pass const char * = "hello\0invisible"
      @s.rb_str_new2.should == "hello"
    end

    it "rb_str_new2 should raise ArgumentError if passed NULL" do
      lambda { @s.rb_str_new2_with_null }.should raise_error(ArgumentError)
    end
  end

  describe "rb_str_dup" do
    it "returns a copy of the string" do
      str1 = "hi"
      str2 = @s.rb_str_dup str1
      str1.should == str2
      str1.object_id.should_not == str2.object_id
    end
  end

  describe "rb_str_append" do
    it "appends a string to another string" do
      @s.rb_str_append("Hello", " Goodbye").should == "Hello Goodbye"
    end
  end

  describe "rb_str_plus" do
    it "returns a new string from concatenating two other strings" do
      @s.rb_str_plus("Hello", " Goodbye").should == "Hello Goodbye"
    end
  end

  describe "rb_str_buf_cat" do
    it "concatenates a C string to a ruby string" do
      @s.rb_str_buf_cat("Your house is on fire").should == "Your house is on fire?"
    end
  end

  describe "rb_str_cat" do
    it "concatenates a C string to ruby string" do
      @s.rb_str_cat("Your house is on fire").should == "Your house is on fire?"
    end
  end

  describe "rb_str_cat2" do
    it "concatenates a C string to a ruby string" do
      @s.rb_str_cat2("Your house is on fire").should == "Your house is on fire?"
    end
  end

  describe "rb_str_cmp" do
    it "returns 0 if two strings are identical" do
      @s.rb_str_cmp("ppp", "ppp").should == 0
    end

    it "returns -1 if the first string is shorter than the second" do
      @s.rb_str_cmp("xxx", "xxxx").should == -1
    end

    it "returns -1 if the first string is lexically less than the second" do
      @s.rb_str_cmp("xxx", "yyy").should == -1
    end

    it "returns 1 if the first string is longer than the second" do
      @s.rb_str_cmp("xxxx", "xxx").should == 1
    end

    it "returns 1 if the first string is lexically greater than the second" do
      @s.rb_str_cmp("yyy", "xxx").should == 1
    end
  end

  describe "rb_str_split" do
    it "splits strings over a splitter" do
      @s.rb_str_split("Hello,Goodbye").should == ["Hello", "Goodbye"]
    end
  end

  describe "rb_str2inum" do
    it "converts a string to a number given a base" do
      @s.rb_str2inum("10", 10).should == 10
      @s.rb_str2inum("A", 16).should == 10
    end
  end

  describe "rb_cstr2inum" do
    it "converts a C string to a number given a base" do
      @s.rb_cstr2inum("10", 10).should == 10
      @s.rb_cstr2inum("10", 16).should == 16
    end
  end

  describe "rb_str_substr" do
    it "returns a substring" do
      "hello".length.times do |time|
        @s.rb_str_substr("hello", 0, time + 1).should == "hello"[0..time]
      end
    end
  end

  describe "rb_str_to_str" do
    it "calls #to_str to coerce the value to a String" do
      @s.rb_str_to_str("foo").should == "foo"
      @s.rb_str_to_str(ValidTostrTest.new).should == "ruby"
    end

    it "raises a TypeError if coercion fails" do
      lambda { @s.rb_str_to_str(0) }.should raise_error(TypeError)
      lambda { @s.rb_str_to_str(InvalidTostrTest.new) }.should raise_error(TypeError)
    end
  end

  describe "RSTRING" do
    it "returns struct with a pointer to the string's contents" do
      str = "xyz"
      chars = []
      @s.RSTRING_ptr_iterate(str) do |c|
        chars << c
      end
      chars.should == [120, 121, 122]
    end

    it "allows changing the characters in the string" do
      str = "abc"
      @s.RSTRING_ptr_assign(str, 70)
      str.should == "FFF"
    end

    it "allows changing the string and calling a rb_str_xxx function" do
      str = "abc"
      @s.RSTRING_ptr_assign_call(str)
      str.should == "axcd"
    end

    it "allows changing the string and calling a method via rb_funcall" do
      str = "abc"
      @s.RSTRING_ptr_assign_funcall(str)
      str.should == "axce"
    end

    it "returns a struct with the string's length" do
      @s.RSTRING_len("dewdrops").should == 8
    end
  end

  describe "RSTRING_PTR" do
    it "returns a pointer to the string's contents" do
      str = "abc"
      chars = []
      @s.RSTRING_PTR_iterate(str) do |c|
        chars << c
      end
      chars.should == [97, 98, 99]
    end

    it "allows changing the characters in the string" do
      str = "abc"
      @s.RSTRING_PTR_assign(str, 65)
      str.should == "AAA"
    end
  end

  describe "RSTRING_LEN" do
    it "returns the size of the string" do
      @s.RSTRING_LEN("gumdrops").should == 8
    end
  end

  describe "StringValue" do
    it "does not call #to_str on a String" do
      str = "genuine"
      str.should_not_receive(:to_str)
      @s.StringValue(str)
    end

    it "does not call #to_s on a String" do
      str = "genuine"
      str.should_not_receive(:to_str)
      @s.StringValue(str)
    end

    it "calls #to_str on non-String objects" do
      str = mock("fake")
      str.should_receive(:to_str).and_return("wannabe")
      @s.StringValue(str)
    end

    it "does not call #to_s on non-String objects" do
      str = mock("fake")
      str.should_not_receive(:to_s)
      lambda { @s.StringValue(str) }.should raise_error(TypeError)
    end
  end
end
