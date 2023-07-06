//
// Created by yuhao on 2/5/22.
//

#ifndef INC_2021_TEMPLATE_IOCTL_CMD_TYPE_H
#define INC_2021_TEMPLATE_IOCTL_CMD_TYPE_H

#include "../KnowledgeLib/device_driver.h"
#include "T_module.h"

namespace sd {
class ioctl_cmd_type {
 public:
  T_module *tm = nullptr;
  std::set<llvm::Function *> *fps{};

  device_driver *dd;
  cmd_info *current_cmd;

  llvm::Function *target_function;
  llvm::DominatorTree *DT = nullptr;
  std::vector<llvm::CallInst *> call_stack;
  bool only_type = false;
  llvm::Value *curr_cmd = nullptr;
  uint64_t curr_func_depth = 0;
  uint64_t count_callee = 0;
  ioctl_cmd_type *caller = nullptr;

  // All instructions whose result depend on the value
  // which is derived from cmd and number arg value.
  std::set<llvm::Value *> all_cmd_inst;
  std::set<llvm::Value *> all_type_inst;
  std::set<llvm::BasicBlock *> visit_BBs;
  // map that stores the actual values passed by the caller,
  // callee <-> caller
  // this is useful in determining the number of argument.
  std::map<llvm::Value *, llvm::Value *> caller_args;

  // statistic
  bool in_indirect_call = false;

 public:
  ioctl_cmd_type(llvm::Function *tf, std::set<uint64_t> cmd,
                 std::set<uint64_t> type, device_driver *dd,
                 T_module *tm = nullptr,
                 std::set<llvm::Function *> *fps = nullptr);

  ioctl_cmd_type(llvm::Function *tf, std::set<uint64_t> cmd,
                 std::set<uint64_t> type, sd::ioctl_cmd_type *caller,
                 std::map<uint64_t, llvm::Value *> caller_arguments);

  virtual ~ioctl_cmd_type();

  void set_only_type_module();

  void unset_only_type_module();

  void run_cmd();

  void run_type(llvm::BasicBlock *start_bb);

  bool is_cmd_affected(llvm::Value *targetValue);

  bool is_type_affected(llvm::Value *targetValue);

  void update_cmd_and_type();

  void check_call_cmd(llvm::CallInst *ci);

  void check_callee(llvm::CallInst *ci, llvm::Function *tf);

  // for number
  void check_call_type(llvm::CallInst *ci);

  void get_dom_bb(llvm::BasicBlock *bb, std::set<llvm::BasicBlock *> *temp);

  void get_arg_propagation(llvm::CallInst *ci, std::set<uint64_t> &cmd,
                           std::set<uint64_t> &type,
                           std::map<uint64_t, llvm::Value *> &caller_arg);

  static llvm::Type *typeOutputHandler(llvm::Value *val,
                                       ioctl_cmd_type *currFunc);

  static llvm::Type *get_type_recursive(llvm::Value *val,
                                        std::set<llvm::Instruction *> &visited);

  static llvm::Value *get_inst_recursive(
      llvm::Value *val, std::set<llvm::Instruction *> &visited);
};
}  // namespace sd

#endif  // INC_2021_TEMPLATE_IOCTL_CMD_TYPE_H
