set(SOURCES
        T_module.cpp T_module.h
        T_function.cpp T_function.h
        entry_function.cpp entry_function.h
        ioctl_cmd_type.cpp ioctl_cmd_type.h
        )

add_library(AnalysisLib ${SOURCES})
set_target_properties(AnalysisLib PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(AnalysisLib PUBLIC
        ToolLib
        KnowledgeLib
        MLTA
        )