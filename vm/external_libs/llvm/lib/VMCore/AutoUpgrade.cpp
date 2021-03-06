//===-- AutoUpgrade.cpp - Implement auto-upgrade helper functions ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the auto-upgrade helper functions 
//
//===----------------------------------------------------------------------===//

#include "llvm/AutoUpgrade.h"
#include "llvm/Constants.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/Intrinsics.h"
#include "llvm/ADT/SmallVector.h"
#include <cstring>
using namespace llvm;


static bool UpgradeIntrinsicFunction1(Function *F, Function *&NewFn) {
  assert(F && "Illegal to upgrade a non-existent Function.");

  // Get the Function's name.
  const std::string& Name = F->getName();

  // Convenience
  const FunctionType *FTy = F->getFunctionType();

  // Quickly eliminate it, if it's not a candidate.
  if (Name.length() <= 8 || Name[0] != 'l' || Name[1] != 'l' || 
      Name[2] != 'v' || Name[3] != 'm' || Name[4] != '.')
    return false;

  Module *M = F->getParent();
  switch (Name[5]) {
  default: break;
  case 'b':
    //  This upgrades the name of the llvm.bswap intrinsic function to only use 
    //  a single type name for overloading. We only care about the old format
    //  'llvm.bswap.i*.i*', so check for 'bswap.' and then for there being 
    //  a '.' after 'bswap.'
    if (Name.compare(5,6,"bswap.",6) == 0) {
      std::string::size_type delim = Name.find('.',11);
      
      if (delim != std::string::npos) {
        //  Construct the new name as 'llvm.bswap' + '.i*'
        F->setName(Name.substr(0,10)+Name.substr(delim));
        NewFn = F;
        return true;
      }
    }
    break;

  case 'c':
    //  We only want to fix the 'llvm.ct*' intrinsics which do not have the 
    //  correct return type, so we check for the name, and then check if the 
    //  return type does not match the parameter type.
    if ( (Name.compare(5,5,"ctpop",5) == 0 ||
          Name.compare(5,4,"ctlz",4) == 0 ||
          Name.compare(5,4,"cttz",4) == 0) &&
        FTy->getReturnType() != FTy->getParamType(0)) {
      //  We first need to change the name of the old (bad) intrinsic, because 
      //  its type is incorrect, but we cannot overload that name. We 
      //  arbitrarily unique it here allowing us to construct a correctly named 
      //  and typed function below.
      F->setName("");

      //  Now construct the new intrinsic with the correct name and type. We 
      //  leave the old function around in order to query its type, whatever it 
      //  may be, and correctly convert up to the new type.
      NewFn = cast<Function>(M->getOrInsertFunction(Name, 
                                                    FTy->getParamType(0),
                                                    FTy->getParamType(0),
                                                    (Type *)0));
      return true;
    }
    break;

  case 'p':
    //  This upgrades the llvm.part.select overloaded intrinsic names to only 
    //  use one type specifier in the name. We only care about the old format
    //  'llvm.part.select.i*.i*', and solve as above with bswap.
    if (Name.compare(5,12,"part.select.",12) == 0) {
      std::string::size_type delim = Name.find('.',17);
      
      if (delim != std::string::npos) {
        //  Construct a new name as 'llvm.part.select' + '.i*'
        F->setName(Name.substr(0,16)+Name.substr(delim));
        NewFn = F;
        return true;
      }
      break;
    }

    //  This upgrades the llvm.part.set intrinsics similarly as above, however 
    //  we care about 'llvm.part.set.i*.i*.i*', but only the first two types 
    //  must match. There is an additional type specifier after these two 
    //  matching types that we must retain when upgrading.  Thus, we require 
    //  finding 2 periods, not just one, after the intrinsic name.
    if (Name.compare(5,9,"part.set.",9) == 0) {
      std::string::size_type delim = Name.find('.',14);

      if (delim != std::string::npos &&
          Name.find('.',delim+1) != std::string::npos) {
        //  Construct a new name as 'llvm.part.select' + '.i*.i*'
        F->setName(Name.substr(0,13)+Name.substr(delim));
        NewFn = F;
        return true;
      }
      break;
    }

    break;
  case 'x': 
    // This fixes all MMX shift intrinsic instructions to take a
    // v1i64 instead of a v2i32 as the second parameter.
    if (Name.compare(5,10,"x86.mmx.ps",10) == 0 &&
        (Name.compare(13,4,"psll", 4) == 0 ||
         Name.compare(13,4,"psra", 4) == 0 ||
         Name.compare(13,4,"psrl", 4) == 0) && Name[17] != 'i') {
      
      const llvm::Type *VT = VectorType::get(IntegerType::get(64), 1);
      
      // We don't have to do anything if the parameter already has
      // the correct type.
      if (FTy->getParamType(1) == VT)
        break;
      
      //  We first need to change the name of the old (bad) intrinsic, because 
      //  its type is incorrect, but we cannot overload that name. We 
      //  arbitrarily unique it here allowing us to construct a correctly named 
      //  and typed function below.
      F->setName("");

      assert(FTy->getNumParams() == 2 && "MMX shift intrinsics take 2 args!");
      
      //  Now construct the new intrinsic with the correct name and type. We 
      //  leave the old function around in order to query its type, whatever it 
      //  may be, and correctly convert up to the new type.
      NewFn = cast<Function>(M->getOrInsertFunction(Name, 
                                                    FTy->getReturnType(),
                                                    FTy->getParamType(0),
                                                    VT,
                                                    (Type *)0));
      return true;
    } else if (Name.compare(5,16,"x86.sse2.movl.dq",16) == 0) {
      // Calls to this intrinsic are transformed into ShuffleVector's.
      NewFn = 0;
      return true;
    }

    break;
  }

  //  This may not belong here. This function is effectively being overloaded 
  //  to both detect an intrinsic which needs upgrading, and to provide the 
  //  upgraded form of the intrinsic. We should perhaps have two separate 
  //  functions for this.
  return false;
}

bool llvm::UpgradeIntrinsicFunction(Function *F, Function *&NewFn) {
  NewFn = 0;
  bool Upgraded = UpgradeIntrinsicFunction1(F, NewFn);

  // Upgrade intrinsic attributes.  This does not change the function.
  if (NewFn)
    F = NewFn;
  if (unsigned id = F->getIntrinsicID(true))
    F->setParamAttrs(Intrinsic::getParamAttrs((Intrinsic::ID)id));
  return Upgraded;
}

// UpgradeIntrinsicCall - Upgrade a call to an old intrinsic to be a call the 
// upgraded intrinsic. All argument and return casting must be provided in 
// order to seamlessly integrate with existing context.
void llvm::UpgradeIntrinsicCall(CallInst *CI, Function *NewFn) {
  Function *F = CI->getCalledFunction();
  assert(F && "CallInst has no function associated with it.");

  if (!NewFn) {
    if (strcmp(F->getNameStart(), "llvm.x86.sse2.movl.dq") == 0) {
      std::vector<Constant*> Idxs;
      Constant *Zero = ConstantInt::get(Type::Int32Ty, 0);
      Idxs.push_back(Zero);
      Idxs.push_back(Zero);
      Idxs.push_back(Zero);
      Idxs.push_back(Zero);
      Value *ZeroV = ConstantVector::get(Idxs);

      Idxs.clear(); 
      Idxs.push_back(ConstantInt::get(Type::Int32Ty, 4));
      Idxs.push_back(ConstantInt::get(Type::Int32Ty, 5));
      Idxs.push_back(ConstantInt::get(Type::Int32Ty, 2));
      Idxs.push_back(ConstantInt::get(Type::Int32Ty, 3));
      Value *Mask = ConstantVector::get(Idxs);
      ShuffleVectorInst *SI = new ShuffleVectorInst(ZeroV, CI->getOperand(1),
                                                    Mask, "upgraded", CI);

      // Handle any uses of the old CallInst.
      if (!CI->use_empty())
        //  Replace all uses of the old call with the new cast which has the 
        //  correct type.
        CI->replaceAllUsesWith(SI);
      
      //  Clean up the old call now that it has been completely upgraded.
      CI->eraseFromParent();
    } else {
      assert(0 && "Unknown function for CallInst upgrade.");
    }
    return;
  }

  switch (NewFn->getIntrinsicID()) {
  default:  assert(0 && "Unknown function for CallInst upgrade.");
  case Intrinsic::x86_mmx_psll_d:
  case Intrinsic::x86_mmx_psll_q:
  case Intrinsic::x86_mmx_psll_w:
  case Intrinsic::x86_mmx_psra_d:
  case Intrinsic::x86_mmx_psra_w:
  case Intrinsic::x86_mmx_psrl_d:
  case Intrinsic::x86_mmx_psrl_q:
  case Intrinsic::x86_mmx_psrl_w: {
    Value *Operands[2];
    
    Operands[0] = CI->getOperand(1);
    
    // Cast the second parameter to the correct type.
    BitCastInst *BC = new BitCastInst(CI->getOperand(2), 
                                      NewFn->getFunctionType()->getParamType(1),
                                      "upgraded", CI);
    Operands[1] = BC;
    
    //  Construct a new CallInst
    CallInst *NewCI = CallInst::Create(NewFn, Operands, Operands+2, 
                                       "upgraded."+CI->getName(), CI);
    NewCI->setTailCall(CI->isTailCall());
    NewCI->setCallingConv(CI->getCallingConv());
    
    //  Handle any uses of the old CallInst.
    if (!CI->use_empty())
      //  Replace all uses of the old call with the new cast which has the 
      //  correct type.
      CI->replaceAllUsesWith(NewCI);
    
    //  Clean up the old call now that it has been completely upgraded.
    CI->eraseFromParent();
    break;
  }        
  case Intrinsic::ctlz:
  case Intrinsic::ctpop:
  case Intrinsic::cttz: {
    //  Build a small vector of the 1..(N-1) operands, which are the 
    //  parameters.
    SmallVector<Value*, 8> Operands(CI->op_begin()+1, CI->op_end());

    //  Construct a new CallInst
    CallInst *NewCI = CallInst::Create(NewFn, Operands.begin(), Operands.end(),
                                       "upgraded."+CI->getName(), CI);
    NewCI->setTailCall(CI->isTailCall());
    NewCI->setCallingConv(CI->getCallingConv());

    //  Handle any uses of the old CallInst.
    if (!CI->use_empty()) {
      //  Check for sign extend parameter attributes on the return values.
      bool SrcSExt = NewFn->getParamAttrs().paramHasAttr(0, ParamAttr::SExt);
      bool DestSExt = F->getParamAttrs().paramHasAttr(0, ParamAttr::SExt);
      
      //  Construct an appropriate cast from the new return type to the old.
      CastInst *RetCast = CastInst::create(
                            CastInst::getCastOpcode(NewCI, SrcSExt,
                                                    F->getReturnType(),
                                                    DestSExt),
                            NewCI, F->getReturnType(),
                            NewCI->getName(), CI);
      NewCI->moveBefore(RetCast);

      //  Replace all uses of the old call with the new cast which has the 
      //  correct type.
      CI->replaceAllUsesWith(RetCast);
    }

    //  Clean up the old call now that it has been completely upgraded.
    CI->eraseFromParent();
  }
  break;
  }
}

// This tests each Function to determine if it needs upgrading. When we find 
// one we are interested in, we then upgrade all calls to reflect the new 
// function.
void llvm::UpgradeCallsToIntrinsic(Function* F) {
  assert(F && "Illegal attempt to upgrade a non-existent intrinsic.");

  // Upgrade the function and check if it is a totaly new function.
  Function* NewFn;
  if (UpgradeIntrinsicFunction(F, NewFn)) {
    if (NewFn != F) {
      // Replace all uses to the old function with the new one if necessary.
      for (Value::use_iterator UI = F->use_begin(), UE = F->use_end();
           UI != UE; ) {
        if (CallInst* CI = dyn_cast<CallInst>(*UI++))
          UpgradeIntrinsicCall(CI, NewFn);
      }
      // Remove old function, no longer used, from the module.
      F->eraseFromParent();
    }
  }
}

