//
// Created by yu on 5/2/21.
//

#ifndef INC_LLVM_RELATED_H
#define INC_LLVM_RELATED_H

#include "basic.h"
#include "log.h"
#include "llvm/IR/Function.h"
#include <cstdint>

llvm::BasicBlock *get_real_basic_block(llvm::BasicBlock *b);

llvm::BasicBlock *get_final_basic_block(llvm::BasicBlock *b);

std::string get_file_name(llvm::Function *f);

std::string get_real_function_name(llvm::Function *f);

std::string get_real_function_type(llvm::FunctionType *f);

std::string get_real_structure_name(const std::string& name);

llvm::Type *get_real_type(llvm::Type *t);

std::string dump_inst(llvm::Instruction *inst);

std::string dump_inst_booltin(llvm::Instruction *inst, std::string version = "v5.12");

std::string dump_function_booltin(llvm::Function *func, std::string version = "v5.12");

std::string real_inst_str(std::string str);

/// Compute the true target of a function call, resolving LLVM aliases
/// and bitcasts.
llvm::Function *get_target_function(llvm::Value *calledVal);

#define yhao_llvm_print(type, empty, out, print, str)   \
    if ((type) >= DEBUG_LEVEL) {                        \
        if ((empty) == 1) {                             \
            (str) = "";                                 \
        }                                               \
        llvm::raw_string_ostream dump(str);             \
        print(dump);                                    \
        if ((out) == 1) {                               \
            yhao_log(type, str);                        \
        }                                               \
    }

#define yhao_add(type, print, str)  yhao_llvm_print(type, 0, 0, print, str)
#define yhao_print(type, print, str)  yhao_llvm_print(type, 1, 0, print, str)
#define yhao_dump(type, print, str)  yhao_llvm_print(type, 1, 1, print, str)
#define yhao_add_dump(type, print, str)  yhao_llvm_print(type, 0, 1, print, str)

// strID: Path-NameFunction-NoBB-NoInst
std::string function_to_strID(llvm::Function *f);

std::string basicblock_to_strID(llvm::BasicBlock *b);

std::string inst_to_strID(llvm::Instruction *inst);

llvm::Function *strID_to_function(llvm::Module *m, const std::string &str);

llvm::BasicBlock *strID_to_basicblock(llvm::Module *m, const std::string &str);

llvm::Instruction *strID_to_inst(llvm::Module *m, const std::string &str);

uint64_t get_loc(llvm::Function *func);

#endif //INC_LLVM_RELATED_H
