//
// Created by yu on 6/24/21.
//

#ifndef INC_2021_TEMPLATE_CHECKER_H
#define INC_2021_TEMPLATE_CHECKER_H

#include "../ToolLib/basic.h"
#include "../ToolLib/json.hpp"

#define DEBUG_CHECKER (-1)
#define MAX_FUNC_DEPTH 16
#define MAX_CALLEE_COUNT 100000000

namespace sd {
enum checker_type {
  dev_name = 0,
  dev_type,
  dri_ops,
  dri_type,
  dri_major,
  dri_minor,
  dri_name,
  // for open
  open_indirect,
  // for file of non-open fd
  file_fd,
  file_ops,
  file_install,
};

const static std::string checker_type_name[] = {
    "dev_name",  "dev_type",  "dri_ops",      "dri_type",
    "dri_major", "dri_minor", "dri_name",     "open_indirect",
    "file_fd",   "file_ops",  "file_install",
};

const static std::string syscall_name[]{
    "4-open",
    "4-read",
    "4-write",
    "4-ioctl",
};

class checker {
 public:
  checker_type ct;
  uint64_t id;
  std::string name;

  std::string structure;
  int64_t offset;

  std::string function;
  int64_t object;
  int64_t value;
  int64_t value_offset;
  bool field;

  virtual ~checker();

  virtual checker *copy();

  void copy(checker *c);

  virtual nlohmann::json *to_json();

  virtual void from_json(nlohmann::json j);

  virtual std::string print();
};

class offset_chain_offset {
 public:
  int64_t offset;
  // TODO: later: support symbolic offset
  std::string type_str;
  // TODO: later: try to support llvm number instead of string
  // TODO: later: this->llvm_module->getIdentifiedStructTypes()
  llvm::Type *type;
  offset_chain_offset *next;

  offset_chain_offset();

  virtual ~offset_chain_offset();

  void copy(offset_chain_offset *oc);

  nlohmann::json to_json();

  void read_value_from_json(nlohmann::json j);
};

class offset_chain_base {
 public:
  std::string base;
  std::string type_str;
  llvm::Type *type;
  offset_chain_offset *next;

  offset_chain_base();

  virtual ~offset_chain_base();

  void copy(offset_chain_base *oc);

  nlohmann::json to_json();

  void read_value_from_json(nlohmann::json j);
};

class checker_result : public checker {
 public:
  uint64_t hash;
  llvm::Instruction *inst{};
  std::string inst_strID;
  std::string inst_str;
  int64_t match;

  void setInst(llvm::Instruction *i);

  void setInst(llvm::Module *m);

  offset_chain_base *offset_chain{};
  std::vector<llvm::Instruction *> call_chain;

  // statistic
  bool has_function_pointer = false;

  void push_call_chain(llvm::Instruction *i);

  void pop_call_chain();

  void compute_hash();

  ~checker_result() override;

  checker_result *copy() override;

  void copy(checker_result *c);

  nlohmann::json *to_json() override;

  void from_json(nlohmann::json j) override;

  std::string get_call_chain(const std::string &linux_kernel_version = "");
};

class checker_result_number : public checker_result {
 public:
  int64_t number;

  checker_result_number *copy() override;

  void copy(checker_result_number *c);

  nlohmann::json *to_json() override;

  void from_json(nlohmann::json j) override;
};

class checker_result_string : public checker_result {
 public:
  std::string fmt;
  std::vector<uint64_t> va1;
  std::vector<std::string> va2;

  checker_result_string *copy() override;

  void copy(checker_result_string *c);

  nlohmann::json *to_json() override;

  void from_json(nlohmann::json j) override;

  std::string to_string();

  ~checker_result_string() override;
};

class checker_result_ops : public checker_result {
 public:
  std::string ops_structure;
  std::string ops_name;

  checker_result_ops *copy() override;

  void copy(checker_result_ops *c);

  nlohmann::json *to_json() override;

  void from_json(nlohmann::json j) override;
};

checker_result *make_checker(checker_type ct);

int64_t match_call_chain(checker_result *a, checker_result *b);
}  // namespace sd

#endif  // INC_2021_TEMPLATE_CHECKER_H
