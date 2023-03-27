//
// Created by yu on 4/20/21.
//

#ifndef INC_2021_TEMPLATE_T_FUNCTION_H
#define INC_2021_TEMPLATE_T_FUNCTION_H

#include "../ToolLib/basic.h"
#include "../KnowledgeLib/knowledge.h"

namespace sd {
    class T_module;

    // init and exit T_function number
    enum callee_type {
        callee_baisc = 0,
        callee_not_isDeclaration,
        callee_isDeclaration,
        callee_indirect,
        function_pointer,
    };

    class T_function;

    class callee {
    public:
        llvm::Instruction *i{};
        callee_type ct = callee_type::callee_baisc;
        // for callee_not_isDeclaration and callee_isDeclaration
        T_function *tf{};
        // for callee_indirect and function_pointer
        std::set<T_function *> *tf_set{};
        // whether this callee is checked by k function
        bool checked = false;
    };

    class T_function {
    public:
        explicit T_function(llvm::Function *f);

        virtual ~T_function();

        [[nodiscard]] std::string get_name() const;

        void check();

        void check(knowledge *k);

        bool check_call(checker *c, llvm::CallInst *cs);

        void check_function_pointer(llvm::Instruction *i);

        void check_function_pointer(llvm::Value *v, llvm::SmallPtrSet<const llvm::GlobalValue *, 3> &visited);

        callee *add_callee(llvm::Instruction *i, callee_type ct, T_function *tf = nullptr);

        callee *add_callee(llvm::Instruction *i, callee_type ct, std::set<T_function *> &tf);

        void add_checker_result(checker *c, llvm::Instruction *i);

        void add_callee_checker_results(llvm::Instruction *i, T_function *t_f);

        void reset();

        // calculate path
        llvm::DominatorTree *dominator_tree{};

        uint64_t calculate_path(llvm::BasicBlock *start_node, llvm::BasicBlock *end_node,
                                std::vector<llvm::BasicBlock *> *path);

        uint64_t calculate_path(llvm::BasicBlock *start_node, std::set<llvm::BasicBlock *> *end_node,
                                std::vector<llvm::BasicBlock *> *path);

        uint64_t calculate_path(std::set<llvm::Instruction *> *pass_node_i,
                                std::vector<llvm::BasicBlock *> *path);

        uint64_t calculate_path(std::set<llvm::Instruction *> *pass_node_i,
                                std::vector<uint64_t> *path);

    public:
        // for itself
        llvm::Function *llvm_function;
        T_module *t_module{};

        std::vector<callee *> all_callee;
        // not include function pointers
        std::map<llvm::Instruction *, callee *> all_callee_map;
        // used for finding function pointers
        std::set<T_function *> current_function_pointer;

        // for caller
        std::set<sd::T_function *> caller_not_isDeclaration;
        std::set<sd::T_function *> caller_indirect;
        std::set<sd::T_function *> caller_pointer;

        // checker results in current function
        std::vector<sd::checker_result *> checker_results;

        // number of checker, could be used for recursive check_callee analysis
        int64_t num_checkers_check_callee;
        int64_t num_checkers_check_caller;
        // order is from caller to callee
        std::map<llvm::Instruction *, std::set<sd::T_function *> *> all_callee_checker_results;
    };
}


#endif //INC_2021_TEMPLATE_T_FUNCTION_H
