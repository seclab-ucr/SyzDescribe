#ifndef ANALYZER_GLOBAL_H
#define ANALYZER_GLOBAL_H

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include "llvm/Support/CommandLine.h"
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "Common.hh"

// 
// typedefs
//
typedef vector<pair<llvm::Module *, llvm::StringRef> > ModuleList;
// Mapping module to its file name.
typedef unordered_map<llvm::Module *, llvm::StringRef> ModuleNameMap;
// The set of all functions.
typedef llvm::SmallPtrSet<llvm::Function *, 8> FuncSet;
// Mapping from function name to function.
typedef unordered_map<string, llvm::Function *> NameFuncMap;
typedef llvm::SmallPtrSet<llvm::CallInst *, 8> CallInstSet;
typedef DenseMap<Function *, CallInstSet> CallerMap;
typedef DenseMap<CallInst *, FuncSet> CalleeMap;
// Pointer analysis types.
typedef DenseMap<Value *, SmallPtrSet<Value *, 16>> PointerAnalysisMap;
typedef unordered_map<Function *, PointerAnalysisMap> FuncPointerAnalysisMap;
typedef unordered_map<Function *, AAResults *> FuncAAResultsMap;
typedef map<Type *, string> TypeNameMap;

struct GlobalContext {

    // Map global types to type_name
    TypeNameMap GlobalTypes;

    // Map global function name to function.
    NameFuncMap GlobalFuncs;

    // Functions whose addresses are taken.
    FuncSet AddressTakenFuncs;

    // Map a callsite to all potential callee functions.
    CalleeMap Callees;

    // Map a function to all potential caller instructions.
    CallerMap Callers;

    // Indirect call instructions.
    std::vector<CallInst *> IndirectCallInsts;

    // Map function signature to functions
    DenseMap<size_t, FuncSet> sigFuncsMap;

    // Modules.
    ModuleList Modules;
    ModuleNameMap ModuleMaps;
    set<string> InvolvedModules;
};

class IterativeModulePass {
protected:
    GlobalContext *Ctx;
    const char *ID;
public:
    IterativeModulePass(GlobalContext *Ctx_, const char *ID_)
            : Ctx(Ctx_), ID(ID_) {}

    // Run on each module before iterative pass.
    virtual bool doInitialization(llvm::Module *M) { return true; }

    // Run on each module after iterative pass.
    virtual bool doFinalization(llvm::Module *M) { return true; }

    // Iterative pass.
    virtual bool doModulePass(llvm::Module *M) { return false; }

    virtual void run(ModuleList &modules);
};

#endif
