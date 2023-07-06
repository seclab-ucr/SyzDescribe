//
// Created by yu on 4/20/21.
//

#include "T_function.h"

#include "../ToolLib/llvm_related.h"
#include "../ToolLib/log.h"
#include "T_module.h"

#define DEBUG_FUNCTION_POINTER -1

sd::T_function::T_function(llvm::Function *f) : llvm_function(f) {
  num_checkers_check_callee = -1;
  num_checkers_check_caller = 0;
}

sd::T_function::~T_function() = default;

void sd::T_function::check() {
  int64_t debug = DEBUG_CHECKER;
  std::string str;
  yhao_log(debug, "****check_device_driver start*****************");
  yhao_log(debug, "check(): " + this->llvm_function->getName().str());

  for (auto &b : *this->llvm_function) {
    for (auto &i : b) {
      switch (i.getOpcode()) {
        case llvm::Instruction::Call: {
          auto &ci = llvm::cast<llvm::CallInst>(i);

          // e.g., @llvm.dbg.declare
          if (ci.isDebugOrPseudoInst()) {
            break;
          }
          // e.g., @llvm.lifetime.start.p0i8
          if (ci.isLifetimeStartOrEnd()) {
            break;
          }
          if (ci.isInlineAsm()) {
            break;
          }

          yhao_dump(-1, i.print, str) if (ci.isIndirectCall()) {
            yhao_log(debug, "ci.isIndirectCall() in " + inst_to_strID(&i));
            add_callee(&i, callee_type::callee_indirect);
            break;
          }

          auto tf = get_target_function(ci.getCalledOperand());
          if (tf == nullptr) {
            yhao_log(2, "ci.getCalledOperand() == nullptr");
            break;
          }
          std::string function_name = get_real_function_name(tf);
          if (function_name == "llvm") {
            break;
          }

          auto t_f = this->t_module->find_t_function(tf);
          if (t_f == nullptr) {
            break;
          }

          if (tf->isDeclaration()) {
            yhao_log(debug, "tf->isDeclaration(): " + function_name);
            add_callee(&i, callee_type::callee_isDeclaration, t_f);
          } else {
            add_callee(&i, callee_type::callee_not_isDeclaration, t_f);
            t_f->caller_not_isDeclaration.insert(this);
          }
          break;
        }
        default: {
        }
      }
      check_function_pointer(&i);
    }
  }

  yhao_log(debug, "****check_device_driver end*******************");
}

void sd::T_function::check(sd::knowledge *k) {
  std::string str;
  int64_t debug = DEBUG_CHECKER;
  yhao_log(debug, "****check_device_driver start*****************");
  yhao_log(debug, "check(sd::k *k): " + this->llvm_function->getName().str());

  for (auto &b : *this->llvm_function) {
    for (auto &i : b) {
      yhao_dump(-1, i.print, str) switch (i.getOpcode()) {
        case llvm::Instruction::Call: {
          if (all_callee_map.find(&i) == all_callee_map.end()) {
            break;
          }
          auto ce = all_callee_map[&i];

          if (ce->ct != callee_type::callee_not_isDeclaration &&
              ce->ct != callee_type::callee_isDeclaration) {
            break;
          }
          auto tf = ce->tf;
          yhao_log(debug, "check: call:" + tf->get_name());

          auto &ci = llvm::cast<llvm::CallInst>(i);
          std::string function_name = get_real_function_name(tf->llvm_function);

          bool ret = false;
          if (k->inst_call.find(function_name) != k->inst_call.end()) {
            yhao_log(debug, "find in inst_call:" + tf->get_name());
            for (auto c : *(k->inst_call[function_name])) {
              if (check_call(c, &ci)) {
                add_checker_result(c, &i);
                ret = true;
              }
            }
          }

          // not check the function again even for callee
          if (ret) {
            ce->checked = true;
          }
          break;
        }
        case llvm::Instruction::Store: {
          auto a1 = i.getOperand(1);
          if (auto *inst = llvm::dyn_cast<llvm::Instruction>(a1)) {
            if (inst->getOpcode() == llvm::Instruction::GetElementPtr) {
              auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(inst);
              if (gep->isInBounds()) {
                auto se_ty = gep->getSourceElementType();
                if (se_ty->isStructTy()) {
                  std::string struct_name = se_ty->getStructName().str();
                  if (struct_name.empty()) {
                    break;
                  }
                  struct_name =
                      get_real_structure_name(se_ty->getStructName().str());
                  if (k->inst_store.find(struct_name) == k->inst_store.end()) {
                    break;
                  }
                  yhao_log(debug, "struct_name: " + struct_name);
                  if (auto c1 =
                          llvm::cast<llvm::ConstantInt>(gep->getOperand(2))) {
                    uint64_t offset = c1->getZExtValue();
                    for (auto c : *(k->inst_store[struct_name])) {
                      if (offset == c->offset) {
                        add_checker_result(c, &i);
                      }
                    }
                  }
                }
              } else {
                // TODO: fix or handle it
                yhao_log(debug, "check: !gep->isInBounds()");
              }
            }
          }
          break;
        }
        default: {
        }
      }
    }
  }

  yhao_log(debug, "****check_device_driver end*******************");
}

bool sd::T_function::check_call(sd::checker *c, llvm::CallInst *cs) {
  int64_t debug = DEBUG_CHECKER;
  if (!c->field) {
    return true;
  }

  auto arg = cs->getOperand(c->object);
  if (auto *inst = llvm::dyn_cast<llvm::Instruction>(arg)) {
    if (auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(inst)) {
      auto se_ty = gep->getSourceElementType();
      if (se_ty->isStructTy()) {
        std::string struct_name =
            get_real_structure_name(se_ty->getStructName().str());
        if (struct_name == c->structure) {
          yhao_log(debug, "check_call_cmd: struct_name: " + struct_name);
          if (auto c1 = llvm::cast<llvm::ConstantInt>(gep->getOperand(2))) {
            uint64_t offset = c1->getZExtValue();
            if (offset == c->offset) {
              return true;
            }
          }
        }
      } else if (se_ty->isArrayTy()) {
        auto arg2 = gep->getOperand(0);
        if (auto gep2 = llvm::dyn_cast<llvm::GetElementPtrInst>(arg2)) {
          auto se_ty2 = gep2->getSourceElementType();
          if (se_ty2->isStructTy()) {
            std::string struct_name =
                get_real_structure_name(se_ty2->getStructName().str());
            if (struct_name == c->structure) {
              yhao_log(debug,
                       "check_call_cmd: array struct_name: " + struct_name);
              if (auto c1 =
                      llvm::cast<llvm::ConstantInt>(gep2->getOperand(2))) {
                uint64_t offset = c1->getZExtValue();
                if (offset == c->offset) {
                  return true;
                }
              }
            }
          }
        }
      }
    }
  }

  return false;
}

void sd::T_function::check_function_pointer(llvm::Instruction *i) {
  std::string str;

  yhao_log(DEBUG_FUNCTION_POINTER,
           "check_function_pointer: " + inst_to_strID(i));

  uint64_t num = i->getNumOperands();
  // not check function call here
  if (i->getOpcode() == llvm::Instruction::Call) {
    num--;
  }

  for (int index = 0; index < num; index++) {
    auto arg = i->getOperand(index);
    llvm::SmallPtrSet<const llvm::GlobalValue *, 3> Visited;
    check_function_pointer(arg, Visited);
  }

  add_callee(i, callee_type::function_pointer, current_function_pointer);
  for (auto temp : current_function_pointer) {
    temp->caller_pointer.insert(this);
  }
  current_function_pointer.clear();
}

void sd::T_function::check_function_pointer(
    llvm::Value *v, llvm::SmallPtrSet<const llvm::GlobalValue *, 3> &visited) {
  std::string str;

  auto *c = llvm::dyn_cast<llvm::Constant>(v);
  if (!c) {
    return;
  }

  while (true) {
    if (auto *gv = llvm::dyn_cast<llvm::GlobalValue>(c)) {
      if (!visited.insert(gv).second) {
        return;
      }
      if (auto *f = llvm::dyn_cast<llvm::Function>(gv)) {
        yhao_log(DEBUG_FUNCTION_POINTER,
                 "find function pointer: " + f->getName().str());

        auto t_f = this->t_module->find_t_function(f);
        if (t_f == nullptr) {
          return;
        }
        current_function_pointer.insert(t_f);
        return;
      } else if (auto *ga = llvm::dyn_cast<llvm::GlobalAlias>(gv)) {
        c = ga->getAliasee();
      } else if (auto gvv = llvm::dyn_cast<llvm::GlobalVariable>(gv)) {
        yhao_log(DEBUG_FUNCTION_POINTER,
                 "GlobalVariable: " + gvv->getName().str());
        if (gvv->hasInitializer()) {
          auto gvvi = gvv->getInitializer();
          if (const llvm::ConstantVector *cp =
                  llvm::dyn_cast<llvm::ConstantVector>(gvvi)) {
            for (unsigned ii = 0, e = cp->getNumOperands(); ii != e; ++ii) {
              check_function_pointer(cp->getOperand(ii), visited);
            }
          } else if (const llvm::ConstantArray *ca =
                         llvm::dyn_cast<llvm::ConstantArray>(gvvi)) {
            for (unsigned ii = 0, e = ca->getNumOperands(); ii != e; ++ii) {
              check_function_pointer(ca->getOperand(ii), visited);
            }
          } else if (const llvm::ConstantStruct *cs =
                         llvm::dyn_cast<llvm::ConstantStruct>(gvvi)) {
            for (unsigned ii = 0, e = cs->getNumOperands(); ii != e; ++ii) {
              check_function_pointer(cs->getOperand(ii), visited);
            }
          } else if (const llvm::ConstantDataSequential *cds =
                         llvm::dyn_cast<llvm::ConstantDataSequential>(gvvi)) {
            for (unsigned ii = 0, e = cds->getNumElements(); ii != e; ++ii) {
              check_function_pointer(cds->getElementAsConstant(ii), visited);
            }
          }
        }
      } else {
        yhao_log(2, "check_function_pointer: not handle else case!");
        return;
      }
    } else if (auto *ce = llvm::dyn_cast<llvm::ConstantExpr>(c)) {
      if (ce->getOpcode() == llvm::Instruction::BitCast) {
        c = ce->getOperand(0);
      } else if (ce->getOpcode() == llvm::Instruction::GetElementPtr) {
        c = ce->getOperand(0);
      } else {
        yhao_log(DEBUG_FUNCTION_POINTER,
                 "check_function_pointer: ConstantExpr: else");
        yhao_dump(DEBUG_FUNCTION_POINTER, ce->print, str) return;
      }
    } else if (const llvm::ConstantVector *cp =
                   llvm::dyn_cast<llvm::ConstantVector>(c)) {
      for (unsigned ii = 0, e = cp->getNumOperands(); ii != e; ++ii) {
        check_function_pointer(cp->getOperand(ii), visited);
      }
      return;
    } else if (const llvm::ConstantArray *ca =
                   llvm::dyn_cast<llvm::ConstantArray>(c)) {
      for (unsigned ii = 0, e = ca->getNumOperands(); ii != e; ++ii) {
        check_function_pointer(ca->getOperand(ii), visited);
      }
      return;
    } else if (const llvm::ConstantStruct *cs =
                   llvm::dyn_cast<llvm::ConstantStruct>(c)) {
      for (unsigned ii = 0, e = cs->getNumOperands(); ii != e; ++ii) {
        check_function_pointer(cs->getOperand(ii), visited);
      }
      return;
    } else if (const llvm::ConstantDataSequential *cds =
                   llvm::dyn_cast<llvm::ConstantDataSequential>(c)) {
      for (unsigned ii = 0, e = cds->getNumElements(); ii != e; ++ii) {
        check_function_pointer(cds->getElementAsConstant(ii), visited);
      }
      return;
    } else {
      yhao_log(DEBUG_FUNCTION_POINTER, "check_function_pointer: else");
      yhao_dump(DEBUG_FUNCTION_POINTER, c->print, str) return;
    }
  }
}

sd::callee *sd::T_function::add_callee(llvm::Instruction *i, sd::callee_type ct,
                                       T_function *tf) {
  auto temp_c = new callee();
  temp_c->i = i;
  temp_c->ct = ct;
  if (tf == nullptr) {
    temp_c->tf_set = new std::set<T_function *>;
  } else {
    temp_c->tf = tf;
  }
  this->all_callee.push_back(temp_c);
  this->all_callee_map[i] = temp_c;
  return temp_c;
}

// only for function pointer
sd::callee *sd::T_function::add_callee(llvm::Instruction *i, sd::callee_type ct,
                                       std::set<T_function *> &tf) {
  if (tf.empty()) {
    return nullptr;
  }
  auto temp_c = new callee();
  temp_c->i = i;
  temp_c->ct = ct;
  temp_c->tf_set = new std::set<T_function *>;
  for (auto temp : tf) {
    temp_c->tf_set->insert(temp);
  }
  this->all_callee.push_back(temp_c);
  return temp_c;
}

void sd::T_function::add_checker_result(sd::checker *c, llvm::Instruction *i) {
  int64_t debug = DEBUG_CHECKER;
  checker_result *cr1 = make_checker(c->ct);
  cr1->checker::copy(c);
  cr1->setInst(i);

  this->checker_results.push_back(cr1);

  yhao_log(debug, "function: " + this->llvm_function->getName().str());
  yhao_log(debug, "checker: " + std::to_string(cr1->ct) + " :" +
                      checker_type_name[cr1->ct]);
  yhao_log(debug, inst_to_strID(i));
}

void sd::T_function::reset() {
  // reset for check k
  for (auto ce : all_callee) {
    ce->checked = false;
  }
  for (auto cr : this->checker_results) {
    delete cr;
  }
  this->checker_results.clear();

  //    // reset for check callee
  //    this->num_checkers_check_callee = -1;
  //    for (auto s: all_callee_checker_results) {
  //        delete s.second;
  //    }
  //    this->all_callee_checker_results.clear();

  // reset for check caller
  this->num_checkers_check_caller = 0;
}

uint64_t sd::T_function::calculate_path(llvm::BasicBlock *start_node,
                                        llvm::BasicBlock *end_node,
                                        std::vector<llvm::BasicBlock *> *path) {
  // TODO: later: more effective path?
  path->push_back(end_node);
  return 0;
}

uint64_t sd::T_function::calculate_path(llvm::BasicBlock *start_node,
                                        std::set<llvm::BasicBlock *> *end_node,
                                        std::vector<llvm::BasicBlock *> *path) {
  uint64_t ret = 0;
  uint64_t size = end_node->size();
  if (size == 0) {
    yhao_log(3, "end_node->size() == 0");
    ret = 1;
  }
  // 1 indirect function call or 1 pair device & driver or ...
  else if (size == 1) {
    calculate_path(start_node, *(end_node->begin()), path);
  }
  // m pairs device & driver
  // get an order based on reachable relationship
  else {
    yhao_log(2, "end_node->size() >= 1");
    auto *pass_node = new std::vector<llvm::BasicBlock *>;
    for (auto temp_b1 : *end_node) {
      for (auto it = pass_node->begin(); it != pass_node->end(); it++) {
        if (llvm::isPotentiallyReachable(&temp_b1->front(), &(*it)->front(),
                                         nullptr, this->dominator_tree)) {
          pass_node->insert(it, temp_b1);
          break;
        } else if (llvm::isPotentiallyReachable(&(*it)->front(),
                                                &temp_b1->front(), nullptr,
                                                this->dominator_tree)) {
          continue;
        } else {
          ret = 1;
          yhao_log(3, "llvm::isPotentiallyReachable(): not reachable");
        }
      }
    }

    auto temp_b1 = start_node;
    path->push_back(start_node);
    for (auto temp_b2 : *pass_node) {
      calculate_path(temp_b1, temp_b2, path);
      temp_b1 = temp_b2;
    }
  }
  return ret;
}

uint64_t sd::T_function::calculate_path(
    std::set<llvm::Instruction *> *pass_node_i,
    std::vector<llvm::BasicBlock *> *path) {
  if (this->dominator_tree == nullptr) {
    this->dominator_tree = new llvm::DominatorTree(*this->llvm_function);
  }

  // filter the basic block by dominate relationship, only keep the dominated
  // basic block
  auto *pass_node_b = new std::set<llvm::BasicBlock *>;
  for (auto i : *pass_node_i) {
    auto temp_b = i->getParent();
    if (pass_node_b->find(temp_b) == pass_node_b->end()) {
      for (auto b : *pass_node_b) {
        // only insert dominated basic block, so all those basic blocks would be
        // reached
        if (this->dominator_tree->dominates(b, temp_b)) {
          pass_node_b->insert(temp_b);
          pass_node_b->erase(b);
        } else if (this->dominator_tree->dominates(temp_b, b)) {
        }
        // no dominate relationship, insert the temp_b
        else {
          pass_node_b->insert(temp_b);
        }
      }
    }
  }

  uint64_t ret = calculate_path(&(this->llvm_function->getEntryBlock()),
                                pass_node_b, path);
  return ret;
}

uint64_t sd::T_function::calculate_path(
    std::set<llvm::Instruction *> *pass_node_i, std::vector<uint64_t> *path) {
  auto *basic_bloc_path = new std::vector<llvm::BasicBlock *>;
  uint64_t ret = calculate_path(pass_node_i, basic_bloc_path);

  // TODO: later: basic_bloc_path --> path
  yhao_log(3, "TODO: basic_bloc_path --> path");

  return ret;
}

std::string sd::T_function::get_name() const {
  return this->llvm_function->getName().str();
}

void sd::T_function::add_callee_checker_results(llvm::Instruction *i,
                                                sd::T_function *t_f) {
  std::set<sd::T_function *> *temp;
  if (all_callee_checker_results.find(i) == all_callee_checker_results.end()) {
    temp = new std::set<sd::T_function *>;
    all_callee_checker_results[i] = temp;
  } else {
    temp = all_callee_checker_results[i];
  }
  temp->insert(t_f);
}
