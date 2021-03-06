require File.dirname(__FILE__) + '/../../../spec_helper'
require File.dirname(__FILE__) + '/fixtures/classes'

describe "Rubinius::Profiler::Instrumenter#profile" do
  before :each do
    @stdout, $stdout = $stdout, IOStub.new
    @profiler = Rubinius::Profiler::Instrumenter.new
  end

  after :each do
    $stdout = @stdout
  end

  it "profiles the code contained in the block" do
    @profiler.profile { ProfilerSpecs.work 10 }
    $stdout.should =~ /ProfilerSpecs.work/
  end

  it "returns the profile data" do
    data = @profiler.profile { ProfilerSpecs.work 10 }
    data.should be_kind_of(LookupTable)
    data.keys.should include(:method, :methods)
  end

  it "prints out the profile by default" do
    @profiler.profile { ProfilerSpecs.work 10 }
    $stdout.should =~ %r[ time   seconds   seconds    calls  ms/call  ms/call  name]
    $stdout.should =~ /ProfilerSpecs.work/
  end

  it "does not print the profile if passed false" do
    @profiler.profile(false) { ProfilerSpecs.work 10 }
    $stdout.should == ""
  end
end
