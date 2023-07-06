//
// Created by yu on 4/20/21.
//

#ifndef INC_2021_TEMPLATE_KNOWLEDGE_H
#define INC_2021_TEMPLATE_KNOWLEDGE_H

#include "../ToolLib/basic.h"
#include "../ToolLib/json.hpp"
#include "checker.h"

namespace sd {

// -2: null
// -1: return value
// 0~N: arguments

class K_function {
 public:
  // additional: file of function
  std::string file;
  // necessary: name of function
  std::string name;

  // index of the driver or device structure object in argument or return
  // value(-1)
  int64_t object{};
  // whether object is the field of the driver or device structure
  // e.g. func(driver->var) driver is the object
  bool field{};

  // index of value in arguments or return value(-1)
  int64_t value{};
  // (optical) if the value is a field of the argument, the index is variable
  // offset e.g. func(arg): arg->var is the value
  int64_t offset{};
  // whether it is printf()
  bool fmt{};

  K_function();

  explicit K_function(const nlohmann::json &j);

  nlohmann::json to_json();
};

class K_variable {
 public:
  // necessary: index of the variable in the driver or device structure
  int64_t offset{};
  // related function
  std::vector<K_function *> function;
  // optical: name of ops structure
  std::string structure;
  // index of the functions
  std::map<std::string, int64_t> functions_index;

  K_variable();

  explicit K_variable(const nlohmann::json &j);

  virtual ~K_variable();

  nlohmann::json to_json();
};

class K_base {
 public:
  // name model
  std::string name;
  // necessary: the driver or device structure
  std::string structure;
  // variable inside the driver or device structure
  // e.g. major, minor, ops, name
  std::map<std::string, K_variable *> variable;

  nlohmann::json j;

  K_base();

  virtual ~K_base();

  void read(const nlohmann::json &json);

  virtual int64_t find_k_variable(const std::string &structure1, int64_t offset,
                                  K_variable **k);

  virtual nlohmann::json to_json();
};

class K_device : public K_base {
 public:
  K_variable *dev_name{};
  K_variable *dev_type{};

  K_device();

  explicit K_device(const nlohmann::json &json);

  nlohmann::json to_json() override;
};

class K_driver : public K_base {
 public:
  // additional
  K_variable *dri_ops{};
  // optical
  K_variable *dri_type{};
  K_variable *dri_major{};
  K_variable *dri_minor{};
  K_variable *dri_name{};

  // necessary: the caller of the ops structure
  std::string key_dereference_obj_name;
  // the index of open function
  int64_t key_dereference_obj_open{};
  // optical: children drivers
  std::vector<K_driver *> sub;

  // if it is a child driver, it has the major number and parent
  int64_t major_value{};
  K_driver *parent{};

  K_driver();

  explicit K_driver(const nlohmann::json &json);

  ~K_driver() override;

  nlohmann::json to_json() override;

  int64_t find_k_variable(const std::string &structure1, int64_t offset,
                          K_variable **k) override;
};

class K_file : public K_base {
 public:
  K_variable *file_fd{};
  K_variable *file_ops{};
  K_variable *file_install{};

  K_file();

  explicit K_file(const nlohmann::json &json);

  nlohmann::json to_json() override;
};

class knowledge {
 public:
  nlohmann::json knowledge_json;
  // knowledge with structure
  K_device *device{};
  std::vector<K_driver *> driver;
  K_file *file{};

  // knowledge items
  std::vector<checker *> checkers;
  std::map<std::string, std::set<checker *> *> inst_store;
  std::map<std::string, std::set<checker *> *> inst_call;

  bool new_knowledge{};

 public:
  knowledge();

  void read(const std::string &knowledge);

  void print(const std::string &path);

  void make_checker(checker_type type, K_base *kb, K_variable *kv,
                    uint64_t id = 0);

  void make_driver_checker(const std::vector<K_driver *> &drivers);

  void make_checker();

  void update_checker();

  int64_t add_knowledge(sd::checker_result *cr,
                        std::string key_dereference_obj_name,
                        int64_t key_dereference_obj_open);

  int64_t find_k_variable(const std::string &structure, int64_t offset,
                          K_variable **k);
};
}  // namespace sd

#endif  // INC_2021_TEMPLATE_KNOWLEDGE_H
