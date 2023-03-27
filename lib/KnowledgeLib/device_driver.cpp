//
// Created by yhao on 11/17/21.
//

#include "device_driver.h"
#include "../AnalysisLib/entry_function.h"
#include "../ToolLib/llvm_related.h"

void sd::device_driver::set_kind() {
    this->kind |= 1 << ops->ct;
    for (auto cr: this->name) {
        kind |= 1 << cr->ct;
    }
    for (auto cr: this->number) {
        kind |= 1 << cr->ct;
    }
    for (auto cr: this->file_install) {
        kind |= 1 << cr->ct;
    }
    for (auto cr: this->file_fd) {
        kind |= 1 << cr->ct;
    }
}

bool sd::device_driver::is_only_driver() const {
    return this->name.empty();
}

void sd::device_driver::update_structure_type() {
    for (auto temp: this->cmd) {
        for (auto temp_1: temp.second->in_types) {
            auto ty = get_real_type(temp_1.second);
            if (ty == nullptr) {
                continue;
            }
            auto st = llvm::dyn_cast<llvm::StructType>(ty);
            if (st == nullptr) {
                continue;
            }
            this->structures.insert(st);
        }
        for (auto temp_1: temp.second->out_types) {
            auto ty = get_real_type(temp_1.second);
            if (ty == nullptr) {
                continue;
            }
            auto st = llvm::dyn_cast<llvm::StructType>(ty);
            if (st == nullptr) {
                continue;
            }
            this->structures.insert(st);
        }
    }
    std::set<llvm::StructType *> new_type_set(this->structures);
    while (!new_type_set.empty()) {
        std::set<llvm::StructType *> temp_type_set;
        for (auto temp: new_type_set) {
            for (auto et: temp->elements()) {
                et = get_real_type(et);
                auto st = llvm::dyn_cast<llvm::StructType>(et);
                if (st == nullptr) {
                    continue;
                }
                if (this->structures.find(st) != this->structures.end()) {
                    continue;
                } else {
                    this->structures.insert(st);
                }
                temp_type_set.insert(st);
            }
        }
        new_type_set.clear();
        new_type_set.insert(temp_type_set.begin(), temp_type_set.end());
    }
}

std::string sd::device_driver::print() {
    std::string str;
    std::string ret;
    int64_t debug = 1;
    update_structure_type();
    get_resource();
    ret += "*************************************************\n";
    ret += "ops_name json: \n" + ops->to_json()->dump(2) + "\n";
    ret += "ops_name: " + ops->ops_name + "\n";
    ret += "parent: " + parent->get_entry_function_name() + "\n";
    if (non_open_parent != nullptr) {
        ret += "non_open_parent: " + std::to_string(non_open_parent->value) + "\n";
    }
    for (auto temp: name) {
        ret += "device name json: \n" + temp->to_json()->dump(2) + "\n";
        ret += "device name string: " + temp->to_string() + "\n";
    }
    if (debug >= DEBUG_LEVEL) {
        for (auto temp: cmd) {
            ret += "  cmd: " + std::to_string(temp.first) + "\n";
            for (auto temp_1: temp.second->in_types) {
                yhao_print(debug, temp_1.first->print, str)
                ret += "    in val: " + str + "\n";
                if (temp_1.second == nullptr) {

                } else {
                    yhao_print(debug, temp_1.second->print, str)
                    ret += "    in type: " + str + "\n";
                }
            }
            for (auto temp_1: temp.second->out_types) {
                yhao_print(debug, temp_1.first->print, str)
                ret += "    out val: " + str + "\n";
                if (temp_1.second == nullptr) {

                } else {
                    yhao_print(debug, temp_1.second->print, str)
                    ret += "    out type: " + str + "\n";
                }
            }
        }
        for (auto temp: this->structures) {
            yhao_print(debug, temp->print, str)
            ret += "all type: " + str + "\n";
        }
    }
    ret += "=================================================\n";
    return ret;
}

std::string sd::device_driver::get_id() {
    if (this->id.empty()) {
        if (this->parent->parent == nullptr) {
        } else {
            this->id += Annotation_Symbol"parent id hash: " + this->parent->parent->get_id_hash() + "\n";
        }
        this->id += Annotation_Symbol"call chain: \n";
        this->id += ops->get_call_chain(this->linux_kernel_version);
    }
    return this->id;
}

std::string sd::device_driver::get_id_hash() {
    if (this->id_hash.empty()) {
        std::hash<std::string> str_hash;
        this->id_hash = std::to_string(str_hash(this->get_id()));
        yhao_log(1, "id: " + this->get_id());
        yhao_log(1, "id hash: ");
    }
    return this->id_hash;
}

std::string sd::device_driver::get_id_real_hash() {
    if (this->id_real_hash.empty()) {
        std::string temp;

        if (this->parent->parent != nullptr) {
            temp += get_id_hash();
        }

        temp += this->ops->ops_name;

        entry_function *temp_parent;
        if (this->syscalls.find(syscall_name[3]) == this->syscalls.end()) {
            temp_parent = nullptr;
        } else {
            temp_parent = this->syscalls[syscall_name[3]];
        }
        std::set<llvm::Function *> fps;
        while (temp_parent != nullptr && temp_parent->parent != nullptr) {
            for (auto temp_1: temp_parent->function_pointers) {
                fps.insert(temp_1->llvm_function);
            }
            temp_parent = temp_parent->parent->parent;
        }
        for (auto temp_func: fps) {
            temp += temp_func->getName().str();
        }

        std::hash<std::string> str_hash;
        this->id_real_hash = std::to_string(str_hash(temp));
    }
    return this->id_real_hash;
}

std::string sd::device_driver::get_resource() {
    if (this->resource.empty()) {
        if (ops->ct == checker_type::dri_ops) {
            this->resource = "fd_" + this->get_id_hash() + "_fd";
        } else if (ops->ct == checker_type::file_ops) {
            if (this->parent->parent == nullptr) {
                this->resource = "fd_" + this->get_id_hash() + "_fd";
            } else if (this->non_open_parent == nullptr) {
                this->resource = "not_find_non_open_parent";
                yhao_log(3, "this->non_open_parent == nullptr");
                yhao_log(2, this->get_id());
            } else {
                this->resource = this->non_open_parent->generate_resource_name();
            }
        } else {
            yhao_log(3, "ops->ct");
        }
    }
    return this->resource;
}

bool sd::device_driver::operator<(const sd::device_driver &rhs) const {
    int64_t index_1 = 0, index_2 = 0;
    auto temp = this;
    while (temp != nullptr && temp->parent != nullptr) {
        temp = temp->parent->parent;
        index_1++;
    }
    temp = &rhs;
    while (temp != nullptr && temp->parent != nullptr) {
        temp = temp->parent->parent;
        index_2++;
    }
    if (index_1 == index_2) {
        if (this->name.size() < rhs.name.size()) {
            return true;
        }
        return false;
    } else if (index_1 < index_2) {
        return true;
    } else if (index_1 > index_2) {
        return false;
    }
    return true;
}

bool sd::device_driver::operator>(const sd::device_driver &rhs) const {
    return rhs < *this;
}

bool sd::device_driver::operator<=(const sd::device_driver &rhs) const {
    return !(rhs < *this);
}

bool sd::device_driver::operator>=(const sd::device_driver &rhs) const {
    return !(*this < rhs);
}

std::string sd::cmd_info::generate_resource_name() {
    return "fd_" + dd->get_id_hash() + "_cmd_" + std::to_string(this->value) + "_fd";
}
