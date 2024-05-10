//
// Created by yu on 4/18/21.
//

#include "Manager.h"
#include <cstdint>
#include <string>

#include "../AnalysisLib/ioctl_cmd_type.h"
#include "../KnowledgeLib/device_driver.h"
#include "../ToolLib/llvm_related.h"
#include "../ToolLib/log.h"

sd::Manager::Manager() {
  t_module = new class T_module();
  k = new class knowledge();
  s = new class syzlang();
}

void sd::Manager::setup(const std::string &config) {
  std::ifstream json_ifstream(config);
  if (json_ifstream.is_open()) {
    json_ifstream >> config_json;
  } else {
    yhao_log(3, "open config json file fail: " + config);
  }

  if (config_json.contains("4_work_dir") &&
      config_json["4_work_dir"].is_string()) {
    this->work_dir = config_json["4_work_dir"].get<std::string>();
  }
  if (this->work_dir.empty()) {
    this->work_dir = std::filesystem::current_path();
  }
  this->work_dir += "/";
  this->t_module->work_dir = this->work_dir;

  std::ofstream json_ofstream(this->work_dir + "config.json");
  json_ofstream << config_json.dump(2);
  json_ofstream.close();

  if (config_json.contains("bitcode") && config_json["bitcode"].is_string()) {
    bitcode = config_json["bitcode"].get<std::string>();
    yhao_log(1, "bitcode: " + bitcode);
    this->t_module->read_bitcode(bitcode);
  } else {
    yhao_log(3, "no bitcode");
    exit(1);
  }
  if (config_json.contains("version") && config_json["version"].is_string()) {
    this->s->linux_kernel_version = config_json["version"].get<std::string>();
  } else {
  }
  this->s->set_target(this->t_module->llvm_module->getTargetTriple());
  this->s->work_dir = this->work_dir;
}

void sd::Manager::analysis() {
  yhao_log(1, "********find_init_and_exit_function start********");
  this->t_module->find_init_and_exit_function();
  this->t_module->find_module_init_function();
  t_module->remove_init_from_indirect();
  yhao_log(1, "********find_init_and_exit_function end**********");

  auto knowledge_file = config_json["knowledge"].get<std::string>();
  yhao_log(1, "knowledge json file: " + knowledge_file);
  this->k->read(knowledge_file);

  // update offset of dev->devt
  int64_t offset;
  for (auto &st : t_module->llvm_module->getIdentifiedStructTypes()) {
    std::string struct_name = get_real_structure_name(st->getName().str());
    if (struct_name == "struct.device") {
      offset = 0;
      for (auto et : st->elements()) {
        if (et->isStructTy()) {
          std::string element_name =
              get_real_structure_name(et->getStructName().str());
          if (element_name == "struct.spinlock") {
            offset = offset - 2;
            goto update_knowledge;
          }
        }
        offset++;
      }
    }
  }
update_knowledge:
  this->k->device->dev_type->offset = offset;

  yhao_log(1,
           "print knowledge json file: " + this->work_dir + "knowledge.json");
  this->k->print(this->work_dir + "knowledge.json");
  this->k->make_checker();
  this->t_module->k = this->k;
  if (config_json.contains("template_analysis") &&
      config_json["template_analysis"].is_string()) {
    this->template_klee = config_json["template_analysis"].get<std::string>();
  }

  yhao_log(1, "start our");
  time(&start);
  analysis_ops_and_file_name();
  time(&end);
  analysis_time_module_init_function = difftime(end, start);

  time(&start);
  for (auto dd : this->all_device_drivers) {
    auto id_real_hash = dd->get_id_real_hash();
    if (device_drivers.find(id_real_hash) != device_drivers.end()) {
    } else {
      device_drivers[id_real_hash] = dd;
    }
  }

  for (const auto &dd : this->device_drivers) {
    collect_info(dd.second);
  }

  for (const auto &dd : this->device_drivers) {
    collect_cmd_and_type(dd.second);
  }

  // collect more entry functions other than module init function, e.g., ioctl
  std::set<entry_function *> entry_set;
  for (const auto &dd : this->device_drivers) {
    collect_entry_function(dd.second, entry_set);
  }

  // analysis those entry functions, mainly for non-open fd
  while (!entry_set.empty()) {
    auto init_f = *entry_set.begin();
    entry_set.erase(init_f);
    // all_entry_set.insert(init_f);
    yhao_log(-1, "analysis entry: " + init_f->get_entry_function_name());

    this->t_module->check_function_pointer(init_f);
    init_f->update_checker_results();
    std::vector<device_driver *> results;
    this->mapping_checker_results(init_f, results);
    execute_init_function(results);

    for (auto dd : results) {
      auto id_real_hash = dd->get_id_real_hash();
      if (device_drivers.find(id_real_hash) != device_drivers.end()) {
      } else {
        device_drivers[id_real_hash] = dd;
      }
    }

    for (auto dd : results) {
      collect_info(dd);
    }
    for (auto dd : results) {
      collect_cmd_and_type(dd);
    }
    for (auto dd : results) {
      collect_entry_function(dd, entry_set);
    }
  }
  time(&end);
  analysis_time_ioctl = difftime(end, start);
  yhao_log(1, "end our");

  // output syscalls description based on source code file
  std::map<llvm::Function *, std::vector<device_driver *>> temp_dds;
  for (const auto &dd_pair : this->device_drivers) {
    auto dd = dd_pair.second;
    entry_function *temp_ef = dd->parent;
    while (temp_ef->parent != nullptr && temp_ef->parent->parent != nullptr) {
      temp_ef = temp_ef->parent->parent;
    }
    temp_dds[temp_ef->get_entry_function()].push_back(dd);
    yhao_log(1, "\n" + dd->print());
  }

  for (auto &dds : temp_dds) {
    std::sort(dds.second.begin(), dds.second.end());
  }

  for (const auto &dds : temp_dds) {
    this->s->generate(dds.first, dds.second);
  }

  // statistic
  this->number_module_init_function = this->init_function.size();
  this->number_driver = this->device_drivers.size();
  for (const auto &dd : this->device_drivers) {
    if (dd.second->syscalls.find(syscall_name[3]) !=
        dd.second->syscalls.end()) {
      this->number_ioctl++;
    }
  }

  for (const auto &init_f : this->init_function) {
    // yhao_log(1, "init: " + init_f.second->get_entry_function_name());

    this->number_function_pointer += init_f.second->function_pointers.size();
    this->number_indirect_call += init_f.second->number_indirect_call;
    this->number_indirect_call_target_before +=
        init_f.second->number_indirect_call_target_before;
    this->number_indirect_call_target_after +=
        init_f.second->number_indirect_call_target_after;

    // for (auto fp : init_f.second->function_pointers) {
    //     yhao_log(1, "fp: " + fp->get_name());
    // }
  }

  for (const auto &init_f : this->all_new_entry_functions) {
    // yhao_log(1, "init: " + init_f->get_entry_function_name());

    this->number_function_pointer += init_f->function_pointers.size();
    this->number_indirect_call += init_f->number_indirect_call;
    this->number_indirect_call_target_before +=
        init_f->number_indirect_call_target_before;
    this->number_indirect_call_target_after +=
        init_f->number_indirect_call_target_after;

    // for (auto fp : init_f->function_pointers) {
    //     yhao_log(1, "fp: " + fp->get_name());
    // }
  }

  std::string output;
  std::ofstream statistic(this->work_dir + "statistic.txt");
  statistic << "overall";
  statistic << " " + std::to_string(number_module_init_function);
  statistic << " " + std::to_string(number_driver);
  statistic << " " + std::to_string(number_ioctl);
  statistic << " " + std::to_string(number_function_pointer);
  statistic << " " + std::to_string(number_indirect_call);
  statistic << " " + std::to_string(number_indirect_call_target_before);
  statistic << " " + std::to_string(number_indirect_call_target_after);
  statistic << " " + std::to_string(analysis_time_module_init_function);
  statistic << " " + std::to_string(analysis_time_ioctl);
  statistic << "\n";

  for (const auto &dd_pair : this->device_drivers) {
    auto dd = dd_pair.second;
    statistic << "driver";
    statistic << " " + std::to_string(dd->ops->has_function_pointer);
    statistic << " " + dd->ops->name;
    statistic << " " + dd->ops->ops_name;
    statistic << " " + std::to_string(dd->entries.size());
    statistic << " " + std::to_string(dd->syscalls.find(syscall_name[3]) !=
                                      dd->syscalls.end());

    std::set<std::string> names;
    for (auto temp : dd->name) {
      auto str = temp->to_string();
      names.insert(str);
    }
    statistic << " " + std::to_string(names.size());

    uint64_t number = 0;
    for (auto temp : names) {
      if (temp.find('#') != std::string::npos) {
        number++;
      }
    }
    statistic << " " + std::to_string(number);

    uint64_t number1 = 0;
    uint64_t number2 = 0;
    uint64_t number3 = 0;
    for (auto temp : dd->cmd) {
      auto cmd = temp.second;
      std::set<llvm::Type *> temp_types;
      for (auto type : cmd->in_types) {
        temp_types.insert(type.second);
      }
      for (auto type : cmd->out_types) {
        temp_types.insert(type.second);
      }
      uint64_t temp_size = 1;
      if (!temp_types.empty()) {
        temp_size = temp_types.size();
      }
      number1 += temp_size;
      if (cmd->in_entry_function) {
        number2 += temp_size;
      }
      if (cmd->in_indirect_call) {
        number3 += temp_size;
      }
    }
    statistic << " " + std::to_string(number1);
    statistic << " " + std::to_string(number2);
    statistic << " " + std::to_string(number3);
    statistic << "\n";
  }

  statistic.close();
}

void sd::Manager::analysis_ops_and_file_name() {
  this->t_module->analysis_functions();

  analysis_init_function();
  // only get driver
  // handle_init_function_with_driver();
  // if new k, recheck all
  // handle_init_function_with_device();
  // get results
  handle_init_function_with_driver_device();
}

// collect info about the non-open parent and set up the related syscalls
void sd::Manager::collect_info(device_driver *dd) {
  int64_t ret;
  int64_t debug = 0;
  int64_t index;

  yhao_log(0, "collect_info: " + dd->get_id_real_hash());

  llvm::Function *fp;
  // handle parent
  // check each device drivers
  if (dd->parent != nullptr && dd->parent->parent != nullptr) {
    auto parent_dd = dd->parent->parent;
    for (auto cmd : parent_dd->cmd) {
      auto bb = cmd.second->b;
      for (auto inst : dd->ops->call_chain) {
        if (inst->getFunction() != bb->getParent()) {
          continue;
        }
        if (!llvm::isPotentiallyReachable(bb, inst->getParent())) {
          continue;
        }
        if (dd->ops->ct == checker_type::file_ops) {
          cmd.second->non_open = true;
          cmd.second->dd = dd;
        }
        dd->non_open_parent = cmd.second;
        break;
      }
      if (dd->non_open_parent != nullptr) {
        break;
      }
    }
  }
  yhao_log(debug, "ops name: " + dd->ops->ops_name);
  yhao_log(debug, "ops name: " + dd->ops->print());
  if (dd->non_open_parent != nullptr) {
    yhao_log(debug,
             "non_open_parent: " + std::to_string(dd->non_open_parent->value));
  }

  T_function *t_f;
  entry_function *new_init_f;
  for (const auto &temp : syscall_name) {
    ret = get_function_by_syscall(dd->ops, temp, &index, &fp);
    if (ret) {
      yhao_log(0, "ret get_function_by_syscall");
      continue;
    }
    t_f = this->t_module->find_t_function(fp);
    if (t_f == nullptr) {
      yhao_log(3, "t_f == nullptr");
      continue;
    }
    new_init_f = new entry_function(t_f);
    new_init_f->is_init = false;
    new_init_f->parent = dd;
    dd->syscalls[temp] = new_init_f;
    this->all_new_entry_functions.push_back(new_init_f);
    this->t_module->check_function_pointer(new_init_f);
  }
  collect_entry(dd);
}

void sd::Manager::collect_cmd_and_type(device_driver *dd) {
  int64_t debug = -1;
  int64_t ret;

  yhao_log(0, "collect_cmd_and_type: " + dd->get_id_real_hash());
  if (dd->syscalls.find(syscall_name[3]) == dd->syscalls.end()) {
    return;
  }
  entry_function *ioctl = dd->syscalls[syscall_name[3]];
  if (ioctl == nullptr) {
    return;
  }

  std::set<llvm::Function *> fps;
  auto parent = ioctl;
  while (parent != nullptr && parent->parent != nullptr) {
    for (auto temp_1 : parent->function_pointers) {
      fps.insert(temp_1->llvm_function);
    }
    parent = parent->parent->parent;
  }

  auto tf = ioctl->t_function->llvm_function;
  std::set<uint64_t> cmd;
  std::set<uint64_t> type;
  auto num_arg = tf->getFunctionType()->getNumParams();
  yhao_log(debug, "num_arg: " + std::to_string(num_arg));
  if (num_arg == 4) {
    cmd.insert(2);
    type.insert(3);
    yhao_log(debug, "block: ioctl");
  }
  // ioctl in char
  else if (num_arg == 3) {
    cmd.insert(1);
    type.insert(2);
    yhao_log(debug, "char: ioctl");
  } else {
    assert(0 && "a bad function?");
  }
  ioctl_cmd_type temp1(tf, cmd, type, dd, this->t_module, &fps);
  temp1.run_cmd();
}

void sd::Manager::collect_entry_function(
    device_driver *dd, std::set<entry_function *> &entry_set) {
  if (dd->syscalls.find(syscall_name[3]) != dd->syscalls.end() &&
      dd->syscalls[syscall_name[3]] != nullptr) {
    // do not insert entry function within loop
    std::set<std::string> temp;
    auto temp_parent = dd->syscalls[syscall_name[3]];
    while (temp_parent != nullptr && temp_parent->parent != nullptr) {
      if (temp.find(temp_parent->get_entry_function_name()) == temp.end()) {
        temp.insert(temp_parent->get_entry_function_name());
      } else {
        return;
      }
      temp_parent = temp_parent->parent->parent;
    }
    entry_set.insert(dd->syscalls[syscall_name[3]]);
  }
}

void sd::Manager::analysis_init_function() {
  int64_t debug = 0;
  for (const auto &temp : this->t_module->module_init_function) {
    auto set_f = temp.second;
    yhao_log(1, "****" + set_f->name + " start*****************");
    yhao_log(1, "size: " + std::to_string(set_f->f->size()));
    if (set_f->order < 7) {
      continue;
    }
    for (auto f : *set_f->f) {
      yhao_log(debug, "****analysis_init_function start*****************");
      if (this->t_module->t_functions.find(f) ==
          this->t_module->t_functions.end()) {
        yhao_log(3, "analysis_init_functions can not find function: " +
                        f->getName().str());
      } else {
        yhao_log(1, "analysis_init_function: " + f->getName().str());
        auto init_f = new entry_function(this->t_module->t_functions[f]);
        init_function[f->getName().str()] = init_f;

        // now we do not use this and use check_caller instead
        // this->t_module->check_callee(init_f->t_function);
        yhao_log(debug, "****check_function_pointer start*****************");
        this->t_module->check_function_pointer(init_f);
        yhao_log(debug, "****check_function_pointer end*******************");

        yhao_log(debug, "****update_checker_results start*****************");
        init_f->update_checker_results();
        std::vector<device_driver *> results;
        this->mapping_checker_results(init_f, results);
        yhao_log(debug, "****update_checker_results start*****************");

        if (results.empty()) {
          if (init_f->all_checker_results.empty()) {
          } else {
            yhao_log(debug, "init_function_with_device: " + f->getName().str());
            init_function_with_device[f->getName().str()] = init_f;
          }
        } else {
          for (auto temp1 : results) {
            int64_t kind = temp1->kind;
            if (kind & (1 << checker_type::dev_name)) {
              yhao_log(debug, "init_function_with_driver_device dev_name: " +
                                  f->getName().str());
              init_function_with_driver_device[f->getName().str()] = init_f;
            } else if (kind & (1 << checker_type::dri_name)) {
              yhao_log(debug, "init_function_with_driver_device dri_name: " +
                                  f->getName().str());
              init_function_with_driver_device[f->getName().str()] = init_f;
            } else {
              yhao_log(debug,
                       "init_function_with_driver: " + f->getName().str());
              init_function_with_driver[f->getName().str()] = init_f;
            }
            yhao_log(debug, temp1->ops->print());
          }
        }
      }
      yhao_log(debug, "****analysis_init_function end*******************");
    }
  }
  yhao_log(1, "****init function statistic start****************");
  yhao_log(1, "init_function_with_driver: " +
                  std::to_string(init_function_with_driver.size()));
  yhao_log(1, "init_function_with_device: " +
                  std::to_string(init_function_with_device.size()));
  yhao_log(1, "init_function_with_driver_device: " +
                  std::to_string(init_function_with_driver_device.size()));
  yhao_log(1, "****init function statistic end  ****************");
}

int64_t sd::Manager::handle_init_function_with_driver() {
  yhao_log(1, "****handle_init_function_with_driver start*******************");
  std::string str;
  int64_t ret;
  for (auto temp : this->all_device_drivers) {
    if (!temp->is_only_driver()) {
      continue;
    }

    // get driver ops checker
    auto init_f = temp->parent;
    auto cr_ops = temp->ops;
    auto task1 = make_task(init_f->get_entry_function_name(), cr_ops);
    execute_task(task1);
    yhao_log(-1, "json: \n" + task1->print());

    // get the open function in ops structure from checker
    llvm::Function *fp;
    int64_t open_index;
    ret = this->get_function_by_syscall(cr_ops, "4-open", &open_index, &fp);
    if (ret) {
      continue;
    }

    // analysis open function and find indirect function call checker
    std::vector<checker_result *> crs_open;
    ret = this->t_module->check_open(fp, &crs_open);
    if (ret) {
      continue;
    }

    // TODO: get offset chain of indirect function call checker cr_open
    auto cr_open = crs_open.at(0);
    cr_open->structure = cr_ops->structure;
    yhao_dump(-1, cr_open->inst->print, str) auto oc = new offset_chain_base();
    cr_open->offset_chain = oc;
    oc->base = "";
    oc->type_str = "struct.snd_minor";
    auto next1 = new offset_chain_offset();
    oc->next = next1;
    next1->offset = 3;
    next1->type_str = "struct.file_operations";
    auto next2 = new offset_chain_offset();
    next1->next = next2;
    next2->offset = 14;

    auto task2 = make_task(fp->getName().str(), cr_open);
    execute_task(task2);
    yhao_log(-1, "json: \n" + task2->print());

    ret = this->k->add_knowledge(cr_open, cr_ops->ops_name, open_index);
    if (ret) {
      continue;
    }
  }

  this->k->print(this->work_dir + "knowledge.json");
  yhao_log(1, "****handle_init_function_with_driver end*******************");
  return 0;
}

int64_t sd::Manager::handle_init_function_with_device() {
  yhao_log(1, "****handle_init_function_with_device start*******************");
  std::string str;
  int64_t ret = 0;

  // update analysis if new k
  if (!k->new_knowledge) {
    yhao_log(1, "****no new k end*************************************");
    return 0;
  }

  this->t_module->update_function();

  // update existing init function with device drivers
  for (const auto &f : this->init_function_with_driver_device) {
    auto init_f = f.second;
    init_f->update_checker_results();
    std::vector<device_driver *> results;
    this->mapping_checker_results(init_f, results);
  }

  // update init function with device
  std::map<std::string, entry_function *> temp;
  for (const auto &f : this->init_function_with_device) {
    auto init_f = f.second;
    init_f->update_checker_results();
    std::vector<device_driver *> results;
    this->mapping_checker_results(init_f, results);

    yhao_log(1, init_f->get_entry_function_name());

    if (results.empty()) {
      if (init_f->all_checker_results.empty()) {
        yhao_log(3, "where is device?");
        ret = 1;
      } else {
        // the same as before
        yhao_log(1, "same as before");
      }
    } else {
      for (auto temp1 : results) {
        int64_t kind = temp1->kind;
        if (kind & (1 << checker_type::dev_name)) {
          temp[f.first] = f.second;
          yhao_log(1, "new init_function_with_driver_device dev_name: " +
                          init_f->get_entry_function_name());
          init_function_with_driver_device[init_f->get_entry_function_name()] =
              init_f;
        } else if (kind & (1 << checker_type::dri_name)) {
          temp[f.first] = f.second;
          yhao_log(1, "new init_function_with_driver_device dri_name: " +
                          init_f->get_entry_function_name());
          init_function_with_driver_device[init_f->get_entry_function_name()] =
              init_f;
        } else {
          yhao_log(3, "find the new driver but where is the device?");
          ret = 1;
        }
      }
    }
  }
  for (const auto &f : temp) {
    init_function_with_device.erase(f.first);
  }

  yhao_log(1, "****handle_init_function_with_device end*******************");
  return ret;
}

// TODO:handle_init_function_with_driver_device
int64_t sd::Manager::handle_init_function_with_driver_device() {
  yhao_log(
      1,
      "****handle_init_function_with_driver_device start*******************");
  int64_t debug = -1;
  std::string str;
  int64_t ret = 0;

  execute_init_function(this->all_device_drivers);
  for (auto temp_dd : this->all_device_drivers) {
    // remove some device name
    yhao_log(debug, "remove name start: ");
    if (temp_dd->name.size() > 1) {
      std::vector<checker_result_string *> temp_name;
      for (auto name : temp_dd->name) {
        yhao_log(debug, name->print());
        if (name->fmt.find("%s") != std::string::npos && name->va2.empty()) {
          continue;
        }
        temp_name.push_back(name);
      }
      if (temp_name.empty()) {
        continue;
      }
      temp_dd->name.clear();
      for (auto name : temp_name) {
        temp_dd->name.push_back(name);
      }
    }
    yhao_log(debug, "remove name end: ");

    yhao_log(debug, "mapping: ");
    yhao_log(debug, "checker ops:");
    yhao_log(debug, temp_dd->ops->print());
    yhao_log(debug, "checker name:");
    for (auto temp_1 : temp_dd->name) {
      yhao_log(debug, temp_1->print());
    }
    yhao_log(debug, "checker number:");
    for (auto temp_1 : temp_dd->number) {
      yhao_log(debug, temp_1->print());
    }
  }
  yhao_log(
      1, "****handle_init_function_with_driver_device end*******************");
  return ret;
}

void sd::Manager::mapping_checker_results(
    entry_function *ef, std::vector<device_driver *> &results) {
  int64_t debug = -1;
  yhao_log(debug, "****mapping_checker_results(); start*****************");
  std::vector<sd::checker_result *> crs;
  ef->get_checker_results(checker_type::dri_ops, &crs);
  ef->get_checker_results(checker_type::file_ops, &crs);
  if (crs.empty()) {
    return;
  }
  for (auto c : crs) {
    yhao_log(debug, "dri_ops or file_ops: " + c->print());
    auto temp = new device_driver();
    temp->linux_kernel_version = this->s->linux_kernel_version;
    temp->ops = (checker_result_ops *)(c);
    temp->parent = ef;
    all_device_drivers.push_back(temp);
    results.push_back(temp);
  }

  crs.clear();
  ef->get_checker_results(checker_type::dri_name, &crs);
  for (auto c : crs) {
    int64_t match = -1, temp_match;
    device_driver *temp_dd = nullptr;
    bool equal_match = false;
    for (auto temp : all_device_drivers) {
      if (c->id == temp->ops->id) {
        temp_match = match_call_chain(c, temp->ops);
        if (temp_match > match) {
          match = temp_match;
          temp_dd = temp;
          equal_match = false;
        } else if (temp_match == match) {
          equal_match = true;
        }
      }
    }
    c->match = match;
    if (equal_match) {
      for (auto temp : all_device_drivers) {
        if (c->id == temp->ops->id) {
          temp_match = match_call_chain(c, temp->ops);
          if (temp_match == match) {
            temp->name.push_back((checker_result_string *)c);
          }
        }
      }
    } else if (temp_dd == nullptr) {
      yhao_log(2, "temp_dd == nullptr: " + c->print());
    } else {
      temp_dd->name.push_back((checker_result_string *)c);
    }
  }

  crs.clear();
  ef->get_checker_results(checker_type::dev_name, &crs);
  for (auto c : crs) {
    std::string p1 =
        get_file_name(c->call_chain.at(0)->getParent()->getParent());
    int64_t index = p1.find('/');
    index = p1.find('/', index + 1);
    int64_t temp_match;
    for (auto temp : all_device_drivers) {
      if (c->id == temp->ops->id) {
        temp_match = match_call_chain(c, temp->ops);
        if (temp_match > 0) {
          c->match = temp_match;
          temp->name.push_back((checker_result_string *)c);
        } else if (temp_match == 0) {
          std::string p2 = get_file_name(
              temp->ops->call_chain.at(0)->getParent()->getParent());
          if (index < p1.size() && index < p2.size()) {
            if (p1.substr(0, index) == p2.substr(0, index)) {
              temp->name.push_back((checker_result_string *)c);
            }
          }
        }
      }
    }
  }

  crs.clear();
  ef->get_checker_results(checker_type::dev_type, &crs);
  for (auto c : crs) {
    for (auto temp : all_device_drivers) {
      temp->number.push_back((checker_result_number *)c);
    }
  }

  crs.clear();
  ef->get_checker_results(checker_type::dri_type, &crs);
  ef->get_checker_results(checker_type::dri_major, &crs);
  ef->get_checker_results(checker_type::dri_minor, &crs);
  for (auto c : crs) {
    int64_t match = -1, temp_match;
    device_driver *temp_dd;
    bool equal_match = false;
    for (auto temp : all_device_drivers) {
      temp_match = match_call_chain(c, temp->ops);
      if (temp_match > match) {
        match = temp_match;
        temp_dd = temp;
        equal_match = false;
      } else if (temp_match == match) {
        equal_match = true;
      }
    }
    c->match = match;
    if (equal_match) {
      for (auto temp : all_device_drivers) {
        temp_match = match_call_chain(c, temp->ops);
        if (temp_match == match) {
          temp->number.push_back((checker_result_number *)c);
        }
      }
    } else {
      temp_dd->number.push_back((checker_result_number *)c);
    }
  }

  crs.clear();
  ef->get_checker_results(checker_type::file_install, &crs);
  for (auto c : crs) {
    yhao_log(debug, "file_install: " + c->print());
    int64_t match = -1, temp_match;
    device_driver *temp_dd;
    bool equal_match = false;
    for (auto temp : all_device_drivers) {
      temp_match = match_call_chain(c, temp->ops);
      if (temp_match > match) {
        match = temp_match;
        temp_dd = temp;
        equal_match = false;
      } else if (temp_match == match) {
        equal_match = true;
      }
    }
    c->match = match;
    if (equal_match) {
      for (auto temp : all_device_drivers) {
        temp_match = match_call_chain(c, temp->ops);
        if (temp_match == match) {
          temp->file_install.push_back(c);
        }
      }
    } else {
      if (temp_dd == nullptr) {
        yhao_log(3, "temp_dd == nullptr");
      }
      temp_dd->file_install.push_back(c);
    }
  }

  crs.clear();
  ef->get_checker_results(checker_type::file_fd, &crs);
  for (auto c : crs) {
    int64_t match = -1, temp_match;
    device_driver *temp_dd;
    bool equal_match = false;
    for (auto temp : all_device_drivers) {
      temp_match = match_call_chain(c, temp->ops);
      if (temp_match > match) {
        match = temp_match;
        temp_dd = temp;
        equal_match = false;
      } else if (temp_match == match) {
        equal_match = true;
      }
    }
    c->match = match;
    if (equal_match) {
      for (auto temp : all_device_drivers) {
        temp_match = match_call_chain(c, temp->ops);
        if (temp_match == match) {
          temp->file_fd.push_back((checker_result_number *)c);
        }
      }
    } else {
      temp_dd->file_fd.push_back((checker_result_number *)c);
    }
  }

  for (auto dd : all_device_drivers) {
    dd->set_kind();
  }
  yhao_log(debug, "****mapping_checker_results(); end*******************");
}

void sd::Manager::check_cmd_and_type(const std::string &function_name,
                                     nlohmann::json *result_json,
                                     std::set<llvm::Function *> *fps) const {}

int64_t sd::Manager::get_function_by_index(sd::checker_result_ops *cr0,
                                           int64_t index,
                                           llvm::Function **fp) const {
  std::string str;
  int64_t debug = -1;
  int64_t ret;
  llvm::GlobalVariable *ops;
  ret = this->t_module->find_ops_structure(cr0->ops_structure, cr0->ops_name,
                                           &ops);
  if (ret) {
    yhao_log(debug, "not find ops");
    return 1;
  }

  if (!ops->hasInitializer()) {
    yhao_log(debug, "ops->hasInitializer()");
    return 1;
  }

  auto c = ops->getInitializer();
  const llvm::ConstantStruct *cs = llvm::dyn_cast<llvm::ConstantStruct>(c);
  if (cs == nullptr) {
    yhao_log(debug, "llvm::dyn_cast<llvm::ConstantStruct> 1");
    yhao_dump(debug, ops->print, str);
    return 1;
  }

  if (index == -2) {
    yhao_log(debug, "index == -2");
    return 1;
  }

  if (cs->getNumOperands() <= 2) {
    c = cs->getOperand(0);
    cs = llvm::dyn_cast<llvm::ConstantStruct>(c);
    if (cs == nullptr) {
      yhao_log(debug, "llvm::dyn_cast<llvm::ConstantStruct> 2");
      yhao_dump(debug, ops->print, str);
      return 1;
    }
  }

  if (index >= cs->getNumOperands() || index < 0) {
    yhao_log(2, "cr0->ops_name: " + cr0->ops_name);
    yhao_log(2, "get_function_by_index: index >= cs->getNumOperands(): " +
                    std::to_string(index));
    yhao_dump(2, cs->print, str);
    return 1;
  }

  llvm::Value *fp1 = cs->getOperand(index);
  if (llvm::isa<llvm::BitCastOperator>(fp1)) {
    fp1 = llvm::dyn_cast<llvm::BitCastOperator>(fp1)->getOperand(0);
  }
  auto *fp2 = llvm::dyn_cast<llvm::Function>(fp1);
  if (fp2 == nullptr) {
    yhao_log(debug, "cr0->ops_name: " + cr0->ops_name);
    yhao_print(debug, fp1->print, str);
    yhao_log(debug, "get_function_by_index: fail: " + std::to_string(index) +
                        ": " + str);
    yhao_dump(debug, cs->print, str);
    return 1;
  }
  *fp = fp2;
  return 0;
}

int64_t sd::Manager::get_function_by_syscall(sd::checker_result_ops *cr0,
                                             std::string syscall,
                                             int64_t *index,
                                             llvm::Function **fp) const {
  std::string str;
  int64_t ret;
  int64_t debug = -1;

  K_variable *v;
  ret = this->k->find_k_variable(cr0->structure, cr0->offset, &v);
  if (ret) {
    yhao_log(debug, "this->k->find_k_variable(cr0->structure, cr0->offset, &v);");
    return ret;
  }
  if (v->functions_index.find(syscall) == v->functions_index.end()) {
    yhao_log(debug, "v->functions_index.find(syscall) == v->functions_index.end()");
    return 1;
  }
  *index = v->functions_index[syscall];
  ret = get_function_by_index(cr0, *index, fp);
  if (ret) {
    yhao_log(debug, "get_function_by_index(cr0, *index, fp);");
    return ret;
  }
  return 0;
}

sd::task *sd::Manager::make_task(const std::string &entry_function,
                                 checker_result *cr,
                                 const std::string &task_bitcode,
                                 const std::string &task_work_dir) const {
  auto task = new class task(entry_function, cr);
  if (task_bitcode.empty()) {
    task->bitcode = this->bitcode;
  } else {
    task->bitcode = task_bitcode;
  }
  if (task_work_dir.empty()) {
    task->work_dir = this->work_dir;
  } else {
    task->work_dir = task_work_dir;
  }
  return task;
}

void sd::Manager::execute_init_function(
    std::vector<device_driver *> &dds) const {
  for (auto temp_dd : dds) {
    // get driver ops checker
    auto cr_ops = temp_dd->ops;
    auto task1 = make_task(temp_dd->parent->get_entry_function_name(), cr_ops);
    execute_task(task1);

    // get device name checker
    for (auto temp_1 : temp_dd->name) {
      auto task2 =
          make_task(temp_dd->parent->get_entry_function_name(), temp_1);
      execute_task(task2);
    }
  }
}

// now only consider one task_str with on checker
int64_t sd::Manager::execute_task(sd::task *t) {
  std::string str;
  int64_t debug = 0;
  llvm::Value *debug_v = nullptr;

  auto ct = t->cr->ct;
  switch (ct) {
    case checker_type::dev_type:
    case checker_type::dri_type:
    case checker_type::dri_major:
    case checker_type::dri_minor: {
      //            cr = new checker_result_number();
      return 0;
    }
    case checker_type::dev_name:
    case checker_type::dri_name: {
      auto cr = (checker_result_string *)t->cr;
      auto inst = cr->inst;
      if (inst->getOpcode() == llvm::Instruction::Call) {
        if (cr->value > inst->getNumOperands()) {
          yhao_log(3, "inst->getNumOperands() <= cr->value");
          return 1;
        }
        auto v = inst->getOperand(cr->value);
        if (llvm::isa<llvm::GEPOperator>(v)) {
          v = llvm::dyn_cast<llvm::GEPOperator>(v)->getOperand(0);
        }
        if (llvm::isa<llvm::BitCastOperator>(v)) {
          v = llvm::dyn_cast<llvm::BitCastOperator>(v)->getOperand(0);
        }
        if (cr->value_offset != -2) {
          auto gv = llvm::dyn_cast<llvm::GlobalVariable>(v);
          if (gv == nullptr) {
            yhao_dump(debug, v->print, str);
            yhao_log(debug, "gv == nullptr");
            break;
          }
          if (!gv->hasInitializer()) {
            yhao_dump(debug, v->print, str);
            yhao_log(debug, "!gv->hasInitializer()");
            break;
          }
          auto gvI = gv->getInitializer();
          auto temp_v1 = llvm::dyn_cast<llvm::ConstantStruct>(gvI);
          if (temp_v1 == nullptr) {
            yhao_dump(debug, v->print, str);
            yhao_log(debug, "temp_v1 == nullptr");
            break;
          }
          yhao_dump(debug, temp_v1->print, str);
          llvm::Value *temp_v2 = nullptr;
          if (!temp_v1->getType()->hasName()) {
            temp_v2 = temp_v1->getOperand(0)->getOperand(cr->value_offset);
          } else {
            temp_v2 = temp_v1->getOperand(cr->value_offset);
          }

          if (llvm::isa<llvm::BitCastOperator>(temp_v2)) {
            temp_v2 =
                llvm::dyn_cast<llvm::BitCastOperator>(temp_v2)->getOperand(0);
          }

          v = temp_v2;
        } else {
        }

        if (auto temp_v1 = llvm::dyn_cast<llvm::GEPOperator>(v)) {
          v = temp_v1->getOperand(0);
        }

        auto gv = llvm::dyn_cast<llvm::GlobalVariable>(v);
        if (gv == nullptr || !gv->hasInitializer()) {
          yhao_dump(debug, v->print, str);
          yhao_log(debug, "gv == nullptr || !gv->hasInitializer()");
          break;
        }

        auto gvI = gv->getInitializer();

        if (const llvm::ConstantStruct *c =
                llvm::dyn_cast<llvm::ConstantStruct>(gvI)) {
          gvI = c->getOperand(0);
        }

        if (const llvm::ConstantDataArray *currDArray =
                llvm::dyn_cast<llvm::ConstantDataArray>(gvI)) {
          cr->fmt = currDArray->getAsCString().str();
        } else {
          yhao_print(3, gvI->print, str);
          cr->fmt = str;
        }

        if (cr->value != -2) {
          yhao_log(debug, "handle situation: name 1");
          yhao_log(debug, t->print());
          return 0;
        }
        yhao_dump(debug, inst->print, str);
        yhao_log(debug, "operands number of call inst: " +
                            std::to_string(inst->getNumOperands()));
        for (int64_t i = cr->value + 1; i < inst->getNumOperands() - 1; ++i) {
          auto temp_v1 = inst->getOperand(i);
          if (llvm::isa<llvm::GEPOperator>(temp_v1)) {
            auto temp_v2 = llvm::dyn_cast<llvm::GEPOperator>(temp_v1);
            temp_v1 = temp_v2->getOperand(0);
            auto temp_gv1 = llvm::dyn_cast<llvm::GlobalVariable>(temp_v1);
            if (temp_gv1 == nullptr || !temp_gv1->hasInitializer()) {
              cr->va2.emplace_back("");
              continue;
            }
            auto temp_gv1I = temp_gv1->getInitializer();

            if (const llvm::ConstantDataArray *cda =
                    llvm::dyn_cast<llvm::ConstantDataArray>(temp_gv1I)) {
              cr->va2.push_back(cda->getAsCString().str());
            } else {
              yhao_print(3, temp_gv1I->print, str);
              cr->va2.push_back(str);
            }
          } else if (llvm::isa<llvm::ConstantInt>(temp_v1)) {
            auto temp_v2 = llvm::dyn_cast<llvm::ConstantInt>(temp_v1);
            cr->va1.push_back(temp_v2->getZExtValue());
          } else {
          }
        }
        yhao_log(debug, "handle situation: name 2");
        yhao_log(debug, t->print());
        return 0;
      }
      break;
    }
    case checker_type::dri_ops:
    case checker_type::file_ops: {
      auto cr = (checker_result_ops *)t->cr;
      auto inst = cr->inst;
      if (inst->getOpcode() == llvm::Instruction::Store) {
        auto v = inst->getOperand(0);
        if (llvm::isa<llvm::GEPOperator>(v)) {
          v = llvm::dyn_cast<llvm::GEPOperator>(v)->getOperand(0);
        }
        if (llvm::isa<llvm::BitCastOperator>(v)) {
          v = llvm::dyn_cast<llvm::BitCastOperator>(v)->getOperand(0);
        }
        auto gv = llvm::dyn_cast<llvm::GlobalVariable>(v);
        if (gv == nullptr) {
          break;
        }
        if (!gv->hasInitializer()) {
          break;
        }
        auto gvI = gv->getInitializer();
        auto gvt = gvI->getType();
        if (gvt->isStructTy()) {
          auto temp = llvm::dyn_cast<llvm::StructType>(gvt);
          if (temp->hasName()) {
            cr->ops_structure = gvt->getStructName().str();
          } else {
            if (llvm::isa<llvm::StructType>(gvt->getStructElementType(0))) {
              cr->ops_structure =
                  gvt->getStructElementType(0)->getStructName().str();
            }
          }
        }
        cr->ops_name = gv->getName().str();
        yhao_log(debug, "handle situation: ops store");
        yhao_log(debug, t->print());
        return 0;

      } else if (inst->getOpcode() == llvm::Instruction::Call) {
        auto v = inst->getOperand(cr->value);
        if (llvm::isa<llvm::GEPOperator>(v)) {
          v = llvm::dyn_cast<llvm::GEPOperator>(v)->getOperand(0);
        }
        if (llvm::isa<llvm::BitCastOperator>(v)) {
          v = llvm::dyn_cast<llvm::BitCastOperator>(v)->getOperand(0);
        }
        auto gv = llvm::dyn_cast<llvm::GlobalVariable>(v);
        if (gv == nullptr) {
          yhao_dump(debug, v->print, str);
          yhao_log(debug, "gv == nullptr");
          break;
        }
        if (!gv->hasInitializer()) {
          yhao_dump(debug, v->print, str);
          yhao_log(debug, "!gv->hasInitializer()");
          break;
        }
        auto gvI = gv->getInitializer();
        if (cr->value_offset != -2) {
          auto temp_v1 = llvm::dyn_cast<llvm::ConstantStruct>(gvI);
          if (temp_v1 == nullptr) {
            yhao_log(debug, "temp_v1 == nullptr");
            debug_v = gvI;
            break;
          }

          llvm::Value *temp_v2 = nullptr;
          if (!temp_v1->getType()->hasName()) {
            temp_v2 = temp_v1->getOperand(0)->getOperand(cr->value_offset);
          } else {
            temp_v2 = temp_v1->getOperand(cr->value_offset);
          }

          if (llvm::isa<llvm::GEPOperator>(temp_v2)) {
            temp_v2 = llvm::dyn_cast<llvm::GEPOperator>(temp_v2)->getOperand(0);
          }
          if (llvm::isa<llvm::BitCastOperator>(temp_v2)) {
            temp_v2 =
                llvm::dyn_cast<llvm::BitCastOperator>(temp_v2)->getOperand(0);
          }

          gv = llvm::dyn_cast<llvm::GlobalVariable>(temp_v2);
          if (gv == nullptr) {
            yhao_dump(debug, temp_v2->print, str);
            yhao_log(debug, "cr->value_offset: gv == nullptr");
            break;
          }
          if (!gv->hasInitializer()) {
            yhao_dump(debug, temp_v2->print, str);
            yhao_log(debug, "cr->value_offset: !gv->hasInitializer()");
            break;
          }
          gvI = gv->getInitializer();
        }
        auto gvt = gvI->getType();
        if (gvt->isStructTy()) {
          auto temp = llvm::dyn_cast<llvm::StructType>(gvt);
          if (temp->hasName()) {
            cr->ops_structure = gvt->getStructName().str();
          } else {
            if (llvm::isa<llvm::StructType>(gvt->getStructElementType(0))) {
              cr->ops_structure =
                  gvt->getStructElementType(0)->getStructName().str();
            }
          }
        }
        cr->ops_name = gv->getName().str();
        yhao_log(debug, "handle situation: ops call");
        yhao_log(debug, t->print());
        return 0;
      }
      break;
    }
    case checker_type::open_indirect: {
      //            execute_task_by_klee(t);
      return 0;
    }
    case checker_type::file_fd:
    case checker_type::file_install:
    default: {
      yhao_log(3, "not handle now");
      return 0;
    }
  }
  yhao_log(2, "not handle situation");
  if (debug_v != nullptr) {
    yhao_dump(2, debug_v->print, str);
  }
  yhao_log(2, t->print());
  //        execute_task_by_klee(t);
  return 0;
}

int64_t sd::Manager::execute_task_by_klee(sd::task *t) const {
  auto j = t->to_json();
  std::ofstream json_of_stream(t->task_str);
  json_of_stream << j->dump(2);
  json_of_stream.close();
  delete j;

  std::string cmd;
  cmd = this->template_klee;
  cmd.append(" --config_json=" + t->work_dir + t->task_str);
  cmd.append("1>>klee_debug.log 2>&1");

  yhao_log(1, "cmd start: " + cmd);

  FILE *stream;
  std::string Result;
  const int max_buffer = 1024;
  char buffer[max_buffer];

  stream = popen(cmd.c_str(), "r");
  if (stream) {
    while (!feof(stream)) {
      if (fgets(buffer, max_buffer, stream) != nullptr) Result = buffer;
    }
    pclose(stream);
  }

  yhao_log(1, "cmd finish: " + cmd);

  std::ifstream json_if_stream(t->task_str);
  nlohmann::json new_json;
  json_if_stream >> new_json;
  t->from_json(new_json);
  json_if_stream.close();
  return 0;
}

void sd::Manager::collect_entry(sd::device_driver *dd) const {
  int64_t ret;
  llvm::GlobalVariable *ops;
  ret = this->t_module->find_ops_structure(dd->ops->ops_structure,
                                           dd->ops->ops_name, &ops);
  if (ret) {
    return;
  }

  if (!ops->hasInitializer()) {
    return;
  }

  auto c = ops->getInitializer();
  const llvm::ConstantStruct *cs = llvm::dyn_cast<llvm::ConstantStruct>(c);
  if (cs == nullptr) {
    return;
  }

  for (uint64_t index = 0; index < cs->getNumOperands(); index++) {
    auto fp1 = cs->getOperand(index);
    auto *fp2 = llvm::dyn_cast<llvm::Function>(fp1);
    if (fp2 == nullptr) {
      continue;
    }
    dd->entries.insert(fp2);
  }
}

llvm::Value *sd::Manager::get_real_value(llvm::Value *v) {
  if (llvm::isa<llvm::GEPOperator>(v)) {
    v = llvm::dyn_cast<llvm::GEPOperator>(v)->getOperand(0);
    return get_real_value(v);
  } else if (llvm::isa<llvm::BitCastOperator>(v)) {
    v = llvm::dyn_cast<llvm::BitCastOperator>(v)->getOperand(0);
    return get_real_value(v);
  } else if (llvm::ConstantStruct *cs = llvm::dyn_cast<llvm::ConstantStruct>(v);
             cs && !cs->hasName()) {
    v = cs->getOperand(0);
    return get_real_value(v);
  } else {
    return v;
  }
}
