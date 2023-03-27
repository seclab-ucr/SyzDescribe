//
// Created by yu on 6/24/21.
//

#include "checker.h"
#include "../ToolLib/llvm_related.h"
#include "../ToolLib/log.h"
#include <string>

sd::checker *sd::checker::copy() {
    auto c = new checker();
    c->copy(this);
    return c;
}

void sd::checker::copy(sd::checker *c) {
    this->ct = c->ct;
    this->id = c->id;
    this->name = c->name;

    this->structure = c->structure;
    this->offset = c->offset;

    this->function = c->function;
    this->object = c->object;
    this->value = c->value;
    this->value_offset = c->value_offset;
    this->field = c->field;
}

nlohmann::json *sd::checker::to_json() {
    auto j = new nlohmann::json();
    (*j)["1_checker_type"] = this->ct;
    (*j)["1_id"] = this->id;
    (*j)["2_structure"] = this->structure;
    (*j)["3_offset"] = this->offset;
    (*j)["4_function"] = this->function;
    (*j)["4_1_object"] = this->object;
    (*j)["4_2_value"] = this->value;
    (*j)["4_3_offset"] = this->value_offset;
    (*j)["4_4_field"] = this->field;
    return j;
}

void sd::checker::from_json(nlohmann::json j) {
    if (j.contains("1_checker_type") && j["1_checker_type"].is_number_integer()) {
        this->ct = j["1_checker_type"].get<checker_type>();
    }
    if (j.contains("1_id") && j["1_id"].is_number_integer()) {
        this->id = j["1_id"].get<uint64_t>();
    }
    if (j.contains("2_structure") && j["2_structure"].is_string()) {
        this->structure = j["2_structure"].get<std::string>();
    }
    if (j.contains("3_offset") && j["3_offset"].is_number_integer()) {
        this->offset = j["3_offset"].get<int64_t>();
    }
    if (j.contains("4_function") && j["4_function"].is_string()) {
        this->function = j["4_function"].get<std::string>();
    }
    if (j.contains("4_1_object") && j["4_1_object"].is_number_integer()) {
        this->object = j["4_1_object"].get<int64_t>();
    }
    if (j.contains("4_2_value") && j["4_2_value"].is_number_integer()) {
        this->value = j["4_2_value"].get<int64_t>();
    }
    if (j.contains("4_3_offset") && j["4_3_offset"].is_number_integer()) {
        this->value_offset = j["4_3_offset"].get<int64_t>();
    }
    if (j.contains("4_4_field") && j["4_4_field"].is_boolean()) {
        this->field = j["4_4_field"].get<bool>();
    }
}

std::string sd::checker::print() {
    auto j = this->to_json();
    std::string s = j->dump(2);
    delete j;
    return s;
}

sd::checker::~checker() = default;

nlohmann::json sd::offset_chain_offset::to_json() {
    auto *j = new nlohmann::json();
    (*j)["0_offset"] = this->offset;
    (*j)["1_type"] = this->type_str;
    if (this->next != nullptr) {
        (*j)["2_next"] = this->next->to_json();
    }
    return *j;
}

sd::offset_chain_offset::offset_chain_offset() {
    offset_chain_offset::offset = -2;
    offset_chain_offset::type_str = "";
    offset_chain_offset::type = nullptr;
    offset_chain_offset::next = nullptr;
}

sd::offset_chain_offset::~offset_chain_offset() {
    delete this->next;
}

void sd::offset_chain_offset::copy(sd::offset_chain_offset *oc) {
    this->offset = oc->offset;
    this->type_str = oc->type_str;
    this->type = oc->type;
    if (oc->next == nullptr) {
        this->next = nullptr;
    } else {
        this->next = new offset_chain_offset();
        this->next->copy(oc->next);
    }
}

void sd::offset_chain_offset::read_value_from_json(nlohmann::json j) {
    if (j.contains("0_offset") && j["0_offset"].is_number_integer()) {
        this->offset = j["0_offset"].get<int64_t>();
    }
    if (j.contains("1_type") && j["1_type"].is_string()) {
        this->type_str = j["1_type"].get<std::string>();
    }
    if (j.contains("2_next") && j["2_next"].is_object()) {
        if (this->next == nullptr) {
            this->next = new offset_chain_offset();
        }
        this->next->read_value_from_json(j["2_next"]);
    }
}

sd::offset_chain_base::offset_chain_base() {
    offset_chain_base::base = "";
    offset_chain_base::type_str = "";
    offset_chain_base::type = nullptr;
    offset_chain_base::next = nullptr;
}

sd::offset_chain_base::~offset_chain_base() {
    delete this->next;
}

nlohmann::json sd::offset_chain_base::to_json() {
    auto *j = new nlohmann::json();
    (*j)["0_base"] = this->base;
    (*j)["1_type"] = this->type_str;
    if (this->next != nullptr) {
        (*j)["2_next"] = this->next->to_json();
    }
    return *j;
}

void sd::offset_chain_base::copy(sd::offset_chain_base *oc) {
    this->base = oc->base;
    this->type_str = oc->type_str;
    this->type = oc->type;
    if (oc->next == nullptr) {
        this->next = nullptr;
    } else {
        this->next = new offset_chain_offset();
        this->next->copy(oc->next);
    }

}

void sd::offset_chain_base::read_value_from_json(nlohmann::json j) {
    if (j.contains("0_base") && j["0_base"].is_string()) {
        this->base = j["0_base"].get<std::string>();
    }
    if (j.contains("1_type") && j["1_type"].is_string()) {
        this->type_str = j["1_type"].get<std::string>();
    }
    if (j.contains("2_next") && j["2_next"].is_object()) {
        if (this->next == nullptr) {
            this->next = new offset_chain_offset();
        }
        this->next->read_value_from_json(j["2_next"]);
    }
}

sd::checker_result::~checker_result() {
    delete this->offset_chain;
}

void sd::checker_result::setInst(llvm::Instruction *i) {
    checker_result::inst = i;
    yhao_print(-1, checker_result::inst->print, checker_result::inst_str)
    checker_result::inst_strID = inst_to_strID(checker_result::inst);
}

void sd::checker_result::setInst(llvm::Module *m) {
    int64_t debug = -1;
    yhao_log(1, this->inst_strID);
    auto i = strID_to_inst(m, this->inst_strID);
    if (i != nullptr) {
        std::string i_str;
        yhao_print(debug, i->print, i_str)
        if (real_inst_str(i_str) == real_inst_str(checker_result::inst_str)) {
            checker_result::inst = i;
            return;
        }
        goto search_bb;
    }

    search_bb:
    auto b = strID_to_basicblock(m, checker_result::inst_strID);
    if (b != nullptr) {
        for (auto &ii: *b) {
            std::string i_str;
            yhao_print(debug, ii.print, i_str)
            if (real_inst_str(i_str) == real_inst_str(checker_result::inst_str)) {
                checker_result::inst = &ii;
                return;
            }
        }
        goto search_f;
    }

    search_f:
    auto f = strID_to_function(m, checker_result::inst_strID);
    if (f != nullptr) {
        for (auto &bb: *f) {
            for (auto &ii: bb) {
                std::string i_str;
                yhao_print(debug, ii.print, i_str)
                if (real_inst_str(i_str) == real_inst_str(checker_result::inst_str)) {
                    checker_result::inst = &ii;
                    return;
                }
            }
        }
        goto error;
    }

    error:
    yhao_log(3, "not find inst");
}

void sd::checker_result::push_call_chain(llvm::Instruction *i) {
    this->call_chain.push_back(i);
}

void sd::checker_result::pop_call_chain() {
    this->call_chain.pop_back();
}

void sd::checker_result::compute_hash() {
    std::string hash_str;
    hash_str += std::to_string(this->ct);
    hash_str += checker_result::structure;
    hash_str += std::to_string(this->offset);
    hash_str += checker_result::inst_strID;
    for (auto i: this->call_chain) {
        hash_str += inst_to_strID(i);
    }
    this->hash = std::hash<std::string>{}(hash_str);
}

sd::checker_result *sd::checker_result::copy() {
    auto c = new checker_result();
    c->checker_result::copy(this);
    return c;
}

void sd::checker_result::copy(sd::checker_result *c) {
    checker::copy(c);
    this->inst = c->inst;
    this->inst_str = c->inst_str;
    this->inst_strID = c->inst_strID;
    if (c->offset_chain == nullptr) {
        this->offset_chain = nullptr;
    } else {
        this->offset_chain = new offset_chain_base();
        this->offset_chain->copy(c->offset_chain);
    }
    for (auto i: c->call_chain) {
        this->call_chain.push_back(i);
    }
    this->hash = c->hash;
}

nlohmann::json *sd::checker_result::to_json() {
    auto j = checker::to_json();
    (*j)["0_hash"] = this->hash;
    (*j)["0_inst_strID"] = this->inst_strID;
    (*j)["0_inst_str"] = this->inst_str;
    if (this->offset_chain != nullptr) {
        (*j)["a_offset_chain"] = this->offset_chain->to_json();
    }
    int64_t No;
    No = 0;
    for (auto i: this->call_chain) {
        std::stringstream ss;
        ss << std::setw(2) << std::setfill('0') << No++;
        std::string s = ss.str();
        (*j)["b_call_chain"][s] = inst_to_strID(i);
    }
    return j;
}

void sd::checker_result::from_json(nlohmann::json j) {
    checker::from_json(j);
    if (j.contains("0_hash") && j["0_hash"].is_number_integer()) {
        this->hash = j["0_hash"].get<int64_t>();
    }
    if (j.contains("0_inst_strID") && j["0_inst_strID"].is_string()) {
        this->inst_strID = j["0_inst_strID"].get<std::string>();
    }
    if (j.contains("0_inst_str") && j["0_inst_str"].is_string()) {
        this->inst_str = j["0_inst_str"].get<std::string>();
    }
    if (j.contains("a_offset_chain") && j["a_offset_chain"].is_object()) {
        if (this->offset_chain == nullptr) {
            this->offset_chain = new offset_chain_base();
        }
        this->offset_chain->read_value_from_json(j["a_offset_chain"]);
    }
}

std::string sd::checker_result::get_call_chain(const std::string &linux_kernel_version) {
    std::string ret;
    int64_t No;
    No = 0;
    for (auto i: this->call_chain) {
        std::stringstream ss;
        ss << std::setw(2) << std::setfill('0') << No++;
        std::string s = ss.str();
        if (linux_kernel_version.empty()) {
            ret += Annotation_Symbol + s + ": " + dump_inst(i) + "\n";
        } else {
            ret += Annotation_Symbol + s + ": " + dump_inst_booltin(i, linux_kernel_version) + "\n";
        }
    }
    return ret;
}

sd::checker_result_number *sd::checker_result_number::copy() {
    auto c = new checker_result_number();
    c->checker_result_number::copy(this);
    return c;
}

void sd::checker_result_number::copy(sd::checker_result_number *c) {
    checker_result::copy(c);
    this->number = c->number;
}

nlohmann::json *sd::checker_result_number::to_json() {
    auto *j = checker_result::to_json();
    (*j)["b_type"] = this->number;
    return j;
}

void sd::checker_result_number::from_json(nlohmann::json j) {
    checker_result::from_json(j);
    if (j.contains("b_type") && j["b_type"].is_object()) {
        if (j["b_type"].get<int64_t>() < 0) {
            yhao_log(3, "b_type: bad  value");
        } else {
            this->number = j["b_type"].get<int64_t>();
        }
    } else {
        yhao_log(3, "b_type: no value");
    }
}

sd::checker_result_string *sd::checker_result_string::copy() {
    auto c = new checker_result_string();
    c->checker_result_string::copy(this);
    return c;
}

void sd::checker_result_string::copy(sd::checker_result_string *c) {
    checker_result::copy(c);
    this->fmt = c->fmt;
    for (auto v: va1) {
        this->va1.push_back(v);
    }
    for (const auto &v: va2) {
        this->va2.push_back(v);
    }
}

nlohmann::json *sd::checker_result_string::to_json() {
    auto j = checker_result::to_json();
    (*j)["c_fmt"] = this->fmt;
    int64_t No;
    No = 0;
    for (auto v: va1) {
        (*j)["c_val1"][std::to_string(No++)] = v;
    }
    No = 0;
    for (const auto &v: va2) {
        (*j)["c_val2"][std::to_string(No++)] = v;
    }
    (*j)["d_match"] = this->match;
    return j;
}

void sd::checker_result_string::from_json(nlohmann::json j) {
    checker_result::from_json(j);
    if (j.contains("c_fmt") && j["c_fmt"].is_string()) {
        this->fmt = j["c_fmt"].get<std::string>();
    } else {
        yhao_log(3, "c_fmt: no value");
    }
    int64_t No;
    if (j.contains("c_val1") && j["c_val1"].is_object()) {
        No = 0;
        auto key = std::to_string(No);
        auto j_val1 = j["c_val1"];
        while (j_val1.contains(key) && j_val1[key].is_number_integer()) {
            this->va1.push_back(j_val1[key].get<int64_t>());
            No++;
            key = std::to_string(No);
        }
    } else {
        yhao_log(3, "c_val1: no value");
    }
    if (j.contains("c_val2") && j["c_val2"].is_object()) {
        No = 0;
        auto key = std::to_string(No);
        auto j_val2 = j["c_val2"];
        while (j_val2.contains(key) && j_val2[key].is_string()) {
            this->va2.push_back(j_val2[key].get<std::string>());
            No++;
            key = std::to_string(No);
        }
    } else {
        yhao_log(3, "c_val2: no value");
    }
}

std::string sd::checker_result_string::to_string() {
    std::string name;
    bool fmt_model = false;
    int64_t va1_index = 0, va2_index = 0;
    for (auto temp: this->fmt) {
        if (temp == '%') {
            fmt_model = true;
        } else if (fmt_model) {
            fmt_model = false;
            if (temp == 's') {
                if (va2_index >= this->va2.size()) {
                    name += "%s";
                    continue;
                }
                auto temp_va = va2.at(va2_index);
                if (temp_va.empty()) {
                    name += "%s";
                    continue;
                }
                name += temp_va;
                va2_index++;
            } else if (temp == 'u' || temp == 'd' || temp == 'i') {
                if (va1_index >= this->va1.size()) {
                    //name += "%";
                    //name += temp;
                    name += "#";
                    continue;
                }
                auto temp_va = va1.at(va1_index);
                name += std::to_string(temp_va);
                va1_index++;
            } else if (temp == 'l') {
                fmt_model = true;
            }
        } else {
            name += temp;
        }
    }
    return name;
}

sd::checker_result_string::~checker_result_string() {
    this->va1.clear();
    this->va2.clear();
}

sd::checker_result_ops *sd::checker_result_ops::copy() {
    auto c = new checker_result_ops();
    c->checker_result_ops::copy(this);
    return c;
}

void sd::checker_result_ops::copy(sd::checker_result_ops *c) {
    checker_result::copy(c);
    this->ops_structure = c->ops_structure;
    this->ops_name = c->ops_name;
}

nlohmann::json *sd::checker_result_ops::to_json() {
    auto j = checker_result::to_json();
    (*j)["d_ops_structure"] = this->ops_structure;
    (*j)["d_ops_name"] = this->ops_name;
    return j;
}

void sd::checker_result_ops::from_json(nlohmann::json j) {
    checker_result::from_json(j);
    if (j.contains("d_ops_structure") && j["d_ops_structure"].is_string() &&
        j["d_ops_structure"].get<std::string>().empty()) {
        this->ops_structure = j["d_ops_structure"].get<std::string>();
    } else {
        yhao_log(3, "d_ops_structure: no value");
    }
    if (j.contains("d_ops_name") && j["d_ops_name"].is_string() && !j["d_ops_name"].get<std::string>().empty()) {
        this->ops_name = j["d_ops_name"].get<std::string>();
    } else {
        yhao_log(3, "d_ops_name: no value");
    }
}

sd::checker_result *sd::make_checker(sd::checker_type ct) {
    checker_result *cr;
    switch (ct) {
        case checker_type::dev_type:
        case checker_type::dri_type:
        case checker_type::dri_major:
        case checker_type::dri_minor:
        case checker_type::file_fd: {
            cr = new checker_result_number();
            break;
        }
        case checker_type::dev_name:
        case checker_type::dri_name: {
            cr = new checker_result_string();
            break;
        }
        case checker_type::dri_ops:
        case checker_type::file_ops: {
            cr = new checker_result_ops();
            break;
        }
        case checker_type::file_install: {
            cr = new checker_result();
            break;
        }
        default: {
            cr = new checker_result();
            yhao_log(3, "bad ct");
        }
    }
    return cr;
}

int64_t sd::match_call_chain(sd::checker_result *a, sd::checker_result *b) {
    int64_t ret = 0;
    if (a == nullptr || b == nullptr) {
        return ret;
    }
    int64_t size = a->call_chain.size() > b->call_chain.size() ? b->call_chain.size() : a->call_chain.size();
    while (ret < size && (a->call_chain.at(ret) == b->call_chain.at(ret))) {
        ret++;
    }
    return ret;
}
