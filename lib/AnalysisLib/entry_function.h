//
// Created by yu on 6/15/21.
//

#ifndef INC_2021_TEMPLATE_ENTRY_FUNCTION_H
#define INC_2021_TEMPLATE_ENTRY_FUNCTION_H

#include "T_function.h"

namespace sd {
class device_driver;

const static std::string possible_name[] = {
    "probe", "init", "register", "connect", "notifier", "client", "add",
};

const static std::string skip_path[] = {
    "./include",
    "./arch/x86/include",
    "./arch/x86/boot",
    "./arch/x86/kernel/../include",
    "arch/x86/boot",
    "arch/x86/event",
    "lib/",
    "kernel/",
    "./security",
};

class entry_function {
 public:
  entry_function();

  explicit entry_function(T_function *t_function);

  // analysis
 public:
  T_function *t_function{};
  std::vector<checker_result *> all_checker_results;
  // real checker number, e.g. checker_result_number
  std::map<checker_type, std::vector<checker_result *> *> checker_results;

  device_driver *parent = nullptr;
  bool is_init = true;

  std::vector<llvm::Instruction *> current_call_chain;
  std::set<T_function *> function_pointers;
  std::set<T_function *> checked_function;

  // statistic
  // indirect call
  uint64_t number_indirect_call = 0;
  uint64_t number_indirect_call_target_before = 0;
  uint64_t number_indirect_call_target_after = 0;

  bool in_function_pointer = false;

  uint64_t count_update_checker = 0;
  uint64_t count_function_pointer = 0;

  void reset();

  void update_checker_results();

  void update_checker_results(T_function *t_f);

  int64_t get_device_driver_kind();

  int64_t get_checker_results(checker_type ct,
                              std::vector<sd::checker_result *> *crs);

  [[nodiscard]] llvm::Function *get_entry_function() const;

  [[nodiscard]] std::string get_entry_function_name() const;
};
}  // namespace sd

#endif  // INC_2021_TEMPLATE_ENTRY_FUNCTION_H
