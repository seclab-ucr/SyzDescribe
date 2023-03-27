//
// Created by yuhao on 5/16/22.
//

#include "syzlang.h"
#include "../ToolLib/log.h"
#include "../ToolLib/llvm_related.h"
#include "../AnalysisLib/entry_function.h"

std::string sd::syzlang::generate_dd(device_driver *dd, std::set<std::string> &resources) {
    std::string ret;

    yhao_log(0, "generate_dd: " + dd->get_id_real_hash());

    // open and resource
    if (dd->non_open_parent == nullptr) {
        ret += Annotation_Symbol"driver found at module init function ";
    } else {
        ret += Annotation_Symbol"driver found at (not module init) function ";
    }
    ret += dd->parent->get_entry_function_name() + "() in ";
    ret += dump_function_booltin(dd->parent->get_entry_function(), this->linux_kernel_version);
    ret += "\n";

    ret += dd->get_id();
    ret += "\n";

    ret += Annotation_Symbol"syscalls handler structure in ";
    ret += dump_inst_booltin(dd->ops->inst, this->linux_kernel_version);
    ret += "\n";
    if (dd->syscalls.empty()) {
        ret += Annotation_Symbol"not find any syscall handler in the structure\n";
    } else {
        for (auto temp: dd->syscalls) {
            ret += Annotation_Symbol"find handler ";
            ret += temp.first.substr(2) + ": " + temp.second->get_entry_function_name() + "\n";
        }
    }
    ret += "\n";

    bool useful = false;
    if ((!dd->syscalls.empty()) && (!dd->cmd.empty())) {
        useful = true;
    }

    // for open fd, if not have useful syscall handlers, make resource annotation
    if (useful) {
    } else {
        ret += Annotation_Symbol" ";
    }
    std::string resource = "resource " + dd->get_resource() + "[fd]\n";
    if (resources.find(resource) == resources.end()) {
        resources.insert(resource);
        ret += resource;
        ret += "\n";
    }

    // for open fd, if not have useful syscall handlers, make open annotation
    if (dd->ops->ct == checker_type::dri_ops) {
        uint64_t index = 0;
        std::map<std::string, checker_result_string *> names;
        for (auto n: dd->name) {
            auto temp_name = n->to_string();
            if (names.find(temp_name) == names.end()) {
                names[temp_name] = n;
            }
        }
        uint64_t open_index = 0;
        for (auto n: names) {
            std::string str_open;
            str_open += Annotation_Symbol"device file name in ";
            str_open += dump_inst_booltin(n.second->inst, this->linux_kernel_version);
            str_open += "\n";

            if (n.first.empty() || n.first == "%s") {
                str_open += Annotation_Symbol"not get correct value for this device file name\n";
            } else {
                if (useful) {

                } else {
                    str_open += Annotation_Symbol;
                }
                open_index++;
                auto temp_name = n.first;
                uint64_t temp_index;
                if (temp_name.find('#') == std::string::npos) {
                    temp_index = 0;
                } else {
                    temp_index = 3;
                }
                str_open += syscall_open[0 + temp_index];
                str_open += PREFIX;
                str_open += dd->get_id_hash() + "_open_" + std::to_string(index++);
                str_open += syscall_open[1 + temp_index];
                str_open += temp_name;
                str_open += syscall_open[2 + temp_index];
                str_open += dd->get_resource() + "\n";
            }
            ret += str_open;
        }
        if (names.empty()) {
            ret += Annotation_Symbol"not get possible device file name\n";
        }
        if (open_index == 0 && useful) {
            std::string str_open;
            str_open += syscall_open[0];
            str_open += PREFIX;
            str_open += dd->get_id_hash() + "_open_" + std::to_string(index++);
            str_open += syscall_open[1];
            str_open += "incorrect_device_file_name";
            str_open += syscall_open[2];
            str_open += dd->get_resource() + "\n";
            ret += str_open;
        }
    } else {
        ret += Annotation_Symbol"non open fd\n";
    }
    ret += "\n";

    // ioctl and cmd
    std::string str_ioctl;
    std::string str_structure_type;
    for (auto cmd: dd->cmd) {
        if (cmd.second->in_types.empty() && cmd.second->out_types.empty()) {
            str_ioctl += generate_ioctl(dd, cmd.second, nullptr, 0, 0);
        } else {
            uint64_t index = 0;
            std::set<llvm::Type *> temp_in_types;
            std::set<llvm::Type *> temp_out_types;
            std::set<llvm::Type *> temp_in_out_types;
            for (auto type: cmd.second->in_types) {
                if (type.second == nullptr) {

                } else {
                    temp_in_types.insert(type.second);
                }
            }
            for (auto type: cmd.second->out_types) {
                if (type.second == nullptr) {

                } else {
                    temp_out_types.insert(type.second);
                }
            }
            for (auto type: temp_in_types) {
                if (temp_out_types.find(type) == temp_out_types.end()) {

                } else {
                    temp_in_out_types.insert(type);
                }
            }
            if (temp_in_types.empty() && temp_out_types.empty() && temp_in_out_types.empty()) {
                str_ioctl += generate_ioctl(dd, cmd.second, nullptr, 0, 0);
            } else {
                for (auto type: temp_in_types) {
                    if (temp_in_out_types.find(type) == temp_in_out_types.end()) {
                        str_ioctl += generate_ioctl(dd, cmd.second, type, 0, index++);
                    }
                }
                for (auto type: temp_out_types) {
                    if (temp_in_out_types.find(type) == temp_in_out_types.end()) {
                        str_ioctl += generate_ioctl(dd, cmd.second, type, 1, index++);
                    }
                }
                for (auto type: temp_in_out_types) {
                    str_ioctl += generate_ioctl(dd, cmd.second, type, 2, index++);
                }
            }
        }
    }
    ret += str_ioctl;
    if (dd->cmd.empty()) {

    } else {
        ret += "\n";
    }


    // type
    for (auto st: dd->structures) {
        ret += generate_struct(st, dd);
    }

    return ret;
}

std::string
sd::syzlang::generate_ioctl(sd::device_driver *dd, sd::cmd_info *cmd, llvm::Type *t, int64_t in_out, uint64_t index) {
    std::string temp_ioctl;
    temp_ioctl += syscall_ioctl[0];
    temp_ioctl += PREFIX;
    if (t == nullptr) {
        temp_ioctl += dd->get_id_hash() + "_" + std::to_string(cmd->value);
    } else {
        temp_ioctl += dd->get_id_hash() + "_" + std::to_string(cmd->value) + "_" + std::to_string(index);
    };
    temp_ioctl += syscall_ioctl[1];
    temp_ioctl += dd->resource;
    temp_ioctl += syscall_ioctl[2];
    temp_ioctl += std::to_string(cmd->value);
    temp_ioctl += syscall_ioctl[3];
    if (t == nullptr) {
        temp_ioctl += "ptr[in, array[int8]]";
    } else if (t->isArrayTy() || t->isStructTy()) {
        temp_ioctl += "ptr[";
        if (in_out == 0) {
            temp_ioctl += "in";
        } else if (in_out == 1) {
            temp_ioctl += "out";
        } else if (in_out == 2) {
            temp_ioctl += "inout";
        }
        temp_ioctl += ", ";
        temp_ioctl += generate_type(t, dd);
        temp_ioctl += "]";
    } else if (t->isIntegerTy()) {
        temp_ioctl += "int64";
    } else {
        temp_ioctl += generate_type(t, dd, in_out);
    }
    temp_ioctl += syscall_ioctl[4];
    if (cmd->non_open) {
        if ((!cmd->dd->syscalls.empty()) && (!cmd->dd->cmd.empty())) {
            temp_ioctl += " " + cmd->generate_resource_name();
        } else {
            temp_ioctl += "\n" Annotation_Symbol + temp_ioctl + " " + cmd->generate_resource_name();
        }
        //std::string resource = "resource " + cmd->generate_resource_name() + "[fd]\n";
        yhao_log(0, "cmd->non_open: " + cmd->generate_resource_name());
        yhao_log(0, cmd->dd->print());
    }
    temp_ioctl += "\n";
    return temp_ioctl;
}

std::string sd::syzlang::generate_type(llvm::Type *t, device_driver *dd, int64_t inout, bool opt) {
    std::string ret;
    std::string str;
    switch (t->getTypeID()) {
        case llvm::Type::HalfTyID:
        case llvm::Type::BFloatTyID:
        case llvm::Type::FloatTyID:
        case llvm::Type::DoubleTyID:
        case llvm::Type::X86_FP80TyID:
        case llvm::Type::FP128TyID:
        case llvm::Type::PPC_FP128TyID:
            yhao_log(3, "case llvm::Type::PPC_FP128TyID:");
            break;
        case llvm::Type::VoidTyID:
            yhao_log(3, "case llvm::Type::VoidTyID:");
            break;
        case llvm::Type::LabelTyID:
        case llvm::Type::MetadataTyID:
        case llvm::Type::X86_MMXTyID:
        case llvm::Type::X86_AMXTyID:
        case llvm::Type::TokenTyID:
            yhao_log(3, "case llvm::Type::TokenTyID:");
            break;
        case llvm::Type::IntegerTyID:
            ret += "int" + std::to_string(llvm::dyn_cast<llvm::IntegerType>(t)->getBitWidth());
            break;
        case llvm::Type::FunctionTyID:
            yhao_log(3, "case llvm::Type::FunctionTyID:");
            yhao_dump(2, t->print, str)
            ret += "intptr";
            break;
        case llvm::Type::PointerTyID:
            ret += "ptr[";
            if (inout == 0) {
                ret += "in, ";
            } else if (inout == 1) {
                ret += "out, ";
            } else if (inout == 2) {
                ret += "inout, ";
            }
            if (t->getNumContainedTypes()) {
                ret += generate_type(t->getNonOpaquePointerElementType(), dd, inout, false);
            } else {
                ret += "array[int8]";
            }
            if (opt) {
                ret += ", opt";
            }
            ret += "]";
            break;
        case llvm::Type::StructTyID:
            ret += generate_struct_name(llvm::dyn_cast<llvm::StructType>(t), dd);
            break;
        case llvm::Type::ArrayTyID:
            ret += "array[";
            ret += generate_type(t->getArrayElementType(), dd, inout, opt);
            if (t->getArrayNumElements() != 0) {
                ret += ", ";
                ret += std::to_string(t->getArrayNumElements());
            }
            ret += "]";
            break;
        case llvm::Type::FixedVectorTyID:
        case llvm::Type::ScalableVectorTyID:
            yhao_log(3, "case llvm::Type::ScalableVectorTyID:");
            break;
    }
    return ret;
}

std::string sd::syzlang::generate_struct(llvm::StructType *st, device_driver *dd) {
    std::string ret;
    auto name = generate_struct_name(st, dd);
    uint64_t field_index = 0;
    ret += name;
    ret += " {\n";
    for (auto i = 0; i < st->getNumElements(); i++) {
        ret += "    ";
        ret += "field_" + std::to_string(field_index++);
        ret += "    ";
        std::set<llvm::Type *> checked;
        ret += generate_type(st->getElementType(i), dd, 0, is_opt_pointer(st, checked, st->getElementType(i)));
        ret += "\n";
    }
    if (st->isPacked()) {
        ret += "} [packed]\n";
    } else {
        ret += "}\n";
    }
    return ret;
}

std::string sd::syzlang::generate_struct_name(llvm::StructType *st, device_driver *dd) {
    std::string ret;
    if (dd == nullptr) {
        if (structure_name.find(st) != structure_name.end()) {
            ret = structure_name[st];
        } else {
            auto temp_name = st->getStructName().str();
            for (auto temp: temp_name) {
                if (temp == '.') {
                    ret += '_';
                } else {
                    ret += temp;
                }
            }
            structure_name[st] = ret;
        }
    } else {
        if (dd->structure_name.find(st) != dd->structure_name.end()) {
            ret = dd->structure_name[st];
        } else {
            auto temp_name = st->getStructName().str();
            for (auto temp: temp_name) {
                if (temp == '.') {
                    ret += '_';
                } else {
                    ret += temp;
                }
            }
            ret += "_" + dd->get_id_hash();
            dd->structure_name[st] = ret;
        }
    }
    return ret;
}

std::string sd::syzlang::generate_general() const {
    std::string ret;
    ret += copyright;
    ret += "\n";
    ret += kernel_version;
    ret += this->linux_kernel_version;
    ret += "\n";
    ret += "\n";
    ret += "meta arches[\"";
    ret += this->target;
    ret += "\"]";
    ret += "\n";
    return ret;
}

void sd::syzlang::set_target(const std::string &t) {
    for (const auto &arche: arches) {
        if (t.find(arche[0] + "-") != std::string::npos) {
            this->target = arche[1];
            break;
        }
    }
}

std::string sd::syzlang::generate(sd::device_driver *dd) {
    std::string ret;
    std::vector<device_driver *> temp;
    temp.push_back(dd);
    ret += generate(dd->parent->get_entry_function(), temp);
    return ret;
}

std::string sd::syzlang::generate(llvm::Function *func, const std::vector<device_driver *> &dds) {
    std::string ret;
    std::string file_name = generate_file_name(func);
    yhao_log(0, "file name: " + file_name);

    std::hash<std::string> str_hash;
    this->file_prefix = std::to_string(str_hash(file_name));
    yhao_log(0, "file name hash: " + this->file_prefix);

    // add debug info
    ret += generate_general();
    ret += "\n";

    std::set<std::string> resources;

    // generate for each device driver
    for (auto dd: dds) {
        ret += generate_dd(dd, resources);
    }

    std::ofstream syz_of(this->work_dir + file_name);
    syz_of << ret;
    syz_of.close();

    this->file_prefix = "";

    return ret;
}

std::string sd::syzlang::generate_file_name(llvm::Function *func) const {
    std::string ret;
    ret += PREFIX;
    std::string path = get_file_name(func);
    for (auto c: path) {
        if (c == '/' || c == '.') {
            ret += '_';
        } else {
            ret += c;
        }
    }
    ret += "_";
    ret += target;
    ret += ".txt";
    return ret;
}

bool sd::syzlang::is_opt_pointer(llvm::StructType *st, std::set<llvm::Type *> &checked, llvm::Type *t) {
    if (checked.find(t) == checked.end()) {
        checked.insert(t);
    } else {
        return false;
    }
    switch (t->getTypeID()) {
        case llvm::Type::HalfTyID:
        case llvm::Type::BFloatTyID:
        case llvm::Type::FloatTyID:
        case llvm::Type::DoubleTyID:
        case llvm::Type::X86_FP80TyID:
        case llvm::Type::FP128TyID:
        case llvm::Type::PPC_FP128TyID:
        case llvm::Type::VoidTyID:
        case llvm::Type::LabelTyID:
        case llvm::Type::MetadataTyID:
        case llvm::Type::X86_MMXTyID:
        case llvm::Type::X86_AMXTyID:
        case llvm::Type::TokenTyID:
        case llvm::Type::IntegerTyID:
        case llvm::Type::FunctionTyID:
        // case llvm::Type::DXILPointerTyID:
            break;
        case llvm::Type::PointerTyID: {
            if (t->getNumContainedTypes()) {
                return is_opt_pointer(st, checked, t->getNonOpaquePointerElementType());
            }
        }
        case llvm::Type::StructTyID: {
            if (get_real_structure_name(t->getStructName().str()) == get_real_structure_name(st->getStructName().str())) {
                return true;
            }
            for (int64_t i = 0; i < t->getStructNumElements(); i++) {
                if (is_opt_pointer(st, checked, t->getStructElementType(i))) {
                    return true;
                }
            }
            break;
        }
        case llvm::Type::ArrayTyID:
            return is_opt_pointer(st, checked, t->getArrayElementType());
        case llvm::Type::FixedVectorTyID:
        case llvm::Type::ScalableVectorTyID:
            break;
    }
    return false;
}

std::string sd::syzlang::print_loc(llvm::Function *func, const std::vector<device_driver *> &dds) {
    std::string ret;
    std::set<llvm::Function *> functions;
    uint64_t line = 0;
    for (auto dd : dds) {
        
    }
    for (auto f : functions) {
        line += get_loc(f);
    }
    ret = Annotation_Symbol" loc of " + get_file_name(func) + " : " + std::to_string(line);
    return ret;
}
