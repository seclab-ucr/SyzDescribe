//
// Created by yhao on 3/16/21.
//

#ifndef INC_2021_TEMPLATE_T_MODULE_H
#define INC_2021_TEMPLATE_T_MODULE_H

#include "../KnowledgeLib/device_driver.h"
#include "../ToolLib/basic.h"
#include "T_function.h"
#include "entry_function.h"

namespace sd {

// init and exit T_function number
enum init_flag {
  INIT_FLAG_INIT_EXIT = 1 << 1,
  INIT_FLAG_OTHER = 1 << 2,
};

const static std::string module_init_name[]{
    "0",  "1", "1s", "2", "2s", "3", "3s", "4",
    "4s", "5", "5s", "6", "6s", "7", "7s",
};
static std::string module_name = "init_module";

class module_init_order {
 public:
  uint64_t order;
  std::string name;
  std::set<llvm::Function *> *f;
};

// things at LLVM T_module level
class T_module {
 public:
  T_module();

  virtual ~T_module();

  void read_bitcode(const std::string &BitcodeFileName);

  std::string bitcode;
  std::unique_ptr<llvm::Module> llvm_module;
  llvm::CallGraph *cg{};
  std::string work_dir;

  knowledge *k{};
  // T_module:
  std::map<llvm::Function *, T_function *> t_functions;
  std::map<std::string, module_init_order *> module_init_function;
  std::vector<llvm::Function *> init_function;
  std::vector<llvm::Function *> exit_function;
  std::set<llvm::Function *> fp_in_init;

  T_function *find_t_function(llvm::Function *f);

  // check drivers and device in all functions
  void analysis_functions();

  void analysis_functions(T_function *f);

  void update_function();

  void update_fp_in_init();

  // functions for indirect function call:
  // number based indirect function call
  std::unordered_map<llvm::FunctionType *, std::set<T_function *> *>
      map_function_type;

  void type_based_indirect_functions();

  void MLTA_indirect_functions();

  void remove_init_from_indirect();

  int64_t get_callee(llvm::Instruction *inst, std::set<T_function *> *targets);

  int64_t get_callee(llvm::Instruction *inst,
                     std::set<llvm::Function *> *targets,
                     std::set<llvm::Function *> *fp = nullptr);

  int64_t get_callee(llvm::Instruction *inst,
                     std::set<llvm::Function *> *targets,
                     std::set<T_function *> *fp = nullptr);

  int64_t find_ops_structure(const std::string &ops_structure,
                             const std::string &ops_name,
                             llvm::GlobalVariable **ops) const;

  // analysis init&exit functions:
  void find_module_init_function();

  void find_init_and_exit_function();

  int64_t check_callee(T_function *t_f);

  int64_t check_caller(T_function *t_f);

  int64_t check_open(llvm::Function *f, std::vector<checker_result *> *crs);

  std::vector<T_function *> current_call_chain;

  int64_t check_function_pointer(entry_function *init_f);

  int64_t check_function_pointer(entry_function *init_f, T_function *t_f);

  // init and exit T_function number
  std::map<llvm::Function *, enum init_flag> function_flag;

  void set_function_flag(llvm::Function *f, init_flag flag);

  [[maybe_unused]] void get_init_and_exit_function_number();

  // get path number :
  const std::set<std::string> no_path_functions = {"vsnprintf",
                                                   "format_decode"};

  void get_path_number(llvm::Function *f, std::set<llvm::Function *> *call,
                       uint64_t *path_number, uint64_t *b_number);

  void get_path_number(llvm::BasicBlock *b, std::set<llvm::BasicBlock *> *path,
                       uint64_t *path_number);
};

}  // namespace sd

#endif  // INC_2021_TEMPLATE_T_MODULE_H
