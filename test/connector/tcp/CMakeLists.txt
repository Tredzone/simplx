# TCP sample 
cmake_minimum_required(VERSION 3.7.2)
set(TARGET_NAME tcp_sample.bin)

include("../../../common_simplx.cmake")

trz_set_cxx_flags()

include_directories(${SIMPLX_DIR}/include)
include_directories(${SIMPLX_DIR}/test/connector/tcp)

enable_testing()
trz_add_topdir(thirdparty/googletest/googletest)
trz_add_topdir(src/engine)

trz_add_test(circularbuffertestu.bin circularbuffertestu.cpp engine gtest)
trz_add_test(clientservertestu.bin clientserverbehaviortest.cpp engine gtest)