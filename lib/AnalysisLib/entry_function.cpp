//
// Created by yu on 6/15/21.
//

#include "entry_function.h"
#include "../ToolLib/log.h"
#include "../ToolLib/llvm_related.h"

sd::entry_function::entry_function() = default;

sd::entry_function::entry_function(sd::T_function *t_function) {
    this->t_function = t_function;
}

void sd::entry_function::reset() {
    for (auto c: this->all_checker_results) {
        delete c;
    }
    this->all_checker_results.clear();
    for (auto v: this->checker_results) {
        delete v.second;
    }
    this->checker_results.clear();
    count_update_checker = 0;
}

// only update checker from callee
void sd::entry_function::update_checker_results() {
    int64_t debug = 0;
    reset();
    yhao_log(debug, "****update_checker_results(t_function); start*****************");
    update_checker_results(t_function);
    yhao_log(debug, "****update_checker_results(t_function); end*******************");
    for (auto cr: all_checker_results) {
        cr->compute_hash();
    }
}

void sd::entry_function::update_checker_results(T_function *t_f) {
    int64_t debug = -1;

    // skip if call chain is too long
    // possible only for kvm
    if (current_call_chain.size() > MAX_FUNC_DEPTH) {
        return;
    }
    if (count_update_checker > MAX_CALLEE_COUNT) {
        return;
    } else {
        count_update_checker++;
    }

    auto path = get_file_name(t_f->llvm_function);
    if (path.empty()) {
        return;
    }
    for(const auto& temp : skip_path) {
        if (path.find(temp) == 0) {
            return;
        }
    }

    std::string real_name = get_real_function_name(t_f->llvm_function);
    for (auto temp: current_call_chain) {
        if (get_real_function_name(temp->getFunction()) == real_name) {
            yhao_log(debug, "update_checker_results checked: " + t_f->get_name());
            return;
        }
    }

    yhao_log(debug, "update_checker_results start: " + t_f->get_name());
    yhao_log(debug, "checker_results: " + std::to_string(checker_results.size()));
    for (auto cr: t_f->checker_results) {
        auto cr1 = cr->copy();
        for (auto i: current_call_chain) {
            cr1->push_call_chain(i);
        }
        cr1->has_function_pointer = in_function_pointer;
        cr1->push_call_chain(cr1->inst);
        this->all_checker_results.push_back(cr1);
        if (this->checker_results.find(cr1->ct) == this->checker_results.end()) {
            this->checker_results[cr1->ct] = new std::vector<sd::checker_result *>;
        }
        checker_results[cr1->ct]->push_back(cr1);
    }

    yhao_log(debug, "all_callee_checker_results: " + std::to_string(t_f->all_callee_checker_results.size()));
    for (auto t_f_set: t_f->all_callee_checker_results) {
        current_call_chain.push_back(t_f_set.first);
        for (auto callee_t_f: *t_f_set.second) {
            yhao_log(debug, "callee: " + t_f->get_name() + ": " + callee_t_f->get_name());
            bool is_pointer = false;
            // try to find callee
            if (t_f->all_callee_map.find(t_f_set.first) == t_f->all_callee_map.end()) {
                // callee_type::function_pointer
                is_pointer = true;
            } else {
                // other callee
                auto ce = t_f->all_callee_map[t_f_set.first];
                if (ce->ct == callee_type::callee_not_isDeclaration) {
                    if (ce->tf != callee_t_f) {
                        // callee_type::function_pointer
                        is_pointer = true;
                    }
                } else if (ce->ct == callee_type::callee_isDeclaration) {
                    if (ce->tf != callee_t_f) {
                        // callee_type::function_pointer
                        is_pointer = true;
                    }
                } else if (ce->ct == callee_type::callee_indirect) {
                    if (ce->tf_set->find(callee_t_f) == ce->tf_set->end()) {
                        // callee_type::function_pointer
                        is_pointer = true;
                    } else {
                        if (this->function_pointers.find(callee_t_f) == this->function_pointers.end()) {
                            continue;
                        }
                    }
                } else if (ce->ct == callee_type::function_pointer) {
                    if (ce->tf_set->find(callee_t_f) != ce->tf_set->end()) {
                        // callee_type::function_pointer
                        is_pointer = true;
                    }
                }
            }
            if (is_pointer) {
                // callee_type::function_pointer
                bool check = false;
                for(const auto& name : possible_name) {
                    if (callee_t_f->llvm_function->getName().contains(name)) {
                        check = true;
                    }
                }
                if (!check) {
                    continue;
                }
            } else {

            }

            // statistic
            bool old_in_function_pointer = in_function_pointer;
            if (is_pointer) {
                in_function_pointer = true;
            }

            update_checker_results(callee_t_f);

            if (is_pointer) {
                in_function_pointer = old_in_function_pointer;
            }
        }
        current_call_chain.pop_back();
    }
    yhao_log(debug, "update_checker_results end: " + t_f->get_name());
}

int64_t sd::entry_function::get_checker_results(sd::checker_type ct, std::vector<sd::checker_result *> *crs) {
    if (this->checker_results.find(ct) == this->checker_results.end()) {
        return 1;
    }
    for (auto cr: *(this->checker_results[ct])) {
        crs->push_back(cr);
    }
    return 0;
}

int64_t sd::entry_function::get_device_driver_kind() {
    yhao_log(1, "all checker result start: " + this->get_entry_function_name());
    yhao_log(1, "all checker size: " + std::to_string(this->all_checker_results.size()));
    int64_t ret = 0;
    for (auto cr: this->all_checker_results) {
        yhao_log(1, "checker result: \n" + cr->print());
        ret |= 1 << cr->ct;
    }
    yhao_log(1, "all checker result end: " + this->get_entry_function_name());
    return ret;
}

llvm::Function *sd::entry_function::get_entry_function() const {
    return this->t_function->llvm_function;
}

std::string sd::entry_function::get_entry_function_name() const {
    return this->get_entry_function()->getName().str();
}
