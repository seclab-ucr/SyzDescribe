//
// Created by yu on 6/19/21.
//

#include "task.h"

#include "../ToolLib/log.h"

sd::task::task(nlohmann::json j) { this->from_json(std::move(j)); }

sd::task::task(const std::string &entryFunction, sd::checker_result *cr) {
  this->entry_function = entryFunction;
  this->cr = cr;
  compute_hash();
}

nlohmann::json *sd::task::to_json() {
  auto j = new nlohmann::json();
  (*j)["0_hash"] = this->hash;
  (*j)["1_task"] = this->task_str;
  (*j)["2_bitcode"] = this->bitcode;
  (*j)["3_entry_function"] = this->entry_function;
  (*j)["4_work_dir"] = this->work_dir;
  (*j)["5_checker"] = *(cr->to_json());
  return j;
}

std::string sd::task::print() {
  auto j = this->to_json();
  std::string s = j->dump(2);
  delete j;
  return s;
}

void sd::task::from_json(nlohmann::json j) {
  if (j.contains("0_hash") && j["0_hash"].is_number_integer()) {
    this->hash = j["0_hash"].get<uint64_t>();
  }
  if (j.contains("1_task") && j["1_task"].is_string()) {
    this->task_str = j["1_task"].get<std::string>();
  }
  if (j.contains("2_bitcode") && j["2_bitcode"].is_string()) {
    this->bitcode = j["2_bitcode"].get<std::string>();
  }
  if (j.contains("3_entry_function") && j["3_entry_function"].is_string()) {
    this->entry_function = j["3_entry_function"].get<std::string>();
  }
  if (j.contains("4_work_dir") && j["4_work_dir"].is_string()) {
    this->work_dir = j["4_work_dir"].get<std::string>();
  }
  if (j.contains("5_checker") && j["5_checker"].is_object()) {
    if (this->cr != nullptr) {
      this->cr->from_json(j["5_checker"]);
    }
  }
}

void sd::task::compute_hash() {
  std::string hash_str;
  hash_str += this->entry_function;
  hash_str += std::to_string(cr->hash);
  this->hash = std::hash<std::string>{}(hash_str);
  this->task_str = std::to_string(this->hash) + ".json";
}

void sd::task::update_checker_inst(llvm::Module *m) { this->cr->setInst(m); }
