//
// Created by yhao on 3/16/21.
//

#include "T_module.h"
#include "../ToolLib/log.h"
#include "../ToolLib/llvm_related.h"

#include "../MLTA/Analyzer.hh"
#include "../MLTA/CallGraph.hh"
#include "../MLTA/TypeInitializer.hh"

#define DEBUG_CHECK -1

sd::T_module::T_module() {
    uint64_t order = 0;
    for (const auto &temp: module_init_name) {
        auto temp_1 = new module_init_order();
        temp_1->order = order++;
        temp_1->name = "module_init_function_" + temp;
        temp_1->f = new std::set<llvm::Function *>();
        module_init_function[temp] = temp_1;
    }

    auto temp_1 = new module_init_order();
    temp_1->order = order++;
    temp_1->name = "module_init_function_" + module_name;
    temp_1->f = new std::set<llvm::Function *>();
    module_init_function[module_name] = temp_1;
}

sd::T_module::~T_module() = default;

void sd::T_module::read_bitcode(const std::string &BitcodeFileName) {
    yhao_log(1, "*************************************************");
    yhao_log(1, "****************ReadIR***************************");

    this->bitcode = BitcodeFileName;
    llvm::LLVMContext *cxts;
    llvm::SMDiagnostic Err;
    cxts = new llvm::LLVMContext[1];
    llvm_module = llvm::parseIRFile(BitcodeFileName, Err, cxts[0]);

    if (!llvm_module) {
        yhao_log(3, "load T_module: " + BitcodeFileName + " failed");
        exit(0);
    } else {

    }

    cg = new llvm::CallGraph(*llvm_module);

    for (auto &f: *this->llvm_module) {
        auto ff = new class T_function(&f);
        ff->t_module = this;
        this->t_functions[&f] = ff;
    }

    yhao_log(1, "****************check all function start*********");
    for (auto ff: this->t_functions) {
        ff.second->check();
    }
    yhao_log(1, "****************check all function end***********");

    yhao_log(1, "****************indirect functions start*********");
    MLTA_indirect_functions();
    //type_based_indirect_functions();
    yhao_log(1, "****************indirect functions end***********");
}

void sd::T_module::analysis_functions() {
    yhao_log(1, "****************analysis functions start*********");
    for (auto ff: this->t_functions) {
        analysis_functions(ff.second);
    }
    for (auto ff: this->t_functions) {
        check_caller(ff.second);
    }
    yhao_log(1, "****************analysis functions end***********");
}

void sd::T_module::analysis_functions(T_function *f) {
    f->check(this->k);
}

void sd::T_module::update_function() {
    for (auto &tf: this->t_functions) {
        tf.second->reset();
    }
    analysis_functions();
}

void sd::T_module::update_fp_in_init() {
    // add function pointer from all init function
    for (auto temp_init_f: init_function) {
        auto temp_init_t_f = this->find_t_function(temp_init_f);
        if (temp_init_f == nullptr) {
            continue;
        }
        for (auto ce: temp_init_t_f->all_callee) {
            if (ce->ct == callee_type::function_pointer) {
                for (auto temp_fp: *ce->tf_set) {
                    fp_in_init.insert(temp_fp->llvm_function);
                }
            }
        }
    }
}

void sd::T_module::type_based_indirect_functions() {
    std::string str;

    // number based indirect call
    for (auto temp_f: this->t_functions) {
        auto f = temp_f.second->llvm_function;
        if (f->hasAddressTaken()) {
            auto *ft = f->getFunctionType();
            if (this->map_function_type.find(ft) == this->map_function_type.end()) {
                auto set_function = new std::set<T_function *>;
                set_function->insert(temp_f.second);
                this->map_function_type[ft] = set_function;
            } else {
                this->map_function_type[ft]->insert(temp_f.second);
            }
        }
    }
    for (auto ff: this->t_functions) {
        // number based indirect call
        for (auto ce: ff.second->all_callee) {
            if (ce->ct != callee_type::callee_indirect) {
                continue;
            }
            if (!ce->tf_set->empty()) {
                continue;
            }
            auto ci = llvm::cast<llvm::CallInst>(ce->i);
            llvm::Type *t = ci->getCalledOperand()->getType();
            auto *ft = llvm::cast<llvm::FunctionType>(
                    llvm::cast<llvm::PointerType>(t)->getNonOpaquePointerElementType());
            std::set<T_function *> *set_function;
            if (this->map_function_type.find(ft) == this->map_function_type.end()) {
                set_function = new std::set<T_function *>;
                this->map_function_type[ft] = set_function;
                yhao_log(2, "wrong function number:");
                yhao_dump(2, ci->print, str)
                yhao_log(2, inst_to_strID(ce->i));
            } else {
                set_function = this->map_function_type[ft];
            }
            for (auto &temp_f: *set_function) {
                ce->tf_set->insert(temp_f);
                temp_f->caller_indirect.insert(ff.second);
            }
        }
    }
}

void sd::T_module::MLTA_indirect_functions() {
    std::string str;
    int64_t debug = -1;

    GlobalContext GlobalCtx;
    auto MName = StringRef(bitcode);
    GlobalCtx.Modules.push_back(make_pair(llvm_module.get(), MName));
    GlobalCtx.ModuleMaps[llvm_module.get()] = bitcode;

    // Initialize global number map
    TypeInitializerPass TIPass(&GlobalCtx);
    TIPass.run(GlobalCtx.Modules);
    TIPass.BuildTypeStructMap();

    // Build global call graph.
    CallGraphPass CGPass(&GlobalCtx);
    CGPass.run(GlobalCtx.Modules);

    for (auto ff: t_functions) {
        for (auto ce: ff.second->all_callee) {
            if (ce->ct != callee_type::callee_indirect) {
                continue;
            }

            yhao_print(debug, ce->i->print, str)
            yhao_log(debug, "mlta ce->i: " + str);
            auto *ci = llvm::cast<llvm::CallInst>(ce->i);
            auto callee = GlobalCtx.Callees[ci];
            for (auto temp_f: callee) {
                yhao_log(-1, "callee: " + temp_f->getName().str());
                auto temp_ff = t_functions[temp_f];
                if (temp_ff == nullptr) {
                    yhao_log(2, "function but no t_function: " + temp_f->getName().str());
                    continue;
                }
                ce->tf_set->insert(temp_ff);
                temp_ff->caller_indirect.insert(ff.second);
            }
        }
    }
}

// for all indirect function call, remove module init functions from targets
// because it is impossible to call module init functions from them
void sd::T_module::remove_init_from_indirect() {
    for (const auto &temp: module_init_function) {
        for (auto temp1: *temp.second->f) {
            if (t_functions.find(temp1) == t_functions.end()) {
                yhao_log(3, "never possible");
            }
            auto t_f = t_functions[temp1];
            for (auto temp2: t_f->caller_indirect) {
                for (auto temp3: temp2->all_callee) {
                    if (temp3->ct == callee_type::callee_indirect) {
                        if (temp3->tf_set->find(t_f) != temp3->tf_set->end()) {
                            temp3->tf_set->erase(t_f);
                            goto next;
                        }
                    }
                }
                next:
                continue;
            }
            t_f->caller_indirect.clear();
        }
    }
}

// use MLTA to find targets function
int64_t sd::T_module::get_callee(llvm::Instruction *inst, std::set<T_function *> *targets) {
    std::string str;
    if (targets == nullptr) {
        yhao_log(2, "get_callee: targets == nullptr");
        return -1;
    }

    auto bb = inst->getParent();
    auto f = bb->getParent();

    if (this->t_functions.find(f) == this->t_functions.end()) {
        yhao_log(2, "get_callee: this->t_functions.find(f) == this->t_functions.end()");
        return -1;
    }
    auto t_f = this->t_functions[f];
    if (t_f->all_callee_map.find(inst) == t_f->all_callee_map.end()) {
        yhao_dump(-1, inst->print, str)
        yhao_log(-1, "get_callee: t_f->all_callee_map.find(inst) == t_f->all_callee_map.end()");
        return -1;
    }
    auto ce = t_f->all_callee_map[inst];
    if (ce->ct == callee_type::callee_not_isDeclaration ||
        ce->ct == callee_type::callee_isDeclaration) {
        if (ce->tf == nullptr) {
            yhao_log(3, "get_callee: ce->tf == nullptr");
            return -1;
        } else {
            targets->insert(ce->tf);
        }
    } else if (ce->ct == callee_type::callee_indirect) {
        if (ce->tf_set == nullptr) {
            yhao_log(3, "get_callee: ce->tf_set == nullptr");
            return -1;
        } else {
            for (auto ff: *ce->tf_set) {
                targets->insert(ff);
            }
        }
    } else {
        yhao_log(2, "get_callee: !ce->ct == callee_type::callee_indirect");
        return -1;
    }
    return 0;
}


// use MLTA to find targets function and filter it by given fd set
int64_t sd::T_module::get_callee(llvm::Instruction *inst, std::set<llvm::Function *> *targets,
                                 std::set<llvm::Function *> *fp) {
    if (targets == nullptr) {
        yhao_log(2, "get_callee: targets == nullptr");
        return 1;
    }

    std::set<T_function *> temp;
    int64_t ret;
    ret = get_callee(inst, &temp);
    if (ret == 0) {
        for (auto f: temp) {
            targets->insert(f->llvm_function);
        }
    }

    if (fp != nullptr && !fp->empty()) {
        std::set<llvm::Function *> temp_1;
        for (auto f: *targets) {
            if (fp->find(f) == fp->end()) {
                continue;
            } else {
                temp_1.insert(f);
            }
        }
        // if (temp_1.empty()) {
        // } else {
        targets->clear();
        targets->insert(temp_1.begin(), temp_1.end());
        // }
    }

    return ret;
}

int64_t sd::T_module::get_callee(llvm::Instruction *inst, std::set<llvm::Function *> *targets,
                                 std::set<T_function *> *fp) {
    std::set<llvm::Function *> temp;
    if (fp != nullptr && !fp->empty()) {
        for (auto temp_t_f: *fp) {
            temp.insert(temp_t_f->llvm_function);
        }
    }
    return get_callee(inst, targets, &temp);
}

int64_t sd::T_module::find_ops_structure(const std::string &ops_structure, const std::string &name,
                                         llvm::GlobalVariable **ops) const {
    std::string str;
    if (name.empty()) {
        return 1;
    }
    *ops = this->llvm_module->getNamedGlobal(name);
    if (*ops == nullptr) {
        yhao_log(3, "not find ops_structure:" + ops_structure);
        yhao_log(3, "not find name:" + name);
        return 1;
    }
    yhao_dump(-1, (*ops)->print, str)
    return 0;
}

void sd::T_module::find_module_init_function() {
    int64_t debug = -1;
    std::string module_init_start = "__initcall_";
    std::string module_init_next = "\n";
    uint64_t pos_start = 0;
    uint64_t pos_end = 0;
    uint64_t pos_next;
    uint64_t size_m = module_init_start.size();
    std::string info;
    std::string module_asm = llvm_module->getModuleInlineAsm();

    std::ofstream temp_of(this->work_dir + "module_inline_asm.s", std::ios_base::out);
    temp_of << module_asm << std::endl;
    temp_of.close();

    auto get_function = [&, this]() {
        uint64_t get_function_size;
        uint64_t get_function_pos_start;
        get_function_size = pos_end - pos_start - size_m;
        std::string init_asm = module_asm.substr(pos_start + size_m, pos_end - pos_start - size_m);
        get_function_pos_start = init_asm.find("__");
        get_function_pos_start = init_asm.find('_', get_function_pos_start + 2);
        get_function_pos_start = init_asm.find('_', get_function_pos_start + 1);
        std::string function_name = init_asm.substr(get_function_pos_start + 1,
                                                    get_function_size - get_function_pos_start - 1);
        yhao_log(debug, info + "function_name: " + function_name);
        llvm::Function *f = llvm_module->getFunction(function_name);
        if (!f) {
            yhao_log(2, "find_module_init_function : can not find module_init T_function :" + function_name);
        } else {
            yhao_log(debug, "find_module_init_function : find module_init T_function :" + f->getName().str());
        }
        return f;
    };

    pos_start = module_asm.find(module_init_start, 0);
    while (pos_start != std::string::npos) {
        pos_next = module_asm.find(module_init_next, pos_start + 1);

        for (const auto &temp: module_init_name) {
            std::string name = temp + ":";
            pos_end = module_asm.find(name, pos_start + 1);
            if (pos_end != std::string::npos && pos_end < pos_next) {
                info = "find_module_init_function_" + temp + ": ";
                auto f = get_function();
                if (!f) {
                    continue;
                }
                auto fv = module_init_function[temp]->f;
                auto it = init_function.begin(), ie = init_function.end();
                while (it != ie && fv->find(f) != fv->end()) {
                    if (get_real_function_name(*it) == get_real_function_name(f)) {
                        f = *it;
                    }
                    it++;
                }
                yhao_log(debug, info + "real function_name: " + f->getName().str());
                fv->insert(f);
                goto find;
            }
        }
        if (pos_next < module_asm.size()) {
            yhao_log(1, "find_module_init_function : can not find module_init T_function: " +
                        module_asm.substr(pos_start, pos_next - pos_start));
        }
        find:
        pos_start = module_asm.find(module_init_start, pos_start + 1);
    }

    for (auto &gv: this->llvm_module->getGlobalList()) {
        auto sec = gv.getSection().str();
        for (const auto &temp: module_init_name) {
            auto name = ".initcall" + temp + ".init";
            if (sec.find(name) != std::string::npos) {
                auto func = llvm::cast<llvm::Function>(gv.getOperand(0));;
                auto fv = module_init_function[temp]->f;
                auto it = init_function.begin(), ie = init_function.end();
                while (it != ie && fv->find(func) != fv->end()) {
                    if (get_real_function_name(*it) == get_real_function_name(func)) {
                        func = *it;
                    }
                    it++;
                }
                yhao_log(debug, info + "real function_name: " + func->getName().str());
                fv->insert(func);
            }
        }
    }

    std::string str;
    auto ga = this->llvm_module->getNamedAlias(module_name);
    if (ga == nullptr) {
        return;
    }
    auto func = llvm::cast<llvm::Function>(ga->getAliasee());
    if (func == nullptr) {
        return;
    }
    auto fv = module_init_function[module_name]->f;
    auto it = init_function.begin(), ie = init_function.end();
    while (it != ie && fv->find(func) != fv->end()) {
        if (get_real_function_name(*it) == get_real_function_name(func)) {
            func = *it;
        }
        it++;
    }
    yhao_log(debug, info + "real function_name: " + func->getName().str());
    fv->insert(func);
}

void sd::T_module::find_init_and_exit_function() {
    std::string section_init = ".init.text";
    std::string section_exit = ".exit.text";

    for (auto &f: llvm_module->getFunctionList()) {
        if (f.getSection().find(section_init) != std::string::npos) {
            yhao_log(-1, "find_init_and_exit_function : section_init function_name : " + f.getName().str());
            init_function.push_back(&f);
        } else if (f.getSection().find(section_exit) != std::string::npos) {
            exit_function.push_back(&f);
        } else {

        }
    }

    this->update_fp_in_init();

}

int64_t sd::T_module::check_callee(T_function *t_f) {
    int64_t debug = DEBUG_CHECK;
    // loop all function once for all entry function
    // if there is caller1 and caller2, skip analysis of caller2
    // but save results of caller2
    if (t_f->num_checkers_check_callee > 0) {
        // copy all checker_results to caller if existing
        yhao_log(debug, "t_f->num_checkers_check_callee > 0: " + t_f->get_name());
        return t_f->num_checkers_check_callee;
    }
        // if has been analyzed before and no checkers, directly return
        // somehow, it would be a bug if the case is a loop when finally the num_checkers_check_callee > 0
    else if (t_f->num_checkers_check_callee == 0) {
        yhao_log(debug, "t_f->num_checkers_check_callee == 0: " + t_f->get_name());
        return 0;
    }
        // set the num_checkers_check_callee to 0
        // so that if it would be analyzed in callee, directly return
    else if (t_f->num_checkers_check_callee == -1) {
        t_f->num_checkers_check_callee = 0;
    }

    yhao_log(debug, "check_callee start: " + t_f->get_name());
    t_f->num_checkers_check_callee += (int64_t) t_f->checker_results.size();

    for (auto ce: t_f->all_callee) {
        if (ce->ct == callee_type::callee_not_isDeclaration) {
            // skip the callee function because it has been checked by k
            if (ce->checked) {
                continue;
            }
            auto temp_f = ce->tf;
            yhao_log(debug, "callee_not_isDeclaration: " + temp_f->get_name());
            auto temp_ret = check_callee(temp_f);
            if (temp_ret > 0) {
                t_f->num_checkers_check_callee += temp_ret;
                t_f->add_callee_checker_results(ce->i, temp_f);
            }
        } else if (ce->ct == callee_type::callee_indirect) {
            // check all possible indirect call, hope there is no FN
            for (auto temp_f: *ce->tf_set) {
                yhao_log(debug, "callee_indirect: " + temp_f->get_name());
                auto temp_ret = check_callee(temp_f);
                if (temp_ret > 0) {
                    t_f->num_checkers_check_callee += temp_ret;
                    t_f->add_callee_checker_results(ce->i, temp_f);
                }
            }
        } else if (ce->ct == callee_type::function_pointer) {
            // a heuristic way to handle register and probe function
            for (auto temp_f: *ce->tf_set) {
                if (temp_f->get_name().find("probe") != std::string::npos) {
                    yhao_log(debug, "function pointer: " + temp_f->get_name());
                    auto temp_ret = check_callee(temp_f);
                    if (temp_ret > 0) {
                        t_f->num_checkers_check_callee += temp_ret;
                        t_f->add_callee_checker_results(ce->i, temp_f);
                    }
                }
            }
        }
    }
    yhao_log(debug, "check_callee end: " + t_f->get_name());

    int64_t ret = t_f->num_checkers_check_callee;
    return ret;
}

int64_t sd::T_module::check_caller(T_function *t_f) {
    int64_t debug = DEBUG_CHECK;
    std::string str;
    yhao_log(debug, "check_caller: " + t_f->get_name());
    int64_t ret = t_f->num_checkers_check_caller + (int64_t) t_f->checker_results.size();
    if (ret > 0) {
        for (auto caller: t_f->caller_not_isDeclaration) {
            yhao_log(debug, "check_caller caller_not_isDeclaration: " + caller->get_name() + ": " + t_f->get_name());
            for (auto temp1: caller->all_callee_checker_results) {
                for (auto temp2: *temp1.second) {
                    if (t_f == temp2) {
                        // find in caller, goto next
                        yhao_log(debug, "check_caller find in all_callee_checker_results");
                        goto next1;
                    }
                }
            }
            // not find in caller, add it
            for (auto temp1: caller->all_callee_map) {
                if (temp1.second->ct == callee_type::callee_not_isDeclaration) {
                    if (temp1.second->tf == t_f) {
                        // if checked by k skip
                        if (temp1.second->checked) {
                            yhao_log(debug, "temp1.second->checked");
                            goto next1;
                        }
                        caller->add_callee_checker_results(temp1.first, t_f);
                        // if new caller, check it recursive
                        if (caller->num_checkers_check_caller == 0) {
                            caller->num_checkers_check_caller += ret;
                            check_caller(caller);
                        } else {
                            yhao_log(debug, "not caller->num_checkers_check_caller == 0");
                            caller->num_checkers_check_caller += ret;
                        }
                        goto next1;
                    }
                }
            }
            yhao_log(debug, "check_caller not find in all_callee_map callee_not_isDeclaration");
            next1:
            continue;
        }

        for (auto caller: t_f->caller_indirect) {
            yhao_log(debug, "check_caller caller_indirect: " + caller->get_name() + ": " + t_f->get_name());
            for (auto temp1: caller->all_callee_checker_results) {
                for (auto temp2: *temp1.second) {
                    if (t_f == temp2) {
                        // find in caller, goto next
                        yhao_log(debug, "check_caller find in all_callee_checker_results");
                        goto next2;
                    }
                }
            }
            // not find in caller, add it
            for (auto temp1: caller->all_callee_map) {
                if (temp1.second->ct == callee_type::callee_indirect) {
                    for (auto temp2: *temp1.second->tf_set) {
                        if (temp2 == t_f) {
                            caller->add_callee_checker_results(temp1.first, t_f);
                            // if new caller, check it recursive
                            if (caller->num_checkers_check_caller == 0) {
                                caller->num_checkers_check_caller += ret;
                                check_caller(caller);
                            } else {
                                yhao_log(debug, "not caller->num_checkers_check_caller == 0");
                                caller->num_checkers_check_caller += ret;
                            }
                            goto next2;
                        }
                    }
                }
            }
            yhao_log(debug, "check_caller not find in all_callee_map callee_indirect");
            next2:
            continue;
        }

        for (auto caller: t_f->caller_pointer) {
            yhao_log(debug, "check_caller caller_pointer: " + caller->get_name() + ": " + t_f->get_name());
            for (auto temp1: caller->all_callee_checker_results) {
                for (auto temp2: *temp1.second) {
                    if (t_f == temp2) {
                        // find in caller, goto next
                        yhao_log(debug, "check_caller find in all_callee_checker_results");
                        goto next3;
                    }
                }
            }
            // not find in caller, add it
            for (auto temp1: caller->all_callee) {
                if (temp1->ct == callee_type::function_pointer) {
                    for (auto temp2: *temp1->tf_set) {
                        if (temp2 == t_f) {
                            caller->add_callee_checker_results(temp1->i, t_f);
                            // if new caller, check it recursive
                            if (caller->num_checkers_check_caller == 0) {
                                caller->num_checkers_check_caller += ret;
                                check_caller(caller);
                            } else {
                                yhao_log(debug, "not caller->num_checkers_check_caller == 0");
                                caller->num_checkers_check_caller += ret;
                            }
                            goto next3;
                        }
                    }
                }
            }
            yhao_log(debug, "check_caller not find in all_callee_map function_pointer");
            next3:
            continue;
        }
        yhao_log(debug, "check_caller end: " + t_f->get_name());
        return 0;
    } else {
        yhao_log(debug, "ret: " + std::to_string(ret));
        return 0;
    }
}

int64_t sd::T_module::check_open(llvm::Function *f, std::vector<checker_result *> *crs) {
    if (this->t_functions.find(f) == this->t_functions.end()) {
        return 1;
    }
    auto t_f = this->t_functions[f];
    uint64_t size = 0;
    callee *ce;
    for (auto temp_ce: t_f->all_callee) {
        if (temp_ce->ct == callee_type::callee_indirect) {
            size++;
            ce = temp_ce;
        }
    }
    if (size == 0) {
        yhao_log(3, "check_open: " + f->getName().str() + ": no indirect");
        return 1;
    } else if (size == 1) {
        auto cr = new checker_result();
        cr->ct = checker_type::open_indirect;
        cr->setInst(ce->i);
        crs->push_back(cr);
    } else if (size > 1) {
        yhao_log(3, "check_open: " + f->getName().str() + ": more than one indirect");
        return 1;
    }
    return 0;
}

int64_t sd::T_module::check_function_pointer(sd::entry_function *init_f) {
    if (init_f->count_function_pointer == 0) {
        //collect all function pointers
        // do not consider order now, so there would be some FP
        if (init_f->parent != nullptr && init_f->parent->parent != nullptr) {
            auto parent = init_f->parent->parent;
            for (auto fp: parent->function_pointers) {
                init_f->function_pointers.insert(fp);
            }
        }
        check_function_pointer(init_f, init_f->t_function);
    }

    return 0;
}

int64_t sd::T_module::check_function_pointer(sd::entry_function *init_f, T_function *t_f) {
    int64_t debug = -1;

    //if (init_f->checked_function.find(t_f) != init_f->checked_function.end()) {
    //    return 0;
    //} else {
    //    init_f->checked_function.insert(t_f);
    //}

    // skip if call chain is too long
    // possible only for kvm
    if (current_call_chain.size() > MAX_FUNC_DEPTH) {
        return 0;
    }

    if (init_f->count_function_pointer > MAX_CALLEE_COUNT) {
        return 0;
    } else {
        init_f->count_function_pointer++;
    }

    auto path = get_file_name(t_f->llvm_function);
    if (path.empty()) {
        return 0;
    }
    for (const auto &temp: skip_path) {
        if (path.find(temp) == 0) {
            return 0;
        }
    }

    if (debug > -1) {
        yhao_log(debug, "current_call_chain: ");
        for (auto temp: current_call_chain) {
            yhao_log(debug, temp->get_name());
        }
    }

    current_call_chain.push_back(t_f);

    for (auto ce: t_f->all_callee) {
        if (ce->ct == callee_type::function_pointer) {
            for (auto fp: *ce->tf_set) {
                init_f->function_pointers.insert(fp);
            }
        } else if (ce->ct == callee_type::callee_not_isDeclaration) {
            if (!ce->checked) {
                check_function_pointer(init_f, ce->tf);
            }
        } else if (ce->ct == callee_type::callee_indirect) {
            // statistic
            // indirect call
            init_f->number_indirect_call++;
            //yhao_log(1, "indirect call: " + init_f->get_entry_function_name());
            //yhao_log(1, "indirect call: " + dump_inst(ce->i));
            for (auto callee: *ce->tf_set) {
                init_f->number_indirect_call_target_before++;
                if (init_f->function_pointers.find(callee) == init_f->function_pointers.end()) {
                    continue;
                }
                init_f->number_indirect_call_target_after++;
                //yhao_log(1, "indirect call target: " + callee->get_name());
                check_function_pointer(init_f, callee);
            }
        }
    }

    for (auto ce: t_f->all_callee) {
        if (ce->ct == callee_type::function_pointer) {
            // current, not handle order of function pointer in callee set
            for (auto callee: *ce->tf_set) {
                for (const auto &name: possible_name) {
                    if (callee->llvm_function->getName().contains(name)) {
                        check_function_pointer(init_f, callee);
                    }
                }
            }
        }
    }

    current_call_chain.pop_back();
    return 0;
}

void sd::T_module::set_function_flag(llvm::Function *f, sd::init_flag flag) {
    // reduce repeat work
    if ((function_flag.find(f) != function_flag.end()) && (function_flag[f] & flag)) {

    } else {
        function_flag[f] = flag;
        if (!f->isDeclaration()) {
            for (auto &cf: *(*this->cg)[f]) {
                if (cf.second->getFunction() != nullptr) {
                    set_function_flag(cf.second->getFunction(), flag);
                }
            }
        }
    }
}

[[maybe_unused]] void sd::T_module::get_init_and_exit_function_number() {
    uint64_t total_basic_block_number = 0;
    std::cout << "this->llvm_module->size() : " << this->llvm_module->size() << std::endl;
    for (auto &f: this->llvm_module->functions()) {
        total_basic_block_number += f.size();
    }
    std::cout << "total_basic_block_number : " << total_basic_block_number << std::endl;

    uint64_t total_init_basic_block_number = 0;
    std::cout << "this->T_function.size() : " << this->init_function.size() << std::endl;
    for (auto f: this->init_function) {
        total_init_basic_block_number += f->size();
    }
    std::cout << "total_init_basic_block_number : " << total_init_basic_block_number << std::endl;

    uint64_t total_exit_basic_block_number = 0;
    std::cout << "this->exit_function.size() : " << this->exit_function.size() << std::endl;
    for (auto f: this->exit_function) {
        total_exit_basic_block_number += f->size();
    }
    std::cout << "total_exit_basic_block_number : " << total_exit_basic_block_number << std::endl;

    for (auto f: this->init_function) {
        set_function_flag(f, init_flag::INIT_FLAG_INIT_EXIT);
    }
    for (auto f: this->exit_function) {
        set_function_flag(f, init_flag::INIT_FLAG_INIT_EXIT);
    }
    for (auto &f: this->llvm_module->functions()) {
        // do not set init T_function
        if ((function_flag.find(&f) != function_flag.end()) &&
            (function_flag[&f] & init_flag::INIT_FLAG_INIT_EXIT)) {

        } else {
            set_function_flag(&f, init_flag::INIT_FLAG_OTHER);
        }
    }

    uint64_t total_init_exit_callee_basic_block_number = 0;
    for (auto f: this->function_flag) {
        if (f.second == init_flag::INIT_FLAG_INIT_EXIT) {
            total_init_exit_callee_basic_block_number += f.first->size();
        }
    }
    std::cout << "total_init_exit_callee_basic_block_number : " << total_init_exit_callee_basic_block_number
              << std::endl;

}

// return the number of paths in the T_function, inter-procedural,
void sd::T_module::get_path_number(llvm::Function *f, std::set<llvm::Function *> *call, uint64_t *path_number,
                                   uint64_t *b_number) {

    if (f == nullptr || f->isDeclaration()) {
        return;
    }

    for (auto &b: *f) {
        if (b.getTerminator()->getNumSuccessors() > 1) {
            *b_number += 1;
        }
    }
    get_path_number(&(f->getEntryBlock()), new std::set<llvm::BasicBlock *>, path_number);

    for (auto &cf: *(*this->cg)[f]) {
        if (cf.second->getFunction() != nullptr && !cf.second->getFunction()->isDeclaration()) {
            if (call->find(cf.second->getFunction()) == call->end()) {
                call->insert(cf.second->getFunction());

                std::string name = cf.second->getFunction()->getName().str();
                if (no_path_functions.find(name) == no_path_functions.end()) {
                    get_path_number(cf.second->getFunction(), call, path_number, b_number);
                }
            }
        }
    }
}

void
sd::T_module::get_path_number(llvm::BasicBlock *b, std::set<llvm::BasicBlock *> *path,
                              uint64_t *path_number) {
    if (b->getTerminator()->getNumSuccessors() == 0) {
        path->clear();
    } else if (b->getTerminator()->getNumSuccessors() == 1) {
        path->insert(b->getSingleSuccessor());
        get_path_number(b->getSingleSuccessor(), path, path_number);
    } else if (b->getTerminator()->getNumSuccessors() == 2) {
        if (path->find(b) == path->end() || path->find(b->getTerminator()->getSuccessor(0)) == path->end()) {
            *path_number += 1;
            auto *new_path = new std::set<llvm::BasicBlock *>;
            for (auto p: *path) {
                new_path->insert(p);
            }
            path->insert(b->getTerminator()->getSuccessor(0));
            new_path->insert(b->getTerminator()->getSuccessor(1));
            get_path_number(b->getTerminator()->getSuccessor(0), path, path_number);
            get_path_number(b->getTerminator()->getSuccessor(1), new_path, path_number);
        } else {
            // loop
            path->insert(b->getTerminator()->getSuccessor(1));
            get_path_number(b->getTerminator()->getSuccessor(1), path, path_number);
        }
    } else {
        *path_number += 1;
    }
}

sd::T_function *sd::T_module::find_t_function(llvm::Function *f) {
    if (this->t_functions.find(f) == this->t_functions.end()) {
        yhao_log(3, "can not find function: " + f->getName().str());
        return nullptr;
    }
    return this->t_functions[f];
}
