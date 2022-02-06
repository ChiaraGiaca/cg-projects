add_executable(yparticleviews    yparticleviews.cpp)

set_target_properties(yparticleviews     PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(yparticleviews    PUBLIC ${CMAKE_SOURCE_DIR}/libs)
target_link_libraries(yparticleviews    yocto yocto_gui yocto_particle)
