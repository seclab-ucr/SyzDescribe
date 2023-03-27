//
// Created by yhao on 11/17/21.
//

#ifndef INC_2021_TEMPLATE_DEVICE_DRIVER_H
#define INC_2021_TEMPLATE_DEVICE_DRIVER_H

#include "../ToolLib/basic.h"
#include "../ToolLib/json.hpp"
#include "checker.h"

namespace sd {
    class entry_function;
    class device_driver;

    class cmd_info {
    public:
        llvm::Instruction *i{};
        uint64_t value{};
        llvm::BasicBlock *b{};
        std::map<llvm::Value *, llvm::Type *> in_types;
        std::map<llvm::Value *, llvm::Type *> out_types;
        bool non_open = false;
        device_driver *dd{};

        std::string generate_resource_name();

        // statistic
        bool in_entry_function = false;
        bool in_indirect_call = false;
    };

    class device_driver {
    public:
        device_driver() {
            no_cmd = new cmd_info();
        };

        void set_kind();

        [[nodiscard]] bool is_only_driver() const;

        void update_structure_type();

        std::string print();

        // the id for each driver
        std::string get_id();

        std::string get_id_hash();

        // whether the two device driver are the similar
        std::string get_id_real_hash();

        std::string get_resource();

        bool operator<(const device_driver &rhs) const;

        bool operator>(const device_driver &rhs) const;

        bool operator<=(const device_driver &rhs) const;

        bool operator>=(const device_driver &rhs) const;

    public:
        std::string linux_kernel_version;
        int64_t kind = 0;

        checker_result_ops *ops{};
        std::vector<checker_result_string *> name;
        std::vector<checker_result_number *> number;
        std::vector<checker_result *> file_install;
        std::vector<checker_result_number *> file_fd;

        // cmd and arg
        std::map<uint64_t, cmd_info *> cmd;
        cmd_info *no_cmd{};
        std::set<llvm::StructType *> structures;
        std::map<llvm::StructType *, std::string> structure_name;

        entry_function *parent = nullptr;
        cmd_info *non_open_parent = nullptr;

        // id based on parent and call chain
        // different device driver in source code
        std::string id;
        std::string id_hash;

        // id based on the ops and whether they have different function pointers
        // different device driver in template
        std::string id_real_hash;

        std::string resource;

        std::map<std::string, entry_function *> syscalls;
        std::set<llvm::Function *> entries;
    };
}


#endif //INC_2021_TEMPLATE_DEVICE_DRIVER_H
