//===-- llvm/CodeGen/SelectionDAG.h - InstSelection DAG ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the SelectionDAG class, and transitively defines the
// SDNode class and subclasses.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_SELECTIONDAG_H
#define LLVM_CODEGEN_SELECTIONDAG_H

#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/ilist"
#include "llvm/CodeGen/SelectionDAGNodes.h"

#include <list>
#include <vector>
#include <map>
#include <set>
#include <string>

namespace llvm {
  class AliasAnalysis;
  class TargetLowering;
  class TargetMachine;
  class MachineModuleInfo;
  class MachineFunction;
  class MachineConstantPoolValue;

/// SelectionDAG class - This is used to represent a portion of an LLVM function
/// in a low-level Data Dependence DAG representation suitable for instruction
/// selection.  This DAG is constructed as the first step of instruction
/// selection in order to allow implementation of machine specific optimizations
/// and code simplifications.
///
/// The representation used by the SelectionDAG is a target-independent
/// representation, which has some similarities to the GCC RTL representation,
/// but is significantly more simple, powerful, and is a graph form instead of a
/// linear form.
///
class SelectionDAG {
  TargetLowering &TLI;
  MachineFunction &MF;
  MachineModuleInfo *MMI;

  /// Root - The root of the entire DAG.  EntryNode - The starting token.
  SDOperand Root, EntryNode;

  /// AllNodes - A linked list of nodes in the current DAG.
  ilist<SDNode> AllNodes;

  /// CSEMap - This structure is used to memoize nodes, automatically performing
  /// CSE with existing nodes with a duplicate is requested.
  FoldingSet<SDNode> CSEMap;

public:
  SelectionDAG(TargetLowering &tli, MachineFunction &mf, MachineModuleInfo *mmi)
  : TLI(tli), MF(mf), MMI(mmi) {
    EntryNode = Root = getNode(ISD::EntryToken, MVT::Other);
  }
  ~SelectionDAG();

  MachineFunction &getMachineFunction() const { return MF; }
  const TargetMachine &getTarget() const;
  TargetLowering &getTargetLoweringInfo() const { return TLI; }
  MachineModuleInfo *getMachineModuleInfo() const { return MMI; }

  /// viewGraph - Pop up a GraphViz/gv window with the DAG rendered using 'dot'.
  ///
  void viewGraph();
  
#ifndef NDEBUG
  std::map<const SDNode *, std::string> NodeGraphAttrs;
#endif

  /// clearGraphAttrs - Clear all previously defined node graph attributes.
  /// Intended to be used from a debugging tool (eg. gdb).
  void clearGraphAttrs();
  
  /// setGraphAttrs - Set graph attributes for a node. (eg. "color=red".)
  ///
  void setGraphAttrs(const SDNode *N, const char *Attrs);
  
  /// getGraphAttrs - Get graph attributes for a node. (eg. "color=red".)
  /// Used from getNodeAttributes.
  const std::string getGraphAttrs(const SDNode *N) const;
  
  /// setGraphColor - Convenience for setting node color attribute.
  ///
  void setGraphColor(const SDNode *N, const char *Color);

  typedef ilist<SDNode>::const_iterator allnodes_const_iterator;
  allnodes_const_iterator allnodes_begin() const { return AllNodes.begin(); }
  allnodes_const_iterator allnodes_end() const { return AllNodes.end(); }
  typedef ilist<SDNode>::iterator allnodes_iterator;
  allnodes_iterator allnodes_begin() { return AllNodes.begin(); }
  allnodes_iterator allnodes_end() { return AllNodes.end(); }
  
  /// getRoot - Return the root tag of the SelectionDAG.
  ///
  const SDOperand &getRoot() const { return Root; }

  /// getEntryNode - Return the token chain corresponding to the entry of the
  /// function.
  const SDOperand &getEntryNode() const { return EntryNode; }

  /// setRoot - Set the current root tag of the SelectionDAG.
  ///
  const SDOperand &setRoot(SDOperand N) { return Root = N; }

  /// Combine - This iterates over the nodes in the SelectionDAG, folding
  /// certain types of nodes together, or eliminating superfluous nodes.  When
  /// the AfterLegalize argument is set to 'true', Combine takes care not to
  /// generate any nodes that will be illegal on the target.
  void Combine(bool AfterLegalize, AliasAnalysis &AA);
  
  /// LegalizeTypes - This transforms the SelectionDAG into a SelectionDAG that
  /// only uses types natively supported by the target.
  ///
  /// Note that this is an involved process that may invalidate pointers into
  /// the graph.
  void LegalizeTypes();
  
  /// Legalize - This transforms the SelectionDAG into a SelectionDAG that is
  /// compatible with the target instruction selector, as indicated by the
  /// TargetLowering object.
  ///
  /// Note that this is an involved process that may invalidate pointers into
  /// the graph.
  void Legalize();

  /// RemoveDeadNodes - This method deletes all unreachable nodes in the
  /// SelectionDAG.
  void RemoveDeadNodes();

  /// DeleteNode - Remove the specified node from the system.  This node must
  /// have no referrers.
  void DeleteNode(SDNode *N);

  /// getVTList - Return an SDVTList that represents the list of values
  /// specified.
  SDVTList getVTList(MVT::ValueType VT);
  SDVTList getVTList(MVT::ValueType VT1, MVT::ValueType VT2);
  SDVTList getVTList(MVT::ValueType VT1, MVT::ValueType VT2,MVT::ValueType VT3);
  SDVTList getVTList(const MVT::ValueType *VTs, unsigned NumVTs);
  
  /// getNodeValueTypes - These are obsolete, use getVTList instead.
  const MVT::ValueType *getNodeValueTypes(MVT::ValueType VT) {
    return getVTList(VT).VTs;
  }
  const MVT::ValueType *getNodeValueTypes(MVT::ValueType VT1, 
                                          MVT::ValueType VT2) {
    return getVTList(VT1, VT2).VTs;
  }
  const MVT::ValueType *getNodeValueTypes(MVT::ValueType VT1,MVT::ValueType VT2,
                                          MVT::ValueType VT3) {
    return getVTList(VT1, VT2, VT3).VTs;
  }
  const MVT::ValueType *getNodeValueTypes(std::vector<MVT::ValueType> &VTList) {
    return getVTList(&VTList[0], (unsigned)VTList.size()).VTs;
  }
  
  
  //===--------------------------------------------------------------------===//
  // Node creation methods.
  //
  SDOperand getString(const std::string &Val);
  SDOperand getConstant(uint64_t Val, MVT::ValueType VT, bool isTarget = false);
  SDOperand getConstant(const APInt &Val, MVT::ValueType VT, bool isTarget = false);
  SDOperand getIntPtrConstant(uint64_t Val, bool isTarget = false);
  SDOperand getTargetConstant(uint64_t Val, MVT::ValueType VT) {
    return getConstant(Val, VT, true);
  }
  SDOperand getTargetConstant(const APInt &Val, MVT::ValueType VT) {
    return getConstant(Val, VT, true);
  }
  SDOperand getConstantFP(double Val, MVT::ValueType VT, bool isTarget = false);
  SDOperand getConstantFP(const APFloat& Val, MVT::ValueType VT, 
                          bool isTarget = false);
  SDOperand getTargetConstantFP(double Val, MVT::ValueType VT) {
    return getConstantFP(Val, VT, true);
  }
  SDOperand getTargetConstantFP(const APFloat& Val, MVT::ValueType VT) {
    return getConstantFP(Val, VT, true);
  }
  SDOperand getGlobalAddress(const GlobalValue *GV, MVT::ValueType VT,
                             int offset = 0, bool isTargetGA = false);
  SDOperand getTargetGlobalAddress(const GlobalValue *GV, MVT::ValueType VT,
                                   int offset = 0) {
    return getGlobalAddress(GV, VT, offset, true);
  }
  SDOperand getFrameIndex(int FI, MVT::ValueType VT, bool isTarget = false);
  SDOperand getTargetFrameIndex(int FI, MVT::ValueType VT) {
    return getFrameIndex(FI, VT, true);
  }
  SDOperand getJumpTable(int JTI, MVT::ValueType VT, bool isTarget = false);
  SDOperand getTargetJumpTable(int JTI, MVT::ValueType VT) {
    return getJumpTable(JTI, VT, true);
  }
  SDOperand getConstantPool(Constant *C, MVT::ValueType VT,
                            unsigned Align = 0, int Offs = 0, bool isT=false);
  SDOperand getTargetConstantPool(Constant *C, MVT::ValueType VT,
                                  unsigned Align = 0, int Offset = 0) {
    return getConstantPool(C, VT, Align, Offset, true);
  }
  SDOperand getConstantPool(MachineConstantPoolValue *C, MVT::ValueType VT,
                            unsigned Align = 0, int Offs = 0, bool isT=false);
  SDOperand getTargetConstantPool(MachineConstantPoolValue *C,
                                  MVT::ValueType VT, unsigned Align = 0,
                                  int Offset = 0) {
    return getConstantPool(C, VT, Align, Offset, true);
  }
  SDOperand getBasicBlock(MachineBasicBlock *MBB);
  SDOperand getExternalSymbol(const char *Sym, MVT::ValueType VT);
  SDOperand getTargetExternalSymbol(const char *Sym, MVT::ValueType VT);
  SDOperand getArgFlags(ISD::ArgFlagsTy Flags);
  SDOperand getValueType(MVT::ValueType);
  SDOperand getRegister(unsigned Reg, MVT::ValueType VT);

  SDOperand getCopyToReg(SDOperand Chain, unsigned Reg, SDOperand N) {
    return getNode(ISD::CopyToReg, MVT::Other, Chain,
                   getRegister(Reg, N.getValueType()), N);
  }

  // This version of the getCopyToReg method takes an extra operand, which
  // indicates that there is potentially an incoming flag value (if Flag is not
  // null) and that there should be a flag result.
  SDOperand getCopyToReg(SDOperand Chain, unsigned Reg, SDOperand N,
                         SDOperand Flag) {
    const MVT::ValueType *VTs = getNodeValueTypes(MVT::Other, MVT::Flag);
    SDOperand Ops[] = { Chain, getRegister(Reg, N.getValueType()), N, Flag };
    return getNode(ISD::CopyToReg, VTs, 2, Ops, Flag.Val ? 4 : 3);
  }

  // Similar to last getCopyToReg() except parameter Reg is a SDOperand
  SDOperand getCopyToReg(SDOperand Chain, SDOperand Reg, SDOperand N,
                         SDOperand Flag) {
    const MVT::ValueType *VTs = getNodeValueTypes(MVT::Other, MVT::Flag);
    SDOperand Ops[] = { Chain, Reg, N, Flag };
    return getNode(ISD::CopyToReg, VTs, 2, Ops, Flag.Val ? 4 : 3);
  }
  
  SDOperand getCopyFromReg(SDOperand Chain, unsigned Reg, MVT::ValueType VT) {
    const MVT::ValueType *VTs = getNodeValueTypes(VT, MVT::Other);
    SDOperand Ops[] = { Chain, getRegister(Reg, VT) };
    return getNode(ISD::CopyFromReg, VTs, 2, Ops, 2);
  }
  
  // This version of the getCopyFromReg method takes an extra operand, which
  // indicates that there is potentially an incoming flag value (if Flag is not
  // null) and that there should be a flag result.
  SDOperand getCopyFromReg(SDOperand Chain, unsigned Reg, MVT::ValueType VT,
                           SDOperand Flag) {
    const MVT::ValueType *VTs = getNodeValueTypes(VT, MVT::Other, MVT::Flag);
    SDOperand Ops[] = { Chain, getRegister(Reg, VT), Flag };
    return getNode(ISD::CopyFromReg, VTs, 3, Ops, Flag.Val ? 3 : 2);
  }

  SDOperand getCondCode(ISD::CondCode Cond);

  /// getZeroExtendInReg - Return the expression required to zero extend the Op
  /// value assuming it was the smaller SrcTy value.
  SDOperand getZeroExtendInReg(SDOperand Op, MVT::ValueType SrcTy);
  
  /// getCALLSEQ_START - Return a new CALLSEQ_START node, which always must have
  /// a flag result (to ensure it's not CSE'd).
  SDOperand getCALLSEQ_START(SDOperand Chain, SDOperand Op) {
    const MVT::ValueType *VTs = getNodeValueTypes(MVT::Other, MVT::Flag);
    SDOperand Ops[] = { Chain,  Op };
    return getNode(ISD::CALLSEQ_START, VTs, 2, Ops, 2);
  }

  /// getCALLSEQ_END - Return a new CALLSEQ_END node, which always must have a
  /// flag result (to ensure it's not CSE'd).
  SDOperand getCALLSEQ_END(SDOperand Chain, SDOperand Op1, SDOperand Op2,
                           SDOperand InFlag) {
    SDVTList NodeTys = getVTList(MVT::Other, MVT::Flag);
    SmallVector<SDOperand, 4> Ops;
    Ops.push_back(Chain);
    Ops.push_back(Op1);
    Ops.push_back(Op2);
    Ops.push_back(InFlag);
    return getNode(ISD::CALLSEQ_END, NodeTys, &Ops[0],
                   (unsigned)Ops.size() - (InFlag.Val == 0 ? 1 : 0));
  }

  /// getNode - Gets or creates the specified node.
  ///
  SDOperand getNode(unsigned Opcode, MVT::ValueType VT);
  SDOperand getNode(unsigned Opcode, MVT::ValueType VT, SDOperand N);
  SDOperand getNode(unsigned Opcode, MVT::ValueType VT,
                    SDOperand N1, SDOperand N2);
  SDOperand getNode(unsigned Opcode, MVT::ValueType VT,
                    SDOperand N1, SDOperand N2, SDOperand N3);
  SDOperand getNode(unsigned Opcode, MVT::ValueType VT,
                    SDOperand N1, SDOperand N2, SDOperand N3, SDOperand N4);
  SDOperand getNode(unsigned Opcode, MVT::ValueType VT,
                    SDOperand N1, SDOperand N2, SDOperand N3, SDOperand N4,
                    SDOperand N5);
  SDOperand getNode(unsigned Opcode, MVT::ValueType VT,
                    SDOperandPtr Ops, unsigned NumOps);
  SDOperand getNode(unsigned Opcode, std::vector<MVT::ValueType> &ResultTys,
                    SDOperandPtr Ops, unsigned NumOps);
  SDOperand getNode(unsigned Opcode, const MVT::ValueType *VTs, unsigned NumVTs,
                    SDOperandPtr Ops, unsigned NumOps);
  SDOperand getNode(unsigned Opcode, SDVTList VTs);
  SDOperand getNode(unsigned Opcode, SDVTList VTs, SDOperand N);
  SDOperand getNode(unsigned Opcode, SDVTList VTs,
                    SDOperand N1, SDOperand N2);
  SDOperand getNode(unsigned Opcode, SDVTList VTs,
                    SDOperand N1, SDOperand N2, SDOperand N3);
  SDOperand getNode(unsigned Opcode, SDVTList VTs,
                    SDOperand N1, SDOperand N2, SDOperand N3, SDOperand N4);
  SDOperand getNode(unsigned Opcode, SDVTList VTs,
                    SDOperand N1, SDOperand N2, SDOperand N3, SDOperand N4,
                    SDOperand N5);
  SDOperand getNode(unsigned Opcode, SDVTList VTs,
                    SDOperandPtr Ops, unsigned NumOps);

  SDOperand getMemcpy(SDOperand Chain, SDOperand Dst, SDOperand Src,
                      SDOperand Size, unsigned Align,
                      bool AlwaysInline,
                      const Value *DstSV, uint64_t DstSVOff,
                      const Value *SrcSV, uint64_t SrcSVOff);

  SDOperand getMemmove(SDOperand Chain, SDOperand Dst, SDOperand Src,
                       SDOperand Size, unsigned Align,
                       const Value *DstSV, uint64_t DstOSVff,
                       const Value *SrcSV, uint64_t SrcSVOff);

  SDOperand getMemset(SDOperand Chain, SDOperand Dst, SDOperand Src,
                      SDOperand Size, unsigned Align,
                      const Value *DstSV, uint64_t DstSVOff);

  /// getSetCC - Helper function to make it easier to build SetCC's if you just
  /// have an ISD::CondCode instead of an SDOperand.
  ///
  SDOperand getSetCC(MVT::ValueType VT, SDOperand LHS, SDOperand RHS,
                     ISD::CondCode Cond) {
    return getNode(ISD::SETCC, VT, LHS, RHS, getCondCode(Cond));
  }

  /// getSelectCC - Helper function to make it easier to build SelectCC's if you
  /// just have an ISD::CondCode instead of an SDOperand.
  ///
  SDOperand getSelectCC(SDOperand LHS, SDOperand RHS,
                        SDOperand True, SDOperand False, ISD::CondCode Cond) {
    return getNode(ISD::SELECT_CC, True.getValueType(), LHS, RHS, True, False,
                   getCondCode(Cond));
  }
  
  /// getVAArg - VAArg produces a result and token chain, and takes a pointer
  /// and a source value as input.
  SDOperand getVAArg(MVT::ValueType VT, SDOperand Chain, SDOperand Ptr,
                     SDOperand SV);

  /// getAtomic - Gets a node for an atomic op, produces result and chain, takes
  // 3 operands
  SDOperand getAtomic(unsigned Opcode, SDOperand Chain, SDOperand Ptr, 
                      SDOperand Cmp, SDOperand Swp, MVT::ValueType VT);

  /// getAtomic - Gets a node for an atomic op, produces result and chain, takes
  // 2 operands
  SDOperand getAtomic(unsigned Opcode, SDOperand Chain, SDOperand Ptr, 
                      SDOperand Val, MVT::ValueType VT);

  /// getLoad - Loads are not normal binary operators: their result type is not
  /// determined by their operands, and they produce a value AND a token chain.
  ///
  SDOperand getLoad(MVT::ValueType VT, SDOperand Chain, SDOperand Ptr,
                    const Value *SV, int SVOffset, bool isVolatile=false,
                    unsigned Alignment=0);
  SDOperand getExtLoad(ISD::LoadExtType ExtType, MVT::ValueType VT,
                       SDOperand Chain, SDOperand Ptr, const Value *SV,
                       int SVOffset, MVT::ValueType EVT, bool isVolatile=false,
                       unsigned Alignment=0);
  SDOperand getIndexedLoad(SDOperand OrigLoad, SDOperand Base,
                           SDOperand Offset, ISD::MemIndexedMode AM);
  SDOperand getLoad(ISD::MemIndexedMode AM, ISD::LoadExtType ExtType,
                    MVT::ValueType VT, SDOperand Chain,
                    SDOperand Ptr, SDOperand Offset,
                    const Value *SV, int SVOffset, MVT::ValueType EVT,
                    bool isVolatile=false, unsigned Alignment=0);

  /// getStore - Helper function to build ISD::STORE nodes.
  ///
  SDOperand getStore(SDOperand Chain, SDOperand Val, SDOperand Ptr,
                     const Value *SV, int SVOffset, bool isVolatile=false,
                     unsigned Alignment=0);
  SDOperand getTruncStore(SDOperand Chain, SDOperand Val, SDOperand Ptr,
                          const Value *SV, int SVOffset, MVT::ValueType TVT,
                          bool isVolatile=false, unsigned Alignment=0);
  SDOperand getIndexedStore(SDOperand OrigStoe, SDOperand Base,
                           SDOperand Offset, ISD::MemIndexedMode AM);

  // getSrcValue - Construct a node to track a Value* through the backend.
  SDOperand getSrcValue(const Value *v);

  // getMemOperand - Construct a node to track a memory reference
  // through the backend.
  SDOperand getMemOperand(const MachineMemOperand &MO);

  /// UpdateNodeOperands - *Mutate* the specified node in-place to have the
  /// specified operands.  If the resultant node already exists in the DAG,
  /// this does not modify the specified node, instead it returns the node that
  /// already exists.  If the resultant node does not exist in the DAG, the
  /// input node is returned.  As a degenerate case, if you specify the same
  /// input operands as the node already has, the input node is returned.
  SDOperand UpdateNodeOperands(SDOperand N, SDOperand Op);
  SDOperand UpdateNodeOperands(SDOperand N, SDOperand Op1, SDOperand Op2);
  SDOperand UpdateNodeOperands(SDOperand N, SDOperand Op1, SDOperand Op2,
                               SDOperand Op3);
  SDOperand UpdateNodeOperands(SDOperand N, SDOperand Op1, SDOperand Op2,
                               SDOperand Op3, SDOperand Op4);
  SDOperand UpdateNodeOperands(SDOperand N, SDOperand Op1, SDOperand Op2,
                               SDOperand Op3, SDOperand Op4, SDOperand Op5);
  SDOperand UpdateNodeOperands(SDOperand N, SDOperandPtr Ops, unsigned NumOps);
  
  /// SelectNodeTo - These are used for target selectors to *mutate* the
  /// specified node to have the specified return type, Target opcode, and
  /// operands.  Note that target opcodes are stored as
  /// ISD::BUILTIN_OP_END+TargetOpcode in the node opcode field.  The 0th value
  /// of the resultant node is returned.
  SDNode *SelectNodeTo(SDNode *N, unsigned TargetOpc, MVT::ValueType VT);
  SDNode *SelectNodeTo(SDNode *N, unsigned TargetOpc, MVT::ValueType VT, 
                       SDOperand Op1);
  SDNode *SelectNodeTo(SDNode *N, unsigned TargetOpc, MVT::ValueType VT, 
                       SDOperand Op1, SDOperand Op2);
  SDNode *SelectNodeTo(SDNode *N, unsigned TargetOpc, MVT::ValueType VT, 
                       SDOperand Op1, SDOperand Op2, SDOperand Op3);
  SDNode *SelectNodeTo(SDNode *N, unsigned TargetOpc, MVT::ValueType VT,
                       SDOperandPtr Ops, unsigned NumOps);
  SDNode *SelectNodeTo(SDNode *N, unsigned TargetOpc, MVT::ValueType VT1, 
                       MVT::ValueType VT2, SDOperand Op1, SDOperand Op2);
  SDNode *SelectNodeTo(SDNode *N, unsigned TargetOpc, MVT::ValueType VT1,
                       MVT::ValueType VT2, SDOperand Op1, SDOperand Op2,
                       SDOperand Op3);


  /// getTargetNode - These are used for target selectors to create a new node
  /// with specified return type(s), target opcode, and operands.
  ///
  /// Note that getTargetNode returns the resultant node.  If there is already a
  /// node of the specified opcode and operands, it returns that node instead of
  /// the current one.
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT,
                        SDOperand Op1);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT,
                        SDOperand Op1, SDOperand Op2);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT,
                        SDOperand Op1, SDOperand Op2, SDOperand Op3);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT,
                        SDOperandPtr Ops, unsigned NumOps);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT1,
                        MVT::ValueType VT2);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT1,
                        MVT::ValueType VT2, SDOperand Op1);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT1,
                        MVT::ValueType VT2, SDOperand Op1, SDOperand Op2);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT1,
                        MVT::ValueType VT2, SDOperand Op1, SDOperand Op2,
                        SDOperand Op3);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT1, 
                        MVT::ValueType VT2,
                        SDOperandPtr Ops, unsigned NumOps);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT1,
                        MVT::ValueType VT2, MVT::ValueType VT3,
                        SDOperand Op1, SDOperand Op2);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT1,
                        MVT::ValueType VT2, MVT::ValueType VT3,
                        SDOperand Op1, SDOperand Op2, SDOperand Op3);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT1, 
                        MVT::ValueType VT2, MVT::ValueType VT3,
                        SDOperandPtr Ops, unsigned NumOps);
  SDNode *getTargetNode(unsigned Opcode, MVT::ValueType VT1, 
                        MVT::ValueType VT2, MVT::ValueType VT3,
                        MVT::ValueType VT4,
                        SDOperandPtr Ops, unsigned NumOps);
  SDNode *getTargetNode(unsigned Opcode, std::vector<MVT::ValueType> &ResultTys,
                        SDOperandPtr Ops, unsigned NumOps);

  /// getNodeIfExists - Get the specified node if it's already available, or
  /// else return NULL.
  SDNode *getNodeIfExists(unsigned Opcode, SDVTList VTs,
                          SDOperandPtr Ops, unsigned NumOps);
  
  /// DAGUpdateListener - Clients of various APIs that cause global effects on
  /// the DAG can optionally implement this interface.  This allows the clients
  /// to handle the various sorts of updates that happen.
  class DAGUpdateListener {
  public:
    virtual ~DAGUpdateListener();
    virtual void NodeDeleted(SDNode *N) = 0;
    virtual void NodeUpdated(SDNode *N) = 0;
  };
  
  /// RemoveDeadNode - Remove the specified node from the system. If any of its
  /// operands then becomes dead, remove them as well. Inform UpdateListener
  /// for each node deleted.
  void RemoveDeadNode(SDNode *N, DAGUpdateListener *UpdateListener = 0);
  
  /// ReplaceAllUsesWith - Modify anything using 'From' to use 'To' instead.
  /// This can cause recursive merging of nodes in the DAG.  Use the first
  /// version if 'From' is known to have a single result, use the second
  /// if you have two nodes with identical results, use the third otherwise.
  ///
  /// These methods all take an optional UpdateListener, which (if not null) is 
  /// informed about nodes that are deleted and modified due to recursive
  /// changes in the dag.
  ///
  void ReplaceAllUsesWith(SDOperand From, SDOperand Op,
                          DAGUpdateListener *UpdateListener = 0);
  void ReplaceAllUsesWith(SDNode *From, SDNode *To,
                          DAGUpdateListener *UpdateListener = 0);
  void ReplaceAllUsesWith(SDNode *From, SDOperandPtr To,
                          DAGUpdateListener *UpdateListener = 0);

  /// ReplaceAllUsesOfValueWith - Replace any uses of From with To, leaving
  /// uses of other values produced by From.Val alone.
  void ReplaceAllUsesOfValueWith(SDOperand From, SDOperand To,
                                 DAGUpdateListener *UpdateListener = 0);

  /// AssignNodeIds - Assign a unique node id for each node in the DAG based on
  /// their allnodes order. It returns the maximum id.
  unsigned AssignNodeIds();

  /// AssignTopologicalOrder - Assign a unique node id for each node in the DAG
  /// based on their topological order. It returns the maximum id and a vector
  /// of the SDNodes* in assigned order by reference.
  unsigned AssignTopologicalOrder(std::vector<SDNode*> &TopOrder);

  /// isCommutativeBinOp - Returns true if the opcode is a commutative binary
  /// operation.
  static bool isCommutativeBinOp(unsigned Opcode) {
    // FIXME: This should get its info from the td file, so that we can include
    // target info.
    switch (Opcode) {
    case ISD::ADD:
    case ISD::MUL:
    case ISD::MULHU:
    case ISD::MULHS:
    case ISD::SMUL_LOHI:
    case ISD::UMUL_LOHI:
    case ISD::FADD:
    case ISD::FMUL:
    case ISD::AND:
    case ISD::OR:
    case ISD::XOR:
    case ISD::ADDC: 
    case ISD::ADDE: return true;
    default: return false;
    }
  }

  void dump() const;

  /// CreateStackTemporary - Create a stack temporary, suitable for holding the
  /// specified value type.
  SDOperand CreateStackTemporary(MVT::ValueType VT);
  
  /// FoldSetCC - Constant fold a setcc to true or false.
  SDOperand FoldSetCC(MVT::ValueType VT, SDOperand N1,
                      SDOperand N2, ISD::CondCode Cond);
  
  /// SignBitIsZero - Return true if the sign bit of Op is known to be zero.  We
  /// use this predicate to simplify operations downstream.
  bool SignBitIsZero(SDOperand Op, unsigned Depth = 0) const;

  /// MaskedValueIsZero - Return true if 'Op & Mask' is known to be zero.  We
  /// use this predicate to simplify operations downstream.  Op and Mask are
  /// known to be the same type.
  bool MaskedValueIsZero(SDOperand Op, const APInt &Mask, unsigned Depth = 0)
    const;
  
  /// ComputeMaskedBits - Determine which of the bits specified in Mask are
  /// known to be either zero or one and return them in the KnownZero/KnownOne
  /// bitsets.  This code only analyzes bits in Mask, in order to short-circuit
  /// processing.  Targets can implement the computeMaskedBitsForTargetNode 
  /// method in the TargetLowering class to allow target nodes to be understood.
  void ComputeMaskedBits(SDOperand Op, const APInt &Mask, APInt &KnownZero,
                         APInt &KnownOne, unsigned Depth = 0) const;

  /// ComputeNumSignBits - Return the number of times the sign bit of the
  /// register is replicated into the other bits.  We know that at least 1 bit
  /// is always equal to the sign bit (itself), but other cases can give us
  /// information.  For example, immediately after an "SRA X, 2", we know that
  /// the top 3 bits are all equal to each other, so we return 3.  Targets can
  /// implement the ComputeNumSignBitsForTarget method in the TargetLowering
  /// class to allow target nodes to be understood.
  unsigned ComputeNumSignBits(SDOperand Op, unsigned Depth = 0) const;

  /// isVerifiedDebugInfoDesc - Returns true if the specified SDOperand has
  /// been verified as a debug information descriptor.
  bool isVerifiedDebugInfoDesc(SDOperand Op) const;
  
private:
  void RemoveNodeFromCSEMaps(SDNode *N);
  SDNode *AddNonLeafNodeToCSEMaps(SDNode *N);
  SDNode *FindModifiedNodeSlot(SDNode *N, SDOperand Op, void *&InsertPos);
  SDNode *FindModifiedNodeSlot(SDNode *N, SDOperand Op1, SDOperand Op2,
                               void *&InsertPos);
  SDNode *FindModifiedNodeSlot(SDNode *N, SDOperandPtr Ops, unsigned NumOps,
                               void *&InsertPos);

  void DeleteNodeNotInCSEMaps(SDNode *N);
  
  // List of non-single value types.
  std::list<std::vector<MVT::ValueType> > VTList;
  
  // Maps to auto-CSE operations.
  std::vector<CondCodeSDNode*> CondCodeNodes;

  std::vector<SDNode*> ValueTypeNodes;
  std::map<MVT::ValueType, SDNode*> ExtendedValueTypeNodes;
  std::map<std::string, SDNode*> ExternalSymbols;
  std::map<std::string, SDNode*> TargetExternalSymbols;
  std::map<std::string, StringSDNode*> StringNodes;
};

template <> struct GraphTraits<SelectionDAG*> : public GraphTraits<SDNode*> {
  typedef SelectionDAG::allnodes_iterator nodes_iterator;
  static nodes_iterator nodes_begin(SelectionDAG *G) {
    return G->allnodes_begin();
  }
  static nodes_iterator nodes_end(SelectionDAG *G) {
    return G->allnodes_end();
  }
};

}  // end namespace llvm

#endif
