//===-- IndMemRemoval.cpp - Remove indirect allocations and frees ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass finds places where memory allocation functions may escape into
// indirect land.  Some transforms are much easier (aka possible) only if free 
// or malloc are not called indirectly.
// Thus find places where the address of memory functions are taken and construct
// bounce functions with direct calls of those functions.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "indmemrem"
#include "llvm/Transforms/IPO.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/Type.h"
#include "llvm/DerivedTypes.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/Compiler.h"
using namespace llvm;

STATISTIC(NumBounceSites, "Number of sites modified");
STATISTIC(NumBounce     , "Number of bounce functions created");

namespace {
  class VISIBILITY_HIDDEN IndMemRemPass : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    IndMemRemPass() : ModulePass((intptr_t)&ID) {}

    virtual bool runOnModule(Module &M);
  };
  char IndMemRemPass::ID = 0;
  RegisterPass<IndMemRemPass> X("indmemrem","Indirect Malloc and Free Removal");
} // end anonymous namespace


bool IndMemRemPass::runOnModule(Module &M) {
  //in Theory, all direct calls of malloc and free should be promoted
  //to intrinsics.  Therefor, this goes through and finds where the
  //address of free or malloc are taken and replaces those with bounce
  //functions, ensuring that all malloc and free that might happen
  //happen through intrinsics.
  bool changed = false;
  if (Function* F = M.getFunction("free")) {
    assert(F->isDeclaration() && "free not external?");
    if (!F->use_empty()) {
      Function* FN = Function::Create(F->getFunctionType(),
                                      GlobalValue::LinkOnceLinkage,
                                      "free_llvm_bounce", &M);
      BasicBlock* bb = BasicBlock::Create("entry",FN);
      Instruction* R = ReturnInst::Create(bb);
      new FreeInst(FN->arg_begin(), R);
      ++NumBounce;
      NumBounceSites += F->getNumUses();
      F->replaceAllUsesWith(FN);
      changed = true;
    }
  }
  if (Function* F = M.getFunction("malloc")) {
    assert(F->isDeclaration() && "malloc not external?");
    if (!F->use_empty()) {
      Function* FN = Function::Create(F->getFunctionType(), 
                                      GlobalValue::LinkOnceLinkage, 
                                      "malloc_llvm_bounce", &M);
      BasicBlock* bb = BasicBlock::Create("entry",FN);
      Instruction* c = CastInst::createIntegerCast(
          FN->arg_begin(), Type::Int32Ty, false, "c", bb);
      Instruction* a = new MallocInst(Type::Int8Ty, c, "m", bb);
      ReturnInst::Create(a, bb);
      ++NumBounce;
      NumBounceSites += F->getNumUses();
      F->replaceAllUsesWith(FN);
      changed = true;
    }
  }
  return changed;
}

ModulePass *llvm::createIndMemRemPass() {
  return new IndMemRemPass();
}
