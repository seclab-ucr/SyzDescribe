//
// Created by yu on 5/2/21.
//

#ifndef INC_BASIC_H
#define INC_BASIC_H

#define DEBUG_LEVEL (0)
#define ERR 0

#define DELIMITER '$'
#define Annotation_Symbol "# "
#define PREFIX "syz_describe_"

#include <llvm/ADT/SCCIterator.h>
#include <llvm/ADT/SmallSet.h>
#include <llvm/Analysis/CFG.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/SourceMgr.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <utility>

#endif  // INC_BASIC_H
