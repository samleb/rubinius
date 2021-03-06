//===-- FileWriters.h - File Writers Creation Functions ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Functions to add the various file writer passes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_FILEWRITERS_H
#define LLVM_CODEGEN_FILEWRITERS_H

#include <iosfwd>

namespace llvm {

  class PassManagerBase;
  class MachineCodeEmitter;
  class TargetMachine;

  MachineCodeEmitter *AddELFWriter(PassManagerBase &FPM, std::ostream &O,
                                   TargetMachine &TM);
  MachineCodeEmitter *AddMachOWriter(PassManagerBase &FPM, std::ostream &O,
                                     TargetMachine &TM);

} // end llvm namespace

#endif // LLVM_CODEGEN_FILEWRITERS_H
