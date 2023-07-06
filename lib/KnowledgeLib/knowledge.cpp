//
// Created by yu on 4/20/21.
//

#include "knowledge.h"

#include "../ToolLib/log.h"

sd::K_function::K_function() = default;

sd::K_function::K_function(const nlohmann::json &j) {
  if (j.contains("1-file") && j["1-file"].is_string()) {
    this->file = j["1-file"].get<std::string>();
  } else {
    yhao_log(0, "1-file");
  }
  if (j.contains("2-name") && j["2-name"].is_string()) {
    this->name = j["2-name"].get<std::string>();
  } else {
    yhao_log(3, "2-name");
  }

  if (j.contains("4-object") && j["4-object"].is_number_integer()) {
    this->object = j["4-object"].get<int64_t>();
  } else {
    this->object = -2;
    yhao_log(0, "4-object");
  }
  if (j.contains("5-field") && j["5-field"].is_boolean()) {
    this->field = j["5-field"].get<bool>();
  } else {
    yhao_log(0, "K_function: 5-field");
  }

  if (j.contains("6-value") && j["6-value"].is_number_integer()) {
    this->value = j["6-value"].get<int64_t>();
  } else {
    this->value = -2;
    yhao_log(3, "6-value");
  }
  if (j.contains("7-offset") && j["7-offset"].is_number_integer()) {
    this->offset = j["7-offset"].get<int64_t>();
  } else {
    this->offset = -2;
    yhao_log(0, "K_function: 7-offset");
  }
  if (j.contains("8-fmt") && j["8-fmt"].is_boolean()) {
    this->fmt = j["8-fmt"].get<bool>();
  } else {
    yhao_log(0, "K_function: 8-fmt");
  }
}

nlohmann::json sd::K_function::to_json() {
  auto *j = new nlohmann::json();
  (*j)["1-file"] = this->file;
  (*j)["2-name"] = this->name;

  (*j)["4-object"] = this->object;
  (*j)["5-field"] = this->field;

  (*j)["6-value"] = this->value;
  (*j)["7-offset"] = this->offset;
  (*j)["8-fmt"] = this->fmt;
  return *j;
}

sd::K_variable::K_variable() = default;

sd::K_variable::K_variable(const nlohmann::json &j) {
  std::string str;

  if (j.contains("1-offset") && j["1-offset"].is_number_integer()) {
    this->offset = j["1-offset"].get<int64_t>();
  } else {
    this->offset = -2;
    yhao_log(0, "1-offset");
  }
  if (j.contains("2-function")) {
    if (j["2-function"].is_array()) {
      for (const auto &f : j["2-function"]) {
        this->function.push_back(new K_function(f));
      }
    } else {
      yhao_log(0, "2-function: \n" + j.dump(2));
    }
  }

  if (j.contains("3-structure") && j["3-structure"].is_string()) {
    this->structure = j["3-structure"].get<std::string>();
  } else {
    yhao_log(0, "K_variable: 3-structure");
  }

  for (auto temp : syscall_name) {
    if (j.contains(temp) && j[temp].is_number_integer()) {
      this->functions_index[temp] = j[temp].get<int64_t>();
    } else {
      this->functions_index[temp] = -2;
      yhao_log(0, "not find K_variable: " + str);
    }
  }
}

nlohmann::json sd::K_variable::to_json() {
  auto *j = new nlohmann::json();
  (*j)["1-offset"] = this->offset;
  for (auto f : this->function) {
    (*j)["2-function"].push_back(f->to_json());
  }
  (*j)["3-structure"] = this->structure;
  for (const auto &temp : syscall_name) {
    (*j)[temp] = this->functions_index[temp];
  }
  return *j;
}

sd::K_variable::~K_variable() {
  for (auto temp : function) {
    delete temp;
  }
};

int64_t sd::K_base::find_k_variable(const std::string &structure1,
                                    int64_t offset, sd::K_variable **k) {
  if (structure1 == this->structure) {
    for (const auto &v : variable) {
      if (v.second != nullptr && offset == v.second->offset) {
        *k = v.second;
        return 0;
      }
    }
  }
  return 1;
}

sd::K_base::K_base() = default;

void sd::K_base::read(const nlohmann::json &json) {
  std::string key;

  key = "1-name";
  if (json.contains(key) && json[key].is_string()) {
    this->name = json[key].get<std::string>();
  } else {
    yhao_log(0, key);
  }

  key = "2-structure";
  if (json.contains(key) && json[key].is_string()) {
    this->structure = json[key].get<std::string>();
  } else {
    yhao_log(3, key);
  }
}

sd::K_base::~K_base() {
  for (const auto &temp : variable) {
    delete temp.second;
  }
}

nlohmann::json sd::K_base::to_json() {
  j.clear();
  j["1-name"] = this->name;
  j["2-structure"] = this->structure;
  for (auto v : variable) {
    if (v.second != nullptr) {
      (j)[v.first] = v.second->to_json();
    }
  }
  return this->j;
}

sd::K_device::K_device() = default;

sd::K_device::K_device(const nlohmann::json &json) {
  K_base::read(json);
  std::string key;

  key = "3-dev_name";
  if (json.contains(key) && json[key].is_object()) {
    this->dev_name = new K_variable(json[key]);
    this->variable[key] = this->dev_name;
  } else {
    yhao_log(3, key);
  }

  key = "4-dev_type";
  if (json.contains(key) && json[key].is_object()) {
    this->dev_type = new K_variable(json[key]);
    this->variable[key] = this->dev_type;
  } else {
    yhao_log(3, key);
  }
}

nlohmann::json sd::K_device::to_json() {
  K_base::to_json();
  return j;
}

sd::K_driver::K_driver() = default;

sd::K_driver::K_driver(const nlohmann::json &json) {
  K_base::read(json);
  std::string key;

  key = "0-major_value";
  if (json.contains(key) && json[key].is_number_integer()) {
    this->major_value = json[key].get<int64_t>();
  } else {
    this->major_value = -2;
    yhao_log(0, key);
  }

  key = "3-dri_ops";
  if (json.contains(key) && json[key].is_object()) {
    this->dri_ops = new K_variable(json[key]);
    this->variable[key] = this->dri_ops;
  } else {
    yhao_log(3, key + ": \n" + json.dump(2));
  }

  key = "4-dri_type";
  if (json.contains(key)) {
    if (json[key].is_object()) {
      this->dri_type = new K_variable(json[key]);
      this->variable[key] = this->dri_type;
    } else {
      yhao_log(0, key);
    }
  }

  key = "5-dri_major";
  if (json.contains(key)) {
    if (json[key].is_object()) {
      this->dri_major = new K_variable(json[key]);
      this->variable[key] = this->dri_major;
    } else {
      yhao_log(0, key);
    }
  }

  key = "6-dri_minor";
  if (json.contains(key)) {
    if (json[key].is_object()) {
      this->dri_minor = new K_variable(json[key]);
      this->variable[key] = this->dri_minor;
    } else {
      yhao_log(0, key);
    }
  }

  key = "7-dri_name";
  if (json.contains(key)) {
    if (json[key].is_object()) {
      this->dri_name = new K_variable(json[key]);
      this->variable[key] = this->dri_name;
    } else {
      yhao_log(0, key);
    }
  }

  key = "8-key_dereference_obj";
  if (json.contains(key) && json[key].is_object()) {
    auto k = json[key];
    std::string temp;
    temp = "1-name";
    if (k.contains(temp) && k[temp].is_string()) {
      this->key_dereference_obj_name = k[temp].get<std::string>();
    } else {
      yhao_log(3, temp);
    }
    temp = "2-open";
    if (k.contains(temp) && k[temp].is_number_integer()) {
      this->key_dereference_obj_open = k[temp].get<std::int64_t>();
    } else {
      yhao_log(3, k.dump());
      yhao_log(3, temp);
    }
  } else {
    yhao_log(3, key);
  }

  key = "9-sub";
  if (json.contains(key)) {
    if (json[key].is_array()) {
      for (const auto &f : json[key]) {
        auto s = new K_driver(f);
        s->parent = this;
        this->sub.push_back(s);
      }
    } else {
      yhao_log(0, key + ": \n" + json.dump(2));
    }
  }
}

nlohmann::json sd::K_driver::to_json() {
  K_base::to_json();
  j["0-major_value"] = this->major_value;
  j["8-key_dereference_obj"]["1-name"] = this->key_dereference_obj_name;
  j["8-key_dereference_obj"]["2-open"] = this->key_dereference_obj_open;
  for (auto s : this->sub) {
    j["9-sub"].push_back(s->to_json());
  }
  return j;
}

int64_t sd::K_driver::find_k_variable(const std::string &structure1,
                                      int64_t offset, sd::K_variable **k) {
  int64_t ret;
  ret = K_base::find_k_variable(structure1, offset, k);
  if (ret == 0) {
    return 0;
  }
  for (auto sd : this->sub) {
    ret = sd->find_k_variable(structure1, offset, k);
    if (ret == 0) {
      return 0;
    }
  }
  return 1;
}

sd::K_driver::~K_driver() {
  for (auto sd : sub) {
    delete sd;
  }
}

sd::K_file::K_file() = default;

sd::K_file::K_file(const nlohmann::json &json) {
  K_base::read(json);
  std::string key;

  key = "3-fd";
  if (json.contains(key) && json[key].is_object()) {
    this->file_fd = new K_variable(json[key]);
    this->variable[key] = this->file_fd;
  } else {
    yhao_log(3, key + ": \n" + json.dump(2));
  }

  key = "4-file_ops";
  if (json.contains(key) && json[key].is_object()) {
    this->file_ops = new K_variable(json[key]);
    this->variable[key] = this->file_ops;
  } else {
    yhao_log(3, key + ": \n" + json.dump(2));
  }

  key = "5-install";
  if (json.contains(key) && json[key].is_object()) {
    this->file_install = new K_variable(json[key]);
    this->variable[key] = this->file_install;
  } else {
    yhao_log(3, key + ": \n" + json.dump(2));
  }
}

nlohmann::json sd::K_file::to_json() {
  K_base::to_json();
  return j;
}

sd::knowledge::knowledge() = default;

void sd::knowledge::read(const std::string &knowledge) {
  std::ifstream json_ifstream(knowledge);
  if (json_ifstream.is_open()) {
    json_ifstream >> knowledge_json;
    json_ifstream.close();
  } else {
    yhao_log(3, "open k json file fail: " + knowledge);
    exit(0);
  }
  if (knowledge_json.contains("device") &&
      knowledge_json["device"].is_object()) {
    this->device = new K_device(knowledge_json["device"]);
  } else {
    yhao_log(3, "knowledge_json.contains(device)");
  }
  if (knowledge_json.contains("driver") &&
      knowledge_json["driver"].is_array()) {
    for (const auto &d : knowledge_json["driver"]) {
      this->driver.push_back(new K_driver(d));
    }
  } else {
    yhao_log(3, "knowledge_json.contains(device)");
  }
  if (knowledge_json.contains("file") && knowledge_json["file"].is_object()) {
    this->file = new K_file(knowledge_json["file"]);
  } else {
    yhao_log(3, "knowledge_json.contains(file)");
  }
}

void sd::knowledge::print(const std::string &path) {
  nlohmann::json j;
  if (this->device != nullptr) {
    j["device"] = this->device->to_json();
  }
  for (auto d : this->driver) {
    j["driver"].push_back(d->to_json());
  }
  if (this->file != nullptr) {
    j["file"] = this->file->to_json();
  }

  std::ofstream json_ofstream(path);
  json_ofstream << j.dump(2);
  json_ofstream.close();
}

void sd::knowledge::make_checker(checker_type type, K_base *kb, K_variable *kv,
                                 uint64_t id) {
  if (kv == nullptr) return;

  if (kv->offset >= 0) {
    auto c = new checker();
    c->ct = type;
    c->id = id;
    c->name = kb->name;
    c->structure = kb->structure;
    c->offset = kv->offset;

    this->checkers.push_back(c);
    if (this->inst_store.find(kb->structure) == this->inst_store.end()) {
      this->inst_store[kb->structure] = new std::set<checker *>;
    }
    this->inst_store[kb->structure]->insert(c);
  }
  for (auto f : kv->function) {
    auto c = new checker();
    c->ct = type;
    c->id = id;
    c->name = kb->name;
    c->structure = kb->structure;
    c->offset = kv->offset;

    c->function = f->name;
    c->object = f->object;
    c->value = f->value;
    c->value_offset = f->offset;
    c->field = f->field;

    this->checkers.push_back(c);
    if (this->inst_call.find(f->name) == this->inst_call.end()) {
      this->inst_call[f->name] = new std::set<checker *>;
    }
    this->inst_call[f->name]->insert(c);
  }
}

void sd::knowledge::make_driver_checker(
    const std::vector<K_driver *> &drivers) {
  for (auto d : drivers) {
    uint64_t id = 0;
    if (d->dri_name == nullptr) {
    } else {
      std::string str = d->structure;
      std::hash<std::string> str_hash;
      id = str_hash(str);
    }
    make_checker(checker_type::dri_ops, d, d->dri_ops, id);
    make_checker(checker_type::dri_type, d, d->dri_type);
    make_checker(checker_type::dri_major, d, d->dri_major);
    make_checker(checker_type::dri_minor, d, d->dri_minor);
    make_checker(checker_type::dri_name, d, d->dri_name, id);
    make_driver_checker(d->sub);
  }
}

void sd::knowledge::make_checker() {
  // for device
  make_checker(checker_type::dev_name, device, device->dev_name);
  make_checker(checker_type::dev_type, device, device->dev_type);

  // for driver
  make_driver_checker(this->driver);

  // for file
  make_checker(checker_type::file_fd, file, file->file_fd);
  make_checker(checker_type::file_ops, file, file->file_ops);
  make_checker(checker_type::file_install, file, file->file_install);

  for (auto temp : checkers) {
    yhao_log(DEBUG_CHECKER, "checker: " + temp->print());
  }
}

void sd::knowledge::update_checker() {}

int64_t sd::knowledge::add_knowledge(sd::checker_result *cr,
                                     std::string key_dereference_obj_name,
                                     int64_t key_dereference_obj_open) {
  if (cr->ct != checker_type::open_indirect) {
    return 1;
  }

  auto oc = cr->offset_chain;
  if (oc == nullptr) {
    yhao_log(3, "offset chain bug: " + cr->print());
    return 1;
  }
  auto temp = oc->next;
  if (temp == nullptr || temp->next == nullptr) {
    yhao_log(3, "offset chain bug: " + cr->print());
    return 1;
  }
  std::string structure = oc->type_str;
  int64_t offset = temp->offset;
  std::string ops_structure = temp->type_str;
  int64_t open_offset = temp->next->offset;

  while (temp->next->next != nullptr) {
    structure = temp->type_str;
    offset = temp->next->offset;
    ops_structure = temp->next->type_str;
    open_offset = temp->next->next->offset;
    temp = temp->next;
  }

  auto new_kd = new K_driver();
  new_kd->structure = structure;
  auto new_kv = new K_variable();
  new_kv->offset = offset;
  new_kv->structure = ops_structure;
  new_kv->functions_index["4-open"] = open_offset;
  new_kd->dri_ops = new_kv;
  new_kd->variable["dri_ops"] = new_kv;
  new_kd->key_dereference_obj_name = std::move(key_dereference_obj_name);
  new_kd->key_dereference_obj_open = key_dereference_obj_open;

  // not support: sub driver: major_value, dri_minor

  K_driver *kd;
  // typically, there is only one layer sub
  structure = cr->structure;
  for (auto d : this->driver) {
    if (d->structure == structure) {
      kd = d;
      goto next;
    }
    for (auto sd : d->sub) {
      if (sd->structure == structure) {
        kd = sd;
        goto next;
      }
    }
  }
  goto error;
next:
  for (auto sub : kd->sub) {
    if (sub->structure == new_kd->structure) {
      yhao_log(1, "existing k");
      goto out;
    }
  }
  new_kd->parent = kd;
  kd->sub.push_back(new_kd);
  make_checker(checker_type::dri_ops, new_kd, new_kd->dri_ops);
  new_knowledge = true;
  yhao_log(1, "add new k");
  return 0;

error:
  yhao_log(3, "not find parent driver: " + structure);
out:
  delete new_kd;
  return 1;
}

int64_t sd::knowledge::find_k_variable(const std::string &structure,
                                       int64_t offset, sd::K_variable **k) {
  uint64_t ret;
  ret = this->device->find_k_variable(structure, offset, k);
  if (ret == 0) {
    return 0;
  }
  ret = this->file->find_k_variable(structure, offset, k);
  if (ret == 0) {
    return 0;
  }
  for (auto d : this->driver) {
    ret = d->find_k_variable(structure, offset, k);
    if (ret == 0) {
      return 0;
    }
  }
  yhao_log(3,
           "not find K_variable: " + structure + ": " + std::to_string(offset));
  return 1;
}
