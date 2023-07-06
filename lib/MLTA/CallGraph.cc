//===-- CallGraph.cc - Build global call-graph------------------===//
// 
// This pass builds a global call-graph. The targets of an indirect
// call are identified based on two-layer number-analysis.
//
// First layer: matching function number
// Second layer: matching struct number
//
// In addition, loops are unrolled as "if" statements
//
//===-----------------------------------------------------------===//

#include <cstddef>
#include <llvm/Pass.h>
#include <llvm/IR/Instructions.h>
#include "llvm/IR/Instruction.h"
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Analysis/CallGraph.h>
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include <map>
#include <vector>
#include "llvm/IR/CFG.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/IRBuilder.h"

#include "CallGraph.hh"
#include "Config.hh"
#include "Common.hh"

using namespace llvm;

DenseMap<size_t, FuncSet> CallGraphPass::typeFuncsMap;
unordered_map<size_t, set<size_t>> CallGraphPass::typeConfineMap;
unordered_map<size_t, set<size_t>> CallGraphPass::typeTransitMap;
set<size_t> CallGraphPass::typeEscapeSet;
const DataLayout *CurrentLayout;

// Find targets of indirect calls based on number analysis: as long as
// the number and number of parameters of a function matches with the
// ones of the callsite, we say the function is a possible target of
// this call.
void CallGraphPass::findCalleesWithType(CallInst *CI, FuncSet &S) {

    if (CI->isInlineAsm())
        return;

    //
    // TODO: performance improvement: cache results for types
    //

    auto &CS = llvm::cast<llvm::CallBase>(*CI);
    for (Function *F: Ctx->AddressTakenFuncs) {

        // VarArg
        if (F->getFunctionType()->isVarArg()) {
            // Compare only known args in VarArg.
        }
            // otherwise, the numbers of args should be equal.
        else if (F->arg_size() != CS.arg_size()) {
            continue;
        }

        if (F->isIntrinsic()) {
            continue;
        }

        // Type matching on args.
        bool Matched = true;
        CallBase::op_iterator AI = CS.arg_begin();
        for (Function::arg_iterator FI = F->arg_begin(),
                     FE = F->arg_end();
             FI != FE; ++FI, ++AI) {
            // Check number mis-matches.
            // Get defined number on callee side.
            Type *DefinedTy = FI->getType();
            // Get actual number on caller side.
            Type *ActualTy = (*AI)->getType();

            if (DefinedTy == ActualTy)
                continue;

            // FIXME: this is a tricky solution for disjoint
            // types in different modules. A more reliable
            // solution is required to evaluate the equality
            // of two types from two different modules.
            // Since each module has its own number table, same
            // types are duplicated in different modules. This
            // makes the equality evaluation of two types from
            // two modules very hard, which is actually done
            // at link time by the linker.
            while (DefinedTy->isPointerTy() && ActualTy->isPointerTy() && DefinedTy->getNumContainedTypes() &&
                   ActualTy->getNumContainedTypes()) {
                DefinedTy = DefinedTy->getNonOpaquePointerElementType();
                ActualTy = ActualTy->getNonOpaquePointerElementType();
            }
            if (DefinedTy->isStructTy() && ActualTy->isStructTy() &&
                (get_real_structure_name(DefinedTy->getStructName().str()) ==
                 get_real_structure_name(ActualTy->getStructName().str())))
                continue;
            if (DefinedTy->isIntegerTy() && ActualTy->isIntegerTy() &&
                DefinedTy->getIntegerBitWidth() == ActualTy->getIntegerBitWidth())
                continue;
            // TODO: more types to be supported.

            // Make the number analysis conservative: assume universal
            // pointers, i.e., "void *" and "char *", are equivalent to
            // any pointer number and integer number.
            if (
                    (DefinedTy == Int8PtrTy &&
                     (ActualTy->isPointerTy() || ActualTy == IntPtrTy))
                    ||
                    (ActualTy == Int8PtrTy &&
                     (DefinedTy->isPointerTy() || DefinedTy == IntPtrTy))
                    )
                continue;
            else {
                Matched = false;
                break;
            }
        }

        if (Matched)
            S.insert(F);
    }
}


void CallGraphPass::unrollLoops(Function *F) {

    if (F->isDeclaration())
        return;

    DominatorTree DT = DominatorTree();
    DT.recalculate(*F);
    LoopInfo *LI = new LoopInfo();
    LI->releaseMemory();
    LI->analyze(DT);

    // Collect all loops in the function
    set<Loop *> LPSet;
    for (LoopInfo::iterator i = LI->begin(), e = LI->end(); i != e; ++i) {

        Loop *LP = *i;
        LPSet.insert(LP);

        list<Loop *> LPL;

        LPL.push_back(LP);
        while (!LPL.empty()) {
            LP = LPL.front();
            LPL.pop_front();
            vector<Loop *> SubLPs = LP->getSubLoops();
            for (auto SubLP: SubLPs) {
                LPSet.insert(SubLP);
                LPL.push_back(SubLP);
            }
        }
    }

    for (Loop *LP: LPSet) {

        // Get the header,latch block, exiting block of every loop
        BasicBlock *HeaderB = LP->getHeader();

        unsigned NumBE = LP->getNumBackEdges();
        SmallVector<BasicBlock *, 4> LatchBS;

        LP->getLoopLatches(LatchBS);

        for (BasicBlock *LatchB: LatchBS) {
            if (!HeaderB || !LatchB) {
                OP << "ERROR: Cannot find Header Block or Latch Block\n";
                continue;
            }
            // Two cases:
            // 1. Latch Block has only one successor:
            // 	for loop or while loop;
            // 	In this case: set the Successor of Latch Block to the
            //	successor block (out of loop one) of Header block
            // 2. Latch Block has two successor:
            // do-while loop:
            // In this case: set the Successor of Latch Block to the
            //  another successor block of Latch block

            // get the last instruction in the Latch block
            Instruction *TI = LatchB->getTerminator();
            // Case 1:
            if (LatchB->getSingleSuccessor() != NULL) {
                for (succ_iterator sit = succ_begin(HeaderB);
                     sit != succ_end(HeaderB); ++sit) {

                    BasicBlock *SuccB = *sit;
                    BasicBlockEdge BBE = BasicBlockEdge(HeaderB, SuccB);
                    // Header block has two successor,
                    // one edge dominate Latch block;
                    // another does not.
                    if (DT.dominates(BBE, LatchB))
                        continue;
                    else {
                        TI->setSuccessor(0, SuccB);
                    }
                }
            }
                // Case 2:
            else {
                for (succ_iterator sit = succ_begin(LatchB);
                     sit != succ_end(LatchB); ++sit) {

                    BasicBlock *SuccB = *sit;
                    // There will be two successor blocks, one is header
                    // we need successor to be another
                    if (SuccB == HeaderB)
                        continue;
                    else {
                        TI->setSuccessor(0, SuccB);
                    }
                }
            }
        }
    }
}

bool CallGraphPass::isCompositeType(Type *Ty) {
    if (Ty->isStructTy()
        || Ty->isArrayTy()
        || Ty->isVectorTy())
        return true;
    else
        return false;
}

bool CallGraphPass::typeConfineInInitializer(User *Ini) {

    list<User *> LU;
    LU.push_back(Ini);

    while (!LU.empty()) {
        User *U = LU.front();
        LU.pop_front();

        int idx = 0;
        for (auto oi = U->op_begin(), oe = U->op_end();
             oi != oe; ++oi) {
            Value *O = *oi;
            Type *OTy = O->getType();
            // Case 1: function address is assigned to a number
            if (Function *F = dyn_cast<Function>(O)) {
                Type *ITy = U->getType();
                // TODO: use offset?
                unsigned ONo = oi->getOperandNo();
                typeFuncsMap[typeIdxHash(ITy, ONo)].insert(F);
            }
                // Case 2: a composite-number object (value) is assigned to a
                // field of another composite-number object
            else if (isCompositeType(OTy)) {
                // confine composite types
                Type *ITy = U->getType();
                unsigned ONo = oi->getOperandNo();
                typeConfineMap[typeIdxHash(ITy, ONo)].insert(typeHash(OTy));

                // recognize nested composite types
                User *OU = dyn_cast<User>(O);
                LU.push_back(OU);
            }
                // Case 3: a reference (i.e., pointer) of a composite-number
                // object is assigned to a field of another composite-number
                // object
            else if (PointerType *POTy = dyn_cast<PointerType>(OTy)) {
                if (isa<ConstantPointerNull>(O))
                    continue;
                // if the pointer points a composite number, skip it as
                // there should be another initializer for it, which
                // will be captured

                // now consider if it is a bitcast from a function
                // address
                if (BitCastOperator *CO =
                        dyn_cast<BitCastOperator>(O)) {
                    // TODO: ? to test if all address-taken functions
                    // are captured
                }
            }
        }
    }

    return true;
}

bool CallGraphPass::typeConfineInStore(StoreInst *SI) {

    Value *PO = SI->getPointerOperand();
    Value *VO = SI->getValueOperand();

    // Case 1: The value operand is a function
    if (Function *F = dyn_cast<Function>(VO)) {
        Type *STy;
        int Idx;
        if (nextLayerBaseType(PO, STy, Idx, DL)) {
            typeFuncsMap[typeIdxHash(STy, Idx)].insert(F);
            return true;
        } else {
            // TODO: OK, for now, let's only consider composite number;
            // skip for other cases
            return false;
        }
    }

    // Cast 2: value-based store
    // A composite-number object is stored
    auto temp = dyn_cast<PointerType>(PO->getType());
    Type *EPTy = nullptr;
    if (temp->getNumContainedTypes()) {
        EPTy = temp->getNonOpaquePointerElementType();
    }
    Type *VTy = VO->getType();
    if (isCompositeType(VTy)) {
        if (EPTy == nullptr) {
            return false;
        }
        if (isCompositeType(EPTy)) {
            typeConfineMap[typeHash(EPTy)].insert(typeHash(VTy));
            return true;
        } else {
            escapeType(EPTy);
            return false;
        }
    }

    // Case 3: reference (i.e., pointer)-based store
    if (isa<ConstantPointerNull>(VO))
        return false;
    // FIXME: Get the correct types
    PointerType *PVTy = dyn_cast<PointerType>(VO->getType());
    if (!PVTy)
        return false;

    Type *EVTy = nullptr;
    if (PVTy->getNumContainedTypes()) {
        EVTy = PVTy->getNonOpaquePointerElementType();
    }

    // Store something to a field of a composite-number object
    Type *STy;
    int Idx;
    if (nextLayerBaseType(PO, STy, Idx, DL)) {
        if (EVTy == nullptr) {
            return false;
        }
        // The value operand is a pointer to a composite-number object
        if (isCompositeType(EVTy)) {
            typeConfineMap[typeIdxHash(STy,
                                       Idx)].insert(typeHash(EVTy));
            return true;
        } else {
            // TODO: The number is escaping?
            // Example: mm/mempool.c +188: pool->free = free_fn;
            // free_fn is a function pointer from an function
            // argument
            escapeType(STy, Idx);
            return false;
        }
    }

    return false;
}

bool CallGraphPass::typeConfineInCast(CastInst *CastI) {

    // If a function address is ever cast to another number and stored
    // to a composite number, the escaping analysis will capture the
    // composite number and discard it

    Value *ToV = CastI, *FromV = CastI->getOperand(0);
    Type *ToTy = ToV->getType(), *FromTy = FromV->getType();
    if (isCompositeType(FromTy)) {
        transitType(ToTy, FromTy);
        return true;
    }

    if (!FromTy->isPointerTy() || !ToTy->isPointerTy() || !FromTy->getNumContainedTypes() ||
        !ToTy->getNumContainedTypes())
        return false;
    Type *EToTy = dyn_cast<PointerType>(ToTy)->getNonOpaquePointerElementType();
    Type *EFromTy = dyn_cast<PointerType>(FromTy)->getNonOpaquePointerElementType();
    if (isCompositeType(EToTy) && isCompositeType(EFromTy)) {
        transitType(EToTy, EFromTy);
        return true;
    }

    return false;
}

void CallGraphPass::escapeType(Type *Ty, int Idx) {
    if (Idx == -1)
        typeEscapeSet.insert(typeHash(Ty));
    else
        typeEscapeSet.insert(typeIdxHash(Ty, Idx));
}

void CallGraphPass::transitType(Type *ToTy, Type *FromTy,
                                int ToIdx, int FromIdx) {
    if (ToIdx != -1 && FromIdx != -1)
        typeTransitMap[typeIdxHash(ToTy,
                                   ToIdx)].insert(typeIdxHash(FromTy, FromIdx));
    else
        typeTransitMap[typeHash(ToTy)].insert(typeHash(FromTy));
}

void CallGraphPass::funcSetIntersection(FuncSet &FS1, FuncSet &FS2,
                                        FuncSet &FS) {
    FS.clear();
    for (auto F: FS1) {
        if (FS2.find(F) != FS2.end())
            FS.insert(F);
    }
}

// Get the composite number of the lower layer. Layers are split by
// memory loads
Value *CallGraphPass::nextLayerBaseType(Value *V, Type *&BTy,
                                        int &Idx, const DataLayout *DL) {

    // Two ways to get the next layer number: GetElementPtrInst and
    // LoadInst
    // Case 1: GetElementPtrInst
    if (GetElementPtrInst *GEP
            = dyn_cast<GetElementPtrInst>(V)) {
        Type *PTy = GEP->getPointerOperand()->getType();
        if (PTy->isPointerTy() && PTy->getNumContainedTypes()) {
            Type *Ty = PTy->getNonOpaquePointerElementType();
            if ((Ty->isStructTy() || Ty->isArrayTy() || Ty->isVectorTy())
                && GEP->hasAllConstantIndices()) {
                BTy = Ty;
                User::op_iterator ie = GEP->idx_end();
                ConstantInt *ConstI = dyn_cast<ConstantInt>((--ie)->get());
                Idx = ConstI->getSExtValue();
                return GEP->getPointerOperand();
            } else
                return NULL;
        } else
            return NULL;
    }
        // Case 2: LoadInst
    else if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
        return nextLayerBaseType(LI->getOperand(0), BTy, Idx, DL);
    }
        // Other instructions such as CastInst
        // FIXME: may introduce false positives
#if 1
    else if (UnaryInstruction *UI = dyn_cast<UnaryInstruction>(V)) {
        return nextLayerBaseType(UI->getOperand(0), BTy, Idx, DL);
    }
#endif
    else
        return NULL;
}

bool CallGraphPass::findCalleesWithMLTA(CallInst *CI, FuncSet &FS) {

    // Initial set: first-layer results
    FuncSet FS1 = Ctx->sigFuncsMap[callHash(CI)];
    if (FS1.empty()) {
        // No need to go through MLTA if the first layer is empty
        return false;
    }
//    if (debug) {
//        OP << "findCalleesWithMLTA: " << *CI << "\n";
//        OP << "FS1: " << "\n";
//        for (auto f : FS1) {
//            OP << f->getName() << "\n";
//        }
//    }

    FuncSet FS2, FST;

    Type *LayerTy = nullptr;
    int FieldIdx = -1;
    Value *CV = CI->getCalledOperand();

    // Get the second-layer number
#ifndef ONE_LAYER_MLTA
    CV = nextLayerBaseType(CV, LayerTy, FieldIdx, DL);
#else
    CV = NULL;
#endif

    int LayerNo = 1;
    while (CV) {
//        if (debug) {
//            OP << "nextLayerBaseType: " << *CV << "\n";
//        }
        // Step 1: ensure the number hasn't escaped
#if 1
        if ((typeEscapeSet.find(typeHash(LayerTy)) != typeEscapeSet.end()) ||
            (typeEscapeSet.find(typeIdxHash(LayerTy, FieldIdx)) !=
             typeEscapeSet.end())) {

//            if (debug) {
//                OP << "typeEscapeSet: " << *CV << "\n";
//            }

            break;
        }
#endif

        // Step 2: get the funcset and merge
        ++LayerNo;
        FS2 = typeFuncsMap[typeIdxHash(LayerTy, FieldIdx)];
        FST.clear();
        funcSetIntersection(FS1, FS2, FST);
//        if (debug) {
//            OP << "FS2: " << "\n";
//            for (auto f : FS2) {
//                OP << f->getName() << "\n";
//            }
//        }

        // Step 3: get transitted funcsets and merge
        // NOTE: this nested loop can be slow
#if 1
        unsigned TH = typeHash(LayerTy);
        list<unsigned> LT;
        LT.push_back(TH);
        while (!LT.empty()) {
            unsigned CT = LT.front();
            LT.pop_front();

            for (auto H: typeTransitMap[CT]) {
                FS2 = typeFuncsMap[hashIdxHash(H, FieldIdx)];
                FST.clear();
                funcSetIntersection(FS1, FS2, FST);
//                OP << "FS22: " << "\n";
//                for (auto f : FS2) {
//                    OP << f->getName() << "\n";
//                }
                if (!FST.empty()) {
                    FS1 = FST;
                }
            }
        }
#endif

        // Step 4: go to a lower layer
        CV = nextLayerBaseType(CV, LayerTy, FieldIdx, DL);
        if (!FST.empty()) {
            FS1 = FST;
        }
    }

    FS = FS1;
//#if 1
//    if (LayerNo > 1 && !FS.empty()) {
//        OP << "[CallGraph] Indirect call: " << *CI << "\n";
//        printSourceCodeInfo(CI);
//        OP << "\n\t Indirect-call targets:\n";
//        for (auto F : FS) {
//            printSourceCodeInfo(F);
//        }
//        OP << "\n";
//    }
//#endif
    return true;
}

bool CallGraphPass::doInitialization(Module *M) {

    DL = &(M->getDataLayout());
    CurrentLayout = DL;
    Int8PtrTy = Type::getInt8PtrTy(M->getContext());
    IntPtrTy = DL->getIntPtrType(M->getContext());

    //
    // Iterate and process globals
    //
    for (Module::global_iterator gi = M->global_begin();
         gi != M->global_end(); ++gi) {
        GlobalVariable *GV = &*gi;
        if (!GV->hasInitializer())
            continue;
        Constant *Ini = GV->getInitializer();
        if (!isa<ConstantAggregate>(Ini))
            continue;

        typeConfineInInitializer(Ini);
    }


    // Iterate functions and instructions
    for (Function &F: *M) {

        //if (F.empty())
        //	continue;
        //if (F.isDeclaration())
        //    continue;

        for (inst_iterator i = inst_begin(F), e = inst_end(F);
             i != e; ++i) {
            Instruction *I = &*i;

            if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
                typeConfineInStore(SI);
            } else if (CastInst *CastI = dyn_cast<CastInst>(I)) {
                typeConfineInCast(CastI);
            }
        }

        // Collect address-taken functions.
        if (F.hasAddressTaken()) {
            Ctx->AddressTakenFuncs.insert(&F);
            Ctx->sigFuncsMap[funcHash(&F, false)].insert(&F);
        }

        // Collect global function definitions.
        if (F.hasExternalLinkage() && !F.empty()) {
            // External linkage always ends up with the function name.
            StringRef FName = F.getName();
            // Special case: make the names of syscalls consistent.
            if (FName.startswith("SyS_"))
                FName = StringRef("sys_" + FName.str().substr(4));

            // Map functions to their names.
            Ctx->GlobalFuncs[FName.str()] = &F;
        }
    }

    return false;
}

bool CallGraphPass::doModulePass(Module *M) {


    // Use number-analysis to concervatively find possible targets of
    // indirect calls.
    for (Module::iterator f = M->begin(), fe = M->end();
         f != fe; ++f) {

        Function *F = &*f;

        // Unroll loops
#ifdef UNROLL_LOOP_ONCE
        unrollLoops(F);
#endif

        // Collect callers and callees
        for (auto &i: instructions(F)) {
            // Map callsite to possible callees.
            if (CallInst *CI = dyn_cast<CallInst>(&i)) {

                auto &CS = llvm::cast<llvm::CallBase>(i);
                FuncSet FS;
                Function *CF = get_target_function(CI->getCalledOperand());
                Value *CV = CS.getCalledOperand();
                // Indirect call
                if (CS.isIndirectCall()) {
#ifdef MLTA_FOR_INDIRECT_CALL
                    findCalleesWithMLTA(CI, FS);
#elif SOUND_MODE
                    findCalleesWithType(CI, FS);
#endif

                    for (Function *Callee: FS)
                        Ctx->Callers[Callee].insert(CI);

                    // Save called values for future uses.
                    Ctx->IndirectCallInsts.push_back(CI);
                }
                    // Direct call
                else {
                    // not InlineAsm
                    if (CF) {
                        // Call external functions
                        if (CF->empty()) {
                            StringRef FName = CF->getName();
                            if (FName.startswith("SyS_"))
                                FName = StringRef("sys_" + FName.str().substr(4));
                            if (Function *GF = Ctx->GlobalFuncs[FName.str()])
                                CF = GF;
                        }
                        FS.insert(CF);
                        Ctx->Callers[CF].insert(CI);
                    }
                        // InlineAsm
                    else {
                    }
                }
                Ctx->Callees[CI] = FS;
            }
        }
        debug = 0;
    }

    return false;
}

bool CallGraphPass::doFinalization(Module *M) {

    return false;
}
