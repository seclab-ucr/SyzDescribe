set(SOURCES
        json.hpp
        basic.h
        llvm_related.h
        llvm_related.cpp
        log.h)

add_library(ToolLib ${SOURCES})
set_target_properties(ToolLib PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(ToolLib PUBLIC
        ${_llvm_libs}
        ${_llvm_system_libs}
        )