add_executable(ycolorgrade ycolorgrade.cpp)

set_target_properties(ycolorgrade PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(ycolorgrade PUBLIC ${CMAKE_SOURCE_DIR}/libs)
target_link_libraries(ycolorgrade yocto yocto_colorgrade)
