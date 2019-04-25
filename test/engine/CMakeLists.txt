# base unit tests
cmake_minimum_required(VERSION 3.7.2)
set (TARGET_NAME utest)

include("../../common_simplx.cmake")

trz_set_cxx_flags()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable -Wno-unused-function -Wfatal-errors")

enable_testing()
trz_add_topdir(src/pattern/timer)                          # order matters ?? [PL]
trz_add_topdir(thirdparty/googletest/googletest)
trz_add_topdir(src/engine)

trz_add_test(testasync.bin testasync.cpp engine gtest)
trz_add_test(testasyncactor.bin testasyncactor.cpp engine gtest)
trz_add_test(testasyncengine.bin testasyncengine.cpp engine gtest)
trz_add_test(testasynceventloop.bin testasynceventloop.cpp engine gtest)
trz_add_test(testasyncinitializer.bin testasyncinitializer.cpp engine gtest)
trz_add_test(testmdoublechain.bin testmdoublechain.cpp engine gtest)
trz_add_test(testmforwardchain.bin testmforwardchain.cpp engine gtest)
trz_add_test(testparallel.bin testparallel.cpp engine gtest)
trz_add_test(testproperty.bin testproperty.cpp engine gtest)
trz_add_test(testtime.bin testtime.cpp engine gtest)
trz_add_test(testtimer.bin testtimeractor.cpp engine timer gtest)
trz_add_test(teststream.bin testdataiostream.cpp engine gtest)

