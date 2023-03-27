//===-- Analyzer.cc - the kernel-analysis framework--------------===//
//
// This file implements the analysis framework. It calls the pass for
// building call-graph and the pass for finding security checks.
//
// ===-----------------------------------------------------------===//

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/Path.h"

#include <memory>
#include <vector>
#include <sstream>
#include <sys/resource.h>

#include "Analyzer.hh"
#include "CallGraph.hh"
#include "Config.hh"

using namespace llvm;

void IterativeModulePass::run(ModuleList &modules) {

    ModuleList::iterator i, e;
//    OP << "[" << ID << "] Initializing " << modules.size() << " modules ";
    bool again = true;
    while (again) {
        again = false;
        for (i = modules.begin(), e = modules.end(); i != e; ++i) {
            again |= doInitialization(i->first);
//            OP << ".";
        }
    }
//    OP << "\n";

    unsigned iter = 0, changed = 1;
    while (changed) {
        ++iter;
        changed = 0;
        unsigned counter_modules = 0;
        unsigned total_modules = modules.size();
        for (i = modules.begin(), e = modules.end(); i != e; ++i) {
//            OP << "[" << ID << " / " << iter << "] ";
//            OP << "[" << ++counter_modules << " / " << total_modules << "] ";
//            OP << "[" << i->second << "]\n";

            bool ret = doModulePass(i->first);
            if (ret) {
                ++changed;
//                OP << "\t [CHANGED]\n";
            } else {
//                OP << "\n";
            }
        }
//        OP << "[" << ID << "] Updated in " << changed << " modules.\n";
    }

//    OP << "[" << ID << "] Postprocessing ...\n";
    again = true;
    while (again) {
        again = false;
        for (i = modules.begin(), e = modules.end(); i != e; ++i) {
            // TODO: Dump the results.
            again |= doFinalization(i->first);
        }
    }

//    OP << "[" << ID << "] Done!\n\n";
}

