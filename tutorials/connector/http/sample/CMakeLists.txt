# TCP sample 
cmake_minimum_required(VERSION 3.7.2)
set(TARGET_NAME tcp_sample.bin)

include("../../../../common_simplx.cmake")

trz_set_cxx_flags()

include_directories(${SIMPLX_DIR}/include)

trz_add_topdir(src/engine)

add_executable(httpserver.bin httpserver.cpp)
target_link_libraries(httpserver.bin engine)