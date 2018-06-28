# combined unit test
cmake_minimum_required(VERSION 3.0)
set (TARGET_NAME utest)

trz_add_test(testasync testasync.cpp engine gtest)
trz_add_test(testasyncactor testasyncactor.cpp engine gtest)
trz_add_test(testasyncengine testasyncengine.cpp engine gtest)
trz_add_test(testasynceventloop testasynceventloop.cpp engine gtest)
trz_add_test(testasyncinitializer testasyncinitializer.cpp engine gtest)
trz_add_test(testmdoublechain testmdoublechain.cpp engine gtest)
trz_add_test(testmforwardchain testmforwardchain.cpp engine gtest)
trz_add_test(testparallel testparallel.cpp engine gtest)
trz_add_test(testproperty testproperty.cpp engine gtest)
trz_add_test(testtime testtime.cpp engine gtest)
trz_add_test(testtimer testtimeractor.cpp timer gtest)

