//
// Created by yu on 4/18/21.
//

#ifndef INC_2021_TEMPLATE_MANAGER_H
#define INC_2021_TEMPLATE_MANAGER_H

#include "../AnalysisLib/T_module.h"
#include "../KnowledgeLib/device_driver.h"
#include "../KnowledgeLib/knowledge.h"
#include "../KnowledgeLib/syzlang.h"
#include "../KnowledgeLib/task.h"
#include "../ToolLib/json.hpp"

namespace sd {
class Manager {
 public:
  nlohmann::json config_json;
  T_module *t_module;
  knowledge *k;
  syzlang *s;

  std::string bitcode;
  std::string work_dir;
  // analysis tool
  std::string template_klee;

  // all new entry function
  std::vector<entry_function *> all_new_entry_functions;

  // module init functions
  std::map<std::string, entry_function *> init_function;

  std::map<std::string, entry_function *> init_function_with_driver;
  std::map<std::string, entry_function *> init_function_with_device;
  std::map<std::string, entry_function *> init_function_with_driver_device;

  std::vector<device_driver *> all_device_drivers;
  std::map<std::string, device_driver *> device_drivers;

  // statistic
  uint64_t number_module_init_function = 0;
  uint64_t number_driver = 0;
  uint64_t number_ioctl = 0;

  // vs dynamic

  // function pointer
  uint64_t number_function_pointer = 0;
  uint64_t number_indirect_call = 0;
  uint64_t number_indirect_call_target_before = 0;
  uint64_t number_indirect_call_target_after = 0;

  // analysis time
  time_t start, end;
  double analysis_time_module_init_function = 0;
  double analysis_time_ioctl = 0;

 public:
  Manager();

  void setup(const std::string &config);

  void analysis();

  void analysis_ops_and_file_name();

  void collect_info(device_driver *dd);

  void collect_entry(device_driver *dd) const;

  void collect_cmd_and_type(device_driver *dd);

  static void collect_entry_function(device_driver *dd,
                                     std::set<entry_function *> &entry_set);

  // 1. use module init function as entry point
  // 2. classify the functions based the number of driver and device
  void analysis_init_function();

  // 1. get the value
  // 2. generate new k
  int64_t handle_init_function_with_driver();

  // check the function with new k
  int64_t handle_init_function_with_device();

  // 1. get the value
  // 2. generate template
  int64_t handle_init_function_with_driver_device();

  void mapping_checker_results(entry_function *ef,
                               std::vector<device_driver *> &results);

  void check_cmd_and_type(const std::string &function_name,
                          nlohmann::json *result_json,
                          std::set<llvm::Function *> *fps) const;

  // find ops structure from give sd::checker_result_ops *cr0
  // and find function by int64_t index
  // and return function to llvm::Function **fp
  int64_t get_function_by_index(sd::checker_result_ops *cr0, int64_t index,
                                llvm::Function **fp) const;

  int64_t get_function_by_syscall(sd::checker_result_ops *cr0,
                                  std::string syscall, int64_t *index,
                                  llvm::Function **fp) const;

  // make a task_str based on give information
  // use default information from Class Manager
  [[nodiscard]] task *make_task(const std::string &entry_function,
                                checker_result *cr,
                                const std::string &task_bitcode = "",
                                const std::string &task_work_dir = "") const;

  void execute_init_function(std::vector<device_driver *> &dds) const;

  // execute the task_str and get the results
  static int64_t execute_task(task *t);

  int64_t execute_task_by_klee(task *t) const;

  llvm::Value *get_real_value(llvm::Value *v);
};
}  // namespace sd

#endif  // INC_2021_TEMPLATE_MANAGER_H
