add_executable(ycolorigrades ycolorigrades.cpp)

set_target_properties(ycolorigrades PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(ycolorigrades PUBLIC ${CMAKE_SOURCE_DIR}/libs)
target_link_libraries(ycolorigrades yocto yocto_gui yocto_colorgrade)
