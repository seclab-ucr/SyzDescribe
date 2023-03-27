# the k about how to find device drivers and
# all device drivers

set(SOURCES
        checker.cpp checker.h
        task.cpp task.h
        knowledge.cpp knowledge.h
        device_driver.cpp device_driver.h
        syzlang.cpp syzlang.h)

add_library(KnowledgeLib ${SOURCES})
set_target_properties(KnowledgeLib PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(KnowledgeLib PUBLIC
        ToolLib
        )