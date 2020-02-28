project(reversion)

add_executable(reversion reversion.cpp)

link_directories(/home/brian/a_bug_project/feb17/1.8-PMDK-Arthas/src/nondebug)

target_link_libraries(reversion libpmemobj.a libpmem.a pthread dl)
