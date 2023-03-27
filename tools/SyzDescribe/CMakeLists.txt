add_executable(SyzDescribe SyzDescribe.cpp)
install(TARGETS SyzDescribe DESTINATION bin)
target_link_libraries(SyzDescribe PUBLIC
        ManagerLib
        )