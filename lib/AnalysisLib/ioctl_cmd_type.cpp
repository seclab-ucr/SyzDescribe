//
// Created by yu hao on 2/5/22.
//

#include "ioctl_cmd_type.h"

#include "../ToolLib/log.h"
#include "../ToolLib/llvm_related.h"

sd::ioctl_cmd_type::ioctl_cmd_type(llvm::Function *tf,
                                   std::set <uint64_t> cmd,
                                   std::set <uint64_t> type,
                                   device_driver *dd,
                                   T_module *tm,
                                   std::set<llvm::Function *> *fps) {
    this->target_function = tf;
    this->DT = new llvm::DominatorTree(*target_function);
    uint64_t function_arg_index = 0;
    for (auto &function_arg: target_function->args()) {
        if (cmd.find(function_arg_index) != cmd.end()) {
            all_cmd_inst.insert(&function_arg);
        }
        if (type.find(function_arg_index) != type.end()) {
            all_type_inst.insert(&function_arg);
        }
        function_arg_index++;
    }
    update_cmd_and_type();

    this->tm = tm;
    this->fps = fps;
    this->dd = dd;
}

sd::ioctl_cmd_type::ioctl_cmd_type(llvm::Function *tf,
                                   std::set <uint64_t> cmd,
                                   std::set <uint64_t> type,
                                   sd::ioctl_cmd_type *caller,
                                   std::map<uint64_t, llvm::Value *> caller_arguments) {
    for (auto ci: caller->call_stack) {
        this->call_stack.push_back(ci);
    }
    this->target_function = tf;
    this->DT = new llvm::DominatorTree(*target_function);
    uint64_t function_arg_index = 0;
    for (auto &function_arg: target_function->args()) {
        if (cmd.find(function_arg_index) != cmd.end()) {
            all_cmd_inst.insert(&function_arg);
        }
        if (type.find(function_arg_index) != type.end()) {
            all_type_inst.insert(&function_arg);
        }
        if (caller_arguments.find(function_arg_index) != caller_arguments.end()) {
            this->caller_args[&function_arg] = caller_arguments[function_arg_index];
        }
        function_arg_index++;
    }
    update_cmd_and_type();

    assert(caller != nullptr);
    this->tm = caller->tm;
    this->fps = caller->fps;
    this->dd = caller->dd;
    this->current_cmd = caller->current_cmd;
    this->only_type = caller->only_type;
    this->curr_func_depth = caller->curr_func_depth + 1;
    this->caller = caller;
}

sd::ioctl_cmd_type::~ioctl_cmd_type() = default;

void sd::ioctl_cmd_type::set_only_type_module() {
    this->only_type = true;
}

void sd::ioctl_cmd_type::unset_only_type_module() {
    this->only_type = false;
}

void sd::ioctl_cmd_type::run_cmd() {
    int64_t debug = -1;
    std::string str;

    std::set < llvm::BasicBlock * > all_bb;
    for (auto &bb: *target_function) {
        all_bb.insert(&bb);
    }
    while (!all_bb.empty()) {
        auto bb = *all_bb.begin();
        all_bb.erase(bb);
        for (auto &i: *bb) {
            switch (i.getOpcode()) {
                case llvm::Instruction::Call: {
                    auto ci = llvm::dyn_cast<llvm::CallInst>(&i);
                    yhao_log(debug, "llvm::Instruction::Call");
                    yhao_dump(debug, i.print, str)
                    check_call_cmd(ci);
                    break;
                }
                case llvm::Instruction::Switch: {
                    yhao_dump(debug, i.print, str)
                    auto si = llvm::dyn_cast<llvm::SwitchInst>(&i);
                    auto targetSwitchCond = si->getCondition();
                    std::set < llvm::BasicBlock * > visited_in_switch;
                    if (!this->is_cmd_affected(targetSwitchCond)) {
                        yhao_log(debug, "is_cmd_affected: false");
                        break;
                    }
                    for (auto cis = si->case_begin(), cie = si->case_end(); cis != cie; cis++) {
                        auto cmd_val = cis->getCaseValue();
                        auto sb = cis->getCaseSuccessor();
                        std::set < llvm::BasicBlock * > visited_in_cmd;
                        auto cmd_value = cmd_val->getValue().getZExtValue();
                        yhao_log(debug, "Found Cmd:" + std::to_string(cmd_value) + ":START");
                        if (this->dd->cmd.find(cmd_value) != this->dd->cmd.end()) {

                        } else {
                            auto temp = new class cmd_info();
                            temp->i = si;
                            temp->value = cmd_value;
                            temp->b = sb;
                            temp->in_entry_function = (this->caller == nullptr);
                            temp->in_indirect_call = this->in_indirect_call;
                            this->dd->cmd[cmd_value] = temp;
                        }
                        this->current_cmd = this->dd->cmd[cmd_value];
                        run_type(sb);
                        yhao_log(debug, "Found Cmd:" + std::to_string(cmd_value) + ":END");
                        uint64_t num = 0;
                        for (auto p: predecessors(sb)) {
                            num++;
                        }
                        if (num == 1) {
                            std::set < llvm::BasicBlock * > temp;
                            temp.insert(sb);
                            get_dom_bb(sb, &temp);
                            for (auto temp_b: temp) {
                                if (all_bb.find(temp_b) != all_bb.end()) {
                                    all_bb.erase(temp_b);
                                }
                            }
                        }
                    }
                    break;
                }
                case llvm::Instruction::ICmp: {
                    auto icmp = llvm::dyn_cast<llvm::ICmpInst>(&i);
                    if (icmp->isEquality()) {
                        auto *op1 = icmp->getOperand(0);
                        auto *op2 = icmp->getOperand(1);
                        llvm::Value *icmp_value = nullptr;
                        if (this->is_cmd_affected(op1)) {
                            icmp_value = op2;
                        } else if (this->is_cmd_affected(op2)) {
                            icmp_value = op1;
                        }
                        if (icmp_value != nullptr) {
                            this->curr_cmd = icmp_value;
                        }
                    }
                    break;
                }
                case llvm::Instruction::Br: {
                    if (this->curr_cmd == nullptr) {
                        break;
                    }
                    auto *c_int = llvm::dyn_cast<llvm::ConstantInt>(this->curr_cmd);
                    this->curr_cmd = nullptr;
                    if (c_int == nullptr) {
                        break;
                    }
                    yhao_log(debug, "Found Cmd:" + std::to_string(c_int->getZExtValue()) + " (BR): START");
                    auto cmd_value = c_int->getZExtValue();
                    auto bi = llvm::dyn_cast<llvm::BranchInst>(&i);
                    auto sb = bi->getSuccessor(0);
                    if (this->dd->cmd.find(cmd_value) != this->dd->cmd.end()) {

                    } else {
                        auto temp = new class cmd_info();
                        temp->i = bi;
                        temp->value = cmd_value;
                        temp->b = sb;
                        temp->in_entry_function = (this->caller == nullptr);
                        temp->in_indirect_call = this->in_indirect_call;
                        this->dd->cmd[cmd_value] = temp;
                    }
                    this->current_cmd = this->dd->cmd[cmd_value];
                    run_type(sb);
                    yhao_log(debug, "Found Cmd:" + std::to_string(c_int->getZExtValue()) + " (BR): END");

                    uint64_t num = 0;
                    for ([[maybe_unused]] auto p: predecessors(sb)) {
                        num++;
                    }
                    if (num == 1) {
                        std::set < llvm::BasicBlock * > temp;
                        temp.insert(sb);
                        get_dom_bb(sb, &temp);
                        for (auto temp_b: temp) {
                            if (all_bb.find(temp_b) != all_bb.end()) {
                                all_bb.erase(temp_b);
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
}

void sd::ioctl_cmd_type::run_type(llvm::BasicBlock *start_bb) {
    std::set < llvm::BasicBlock * > all_bb;
    if (start_bb == &target_function->getEntryBlock()) {
        for (auto &bb: *target_function) {
            all_bb.insert(&bb);
        }
    } else {
        for (auto &bb: *target_function) {
            if (llvm::isPotentiallyReachable(start_bb, &bb, nullptr, this->DT)) {
                all_bb.insert(&bb);
            }
        }
    }

    while (!all_bb.empty()) {
        auto bb = *all_bb.begin();
        all_bb.erase(bb);
        for (auto &i: *bb) {
            switch (i.getOpcode()) {
                case llvm::Instruction::Call: {
                    auto ci = llvm::dyn_cast<llvm::CallInst>(&i);
                    check_call_type(ci);
                    break;
                }
                default: {

                }
            }
        }
    }
}

bool sd::ioctl_cmd_type::is_cmd_affected(llvm::Value *targetValue) {
    if (all_cmd_inst.find(targetValue) == all_cmd_inst.end()) {
        return all_cmd_inst.find(targetValue->stripPointerCasts()) != all_cmd_inst.end();
    }
    return true;
}

bool sd::ioctl_cmd_type::is_type_affected(llvm::Value *targetValue) {
    if (all_type_inst.find(targetValue) == all_type_inst.end()) {
        return all_type_inst.find(targetValue->stripPointerCasts()) != all_type_inst.end();
    }
    return true;
}

void sd::ioctl_cmd_type::update_cmd_and_type() {
    for (auto &bb: *target_function) {
        for (auto &i: bb) {
            if (i.getOpcode() == llvm::Instruction::Call) {
                continue;
            }
            llvm::Value *storePtrArg = nullptr;
            if (i.getOpcode() == llvm::Instruction::Store) {
                storePtrArg = llvm::dyn_cast<llvm::StoreInst>(&i)->getPointerOperand()->stripPointerCasts();
            }
            for (auto index = 0; index < i.getNumOperands(); index++) {
                auto *currValue = i.getOperand(index);
                if (currValue != nullptr) {
                    auto *strippedValue = currValue->stripPointerCasts();
                    if (all_cmd_inst.find(currValue) != all_cmd_inst.end() ||
                        all_cmd_inst.find(strippedValue) != all_cmd_inst.end()) {
                        if (all_cmd_inst.find(&i) == all_cmd_inst.end()) {
                            all_cmd_inst.insert(&i);

                        }
                        if (storePtrArg != nullptr) {
                            if (all_cmd_inst.find(storePtrArg) == all_cmd_inst.end()) {
                                all_cmd_inst.insert(storePtrArg);
                            }
                        }
                    }
                    if (all_type_inst.find(currValue) != all_type_inst.end() ||
                        all_type_inst.find(strippedValue) != all_type_inst.end()) {
                        if (all_type_inst.find(&i) == all_type_inst.end()) {
                            all_type_inst.insert(&i);
                        }

                        if (storePtrArg != nullptr) {
                            if (all_type_inst.find(storePtrArg) == all_type_inst.end()) {
                                all_type_inst.insert(storePtrArg);
                            }
                        }
                    }

                }

            }
        }
    }
}

void sd::ioctl_cmd_type::check_call_cmd(llvm::CallInst *ci) {
    int64_t debug = -1;
    std::string str;
    yhao_print(debug, ci->print, str)
    yhao_log(debug, "check_call_cmd: " + str);

    std::set < llvm::Function * > targets;

    if (ci->isInlineAsm()) {
        return;
    }
    if (this->tm == nullptr) {
        return;
    }

    if (ci->isIndirectCall()) {
        this->tm->get_callee(ci, &targets, this->fps);
    } else {
        targets.insert(get_target_function(ci->getCalledOperand()));
    }

    for (auto target: targets) {
        if (target->isDeclaration()) {

        } else {
            check_callee(ci, target);
        }
    }
}

void sd::ioctl_cmd_type::check_callee(llvm::CallInst *ci, llvm::Function *tf) {
    std::string str;
    int64_t debug = -1;
    yhao_print(debug, ci->print, str)
    yhao_log(debug, "check_callee: " + std::to_string(this->only_type) + ": " + str);
    yhao_log(debug, tf->getName().str());

    if (this->count_callee > MAX_CALLEE_COUNT) {
        return;
    } else {
        this->count_callee++;
    }
    if (this->curr_func_depth > MAX_FUNC_DEPTH) {
        return;
    }
    // we need to follow the called function, only if this is not recursive.
    if (std::find(this->call_stack.begin(), this->call_stack.end(), ci) == this->call_stack.end()) {
        std::vector < llvm::CallInst * > new_call_stack;
        new_call_stack.insert(new_call_stack.end(), this->call_stack.begin(), this->call_stack.end());
        new_call_stack.insert(new_call_stack.end(), ci);
        std::set <uint64_t> cmd;
        std::set <uint64_t> type;
        std::map < uint64_t, llvm::Value * > new_caller_args;
        // get propagation info
        this->get_arg_propagation(ci, cmd, type, new_caller_args);

        // handle bit cast in function call
        if (false && this->only_type && !type.empty()) {
            yhao_log(debug, "only_type: check function call:");
            yhao_dump(debug, ci->print, str)
            for (auto index: type) {
                yhao_log(debug, "index: " + std::to_string(index));
                auto targetOperand = tf->getArg(index);
                if (targetOperand != nullptr) {
                    yhao_dump(debug, targetOperand->print, str)

                    llvm::Type *temp_t;
                    temp_t = targetOperand->getType();
                    if (temp_t->isPointerTy() && temp_t->getNumContainedTypes()) {
                        temp_t = temp_t->getNonOpaquePointerElementType();
                    }

                    yhao_print(-1, temp_t->print, str)
                    if (str == "i1" || str == "i8" || str == "i16" || str == "i32" || str == "i64") {
                        continue;
                    }
                    if (current_cmd == nullptr) {
                        continue;
                    }
                    current_cmd->in_types[ci] = temp_t;
                }
            }
        }

        // analyze only if one of the argument is a command or argument
        if (!cmd.empty() || !type.empty()) {
            this->call_stack.push_back(ci);
            auto *child = new ioctl_cmd_type(tf, cmd, type, this,
                                             new_caller_args);
            child->in_indirect_call = this->in_indirect_call;
            if (ci->isIndirectCall()) {
                child->in_indirect_call = true;
            }
            if (child->only_type) {
                child->run_type(&child->target_function->getEntryBlock());
            } else {
                child->run_cmd();
            }
            this->call_stack.pop_back();
        }
    }
}

void sd::ioctl_cmd_type::check_call_type(llvm::CallInst *ci) {
    int64_t debug = -1;
    std::string str;
    yhao_print(debug, ci->print, str)
    yhao_log(debug, "visitCallInst: " + str);

    std::set < llvm::Function * > targets;

    if (ci->isInlineAsm()) {
        return;
    }
    if (this->tm == nullptr) {
        return;
    }

    if (ci->isIndirectCall()) {
        this->tm->get_callee(ci, &targets, this->fps);
    } else {
        targets.insert(get_target_function(ci->getCalledOperand()));
    }

    for (auto target: targets) {
        if (!target->hasName()) {
            continue;
        }
        auto function_name = target->getName().str();
        llvm::Value *targetOperand = nullptr;
        llvm::Value *srcOperand = nullptr;
        bool in_out = true;
        if (function_name.find("copy_from_user") != std::string::npos) {
            yhao_print(debug, ci->print, str)
            yhao_log(debug, "copy_from_user: " + str);
            if (ci->getNumOperands() >= 2) {
                targetOperand = ci->getArgOperand(0);
            }
            if (ci->getNumOperands() >= 3) {
                srcOperand = ci->getArgOperand(1);
            }
        } else if (function_name.find("copy_to_user") != std::string::npos) {
            if (ci->getNumOperands() >= 3) {
                targetOperand = ci->getArgOperand(1);
            }
            if (ci->getNumOperands() >= 2) {
                srcOperand = ci->getArgOperand(0);
            }
            in_out = false;
        } else if (function_name.find("memdup_user") != std::string::npos) {
            srcOperand = ci->getArgOperand(0);
            targetOperand = ci->getReturnedArgOperand();
        } else {
            if (target->isDeclaration()) {
            } else {
                this->only_type = true;
                check_callee(ci, target);
                this->only_type = false;
            }
        }
        if (srcOperand != nullptr) {
            if (!this->is_type_affected(srcOperand)) {
                yhao_log(0, "Found a copy from user from non-user argument");
                yhao_dump(-1, ci->print, str)
            }
        }
        if (targetOperand != nullptr) {
            // yu_hao: fix3: handle copy_from_user before cmd
            if (current_cmd == nullptr) {
                this->current_cmd = dd->no_cmd;
                if (in_out) {
                    this->current_cmd->in_types[targetOperand] = typeOutputHandler(targetOperand, this);
                } else {
                    this->current_cmd->out_types[targetOperand] = typeOutputHandler(targetOperand, this);
                }
            } else {
                if (in_out) {
                    this->current_cmd->in_types[targetOperand] = typeOutputHandler(targetOperand, this);
                } else {
                    this->current_cmd->out_types[targetOperand] = typeOutputHandler(targetOperand, this);
                }
            }
        }
    }
}

void sd::ioctl_cmd_type::get_dom_bb(llvm::BasicBlock *bb, std::set<llvm::BasicBlock *> *temp) {
    for (auto *c: DT->getNode(bb)->children()) {
        temp->insert(c->getBlock());
        get_dom_bb(c->getBlock(), temp);
    }
}

void sd::ioctl_cmd_type::get_arg_propagation(llvm::CallInst *ci, std::set <uint64_t> &cmd,
                                             std::set <uint64_t> &type,
                                             std::map<uint64_t, llvm::Value *> &caller_arg) {
    int curr_arg_index = 0;
    for (auto &arg: ci->args()) {
        if (this->is_cmd_affected(arg.get())) {
            cmd.insert(curr_arg_index);
        }
        if (this->is_type_affected(arg.get())) {
            type.insert(curr_arg_index);
        }
        caller_arg[curr_arg_index] = arg.get();
        curr_arg_index++;
    }
}


namespace sd {
    using namespace llvm;
    using namespace std;

    Type *get_source_type(Instruction *inst) {
        if (auto *bci = dyn_cast<BitCastInst>(inst)) {
            return bci->getSrcTy();
        } else if (auto *gepInst = dyn_cast<GetElementPtrInst>(inst)) {
            return gepInst->getSourceElementType();
        }
        return nullptr;
    }

    Type *get_val_type(llvm::Value *val, ioctl_cmd_type *currFunction) {
        val = val->stripPointerCasts();
        std::set < Value * > all_uses;
        all_uses.clear();
        for (auto us = val->use_begin(), ue = val->use_end(); us != ue; us++) {
            all_uses.insert((*us).get());
        }
        if (all_uses.size() > 1) {
            yhao_log(1, "More than one use :");
        }
        for (auto sd: all_uses) {
            auto *gv = dyn_cast<GlobalVariable>(sd);
            if (gv) {
                return gv->getType();
            }
        }

        if (currFunction) {
            if (currFunction->caller_args.find(val) != currFunction->caller_args.end()) {
                Value *callerArg = currFunction->caller_args[val];
                Type *retVal = ioctl_cmd_type::typeOutputHandler(callerArg, currFunction->caller);
                return retVal;
            }
        }
        return nullptr;
    }

    llvm::Type *ioctl_cmd_type::typeOutputHandler(llvm::Value *val, ioctl_cmd_type *currFunc) {
        int64_t debug = -1;
        std::string str = "typeOutputHandler: ";
        yhao_add_dump(debug, val->print, str)

        Type *retType = nullptr;
        if (!dyn_cast<Instruction>(val)) {
            Type *targetType = get_val_type(val, currFunc);
            if (targetType == nullptr) {
                if (dyn_cast<Instruction>(val)) {
                    std::set < Instruction * > visitedInstr;
                    visitedInstr.clear();
                    targetType = ioctl_cmd_type::get_type_recursive(val, visitedInstr);
                    if (targetType == nullptr) {
                        str = " !targetType != nullptr: ";
                        yhao_add_dump(debug, val->print, str)
                    }
                } else {
                    str = " !dyn_cast<Instruction>(val): ";
                    yhao_add_dump(debug, val->print, str)
                }
            }
            retType = targetType;
        } else {
            yhao_dump(debug, val->print, str)

            Type *targetType = get_source_type(dyn_cast<Instruction>(val));
            if (targetType == nullptr) {
                targetType = get_val_type(val, currFunc);
            }
            // yu_hao: fix2: data flow like store i8* %to, i8** %to.addr, align 8
            if (targetType == nullptr) {
                std::set < Instruction * > visitedInstr;
                visitedInstr.clear();
                auto temp = get_inst_recursive(val, visitedInstr);
                if (temp != nullptr) {
                    val = temp;
                    targetType = get_val_type(val, currFunc);
                } else {

                }
            }

            if (targetType == nullptr) {
                if (val == nullptr) {

                } else {
                    if (dyn_cast<Instruction>(val)) {
                        std::set < Instruction * > visitedInstr;
                        visitedInstr.clear();
                        targetType = ioctl_cmd_type::get_type_recursive(val, visitedInstr);
                        if (targetType == nullptr) {
                            str = " ! !targetType != nullptr: ";
                            yhao_add_dump(debug, val->print, str)
                        }
                    } else {
                        str = " ! !dyn_cast<Instruction>(val): ";
                        yhao_add_dump(debug, val->print, str)
                    }
                }
            }
            retType = targetType;
        }

        return retType;
    }

    Type *ioctl_cmd_type::get_type_recursive(Value *val, std::set<Instruction *> &visited) {
        int64_t debug = -1;
        std::string str = "get_type_recursive";
        yhao_add_dump(debug, val->print, str)
        auto *inst = llvm::dyn_cast<llvm::Instruction>(val);
        if (inst == nullptr) {
            return nullptr;
        }
        if (visited.find(inst) != visited.end()) {
            return nullptr;
        }

        visited.insert(inst);
        if (llvm::dyn_cast<llvm::LoadInst>(inst)) {
            auto *currLI = llvm::dyn_cast<llvm::LoadInst>(inst);
            llvm::Value *point_val = currLI->getPointerOperand()->stripPointerCasts();
            for (auto u: point_val->users()) {
                auto *tmpVal = llvm::dyn_cast<llvm::Value>(u);
                if (llvm::dyn_cast<llvm::StoreInst>(tmpVal)) {
                    llvm::Type *currChType = ioctl_cmd_type::get_type_recursive(tmpVal, visited);
                    if (currChType) {
                        return currChType;
                    }
                }
            }
        }
        if (llvm::dyn_cast<llvm::StoreInst>(inst)) {
            auto *currSI = llvm::dyn_cast<llvm::StoreInst>(inst);
            llvm::Value *valueOp = currSI->getValueOperand();
            if (llvm::dyn_cast<llvm::Instruction>(valueOp)) {
                llvm::Type *currChType = ioctl_cmd_type::get_type_recursive(valueOp, visited);
                if (currChType) {
                    return currChType;
                }
            }
        }
        if (llvm::dyn_cast<llvm::BitCastInst>(inst)) {
            auto *currBCI = llvm::dyn_cast<llvm::BitCastInst>(inst);
            return currBCI->getSrcTy();
        }
        if (llvm::dyn_cast<llvm::PHINode>(inst)) {
            auto *currPN = llvm::dyn_cast<llvm::PHINode>(inst);
            unsigned i = 0;
            while (i < currPN->getNumOperands()) {
                llvm::Value *targetOp = currPN->getOperand(i);
                llvm::Type *currChType = ioctl_cmd_type::get_type_recursive(targetOp, visited);
                if (currChType) {
                    return currChType;
                }
                i++;
            }
        }
        if (llvm::dyn_cast<llvm::AllocaInst>(inst)) {
            auto *currAI = dyn_cast<AllocaInst>(inst);
            return currAI->getType();
        }
        visited.erase(inst);
        return nullptr;
    }

    Value *ioctl_cmd_type::get_inst_recursive(Value *val, set<Instruction *> &visited) {
        int64_t debug = -1;
        std::string str = "get_inst_recursive";
        yhao_add_dump(debug, val->print, str)
        auto inst = llvm::dyn_cast<llvm::Instruction>(val);
        if (inst == nullptr) {
            return nullptr;
        }
        if (visited.find(inst) != visited.end()) {
            return nullptr;
        }

        visited.insert(inst);
        if (llvm::dyn_cast<llvm::LoadInst>(inst)) {
            auto currLI = llvm::dyn_cast<llvm::LoadInst>(inst);
            llvm::Value *pointerVal = currLI->getPointerOperand()->stripPointerCasts();
            for (auto u: pointerVal->users()) {
                auto *tmpVal = llvm::dyn_cast<llvm::Value>(u);
                if (llvm::dyn_cast<llvm::StoreInst>(tmpVal)) {
                    auto currInst = ioctl_cmd_type::get_inst_recursive(tmpVal, visited);
                    if (currInst) {
                        return currInst;
                    }
                }
            }
        } else if (llvm::dyn_cast<llvm::StoreInst>(inst)) {
            auto *currSI = llvm::dyn_cast<llvm::StoreInst>(inst);
            llvm::Value *valueOp = currSI->getValueOperand();
            if (llvm::dyn_cast<llvm::Instruction>(valueOp)) {
                auto currInst = ioctl_cmd_type::get_inst_recursive(valueOp, visited);
                if (currInst) {
                    return currInst;
                }
            } else {
                return valueOp;
            }
        } else if (llvm::dyn_cast<llvm::BitCastInst>(inst)) {
            auto currBCI = llvm::dyn_cast<llvm::BitCastInst>(inst);
            return currBCI;
        } else if (llvm::dyn_cast<llvm::PHINode>(inst)) {
            auto currPN = llvm::dyn_cast<llvm::PHINode>(inst);
            unsigned i = 0;
            while (i < currPN->getNumOperands()) {
                llvm::Value *targetOp = currPN->getOperand(i);
                auto currInst = ioctl_cmd_type::get_inst_recursive(targetOp, visited);
                if (currInst) {
                    return currInst;
                }
                i++;
            }
        } else if (llvm::dyn_cast<llvm::AllocaInst>(inst)) {
            auto temp_ai = llvm::dyn_cast<llvm::AllocaInst>(inst);
            return temp_ai;
        }
        visited.erase(inst);
        return nullptr;
    }
}