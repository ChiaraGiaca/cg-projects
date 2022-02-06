add_executable(yparticletrace yparticletrace.cpp)

set_target_properties(yparticletrace PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(yparticletrace PRIVATE ${CMAKE_SOURCE_DIR}/libs)
target_link_libraries(yparticletrace yocto yocto_particle)
