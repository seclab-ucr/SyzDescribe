set(SOURCES
        Analyzer.hh
        Analyzer.cc
        CallGraph.hh
        CallGraph.cc
        Common.hh
        Common.cc
        Config.hh
        TypeInitializer.hh
        TypeInitializer.cc
        )

add_library(MLTA ${SOURCES})
set_target_properties(MLTA PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(MLTA PUBLIC
        ToolLib)