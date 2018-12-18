cmake_minimum_required(VERSION 3.0)

project(simplx)

# modules
include(FindThreads)

#valgrind 
find_program(CTEST_MEMORYCHECK_COMMAND NAMES valgrind)
set( MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full")
INCLUDE(Dart)

option(BUILD_TEST "BUILD_TEST" OFF)
option(BUILD_CONNECTORS "BUILD_CONNECTORS" OFF)
option(BUILD_CONNECTORS_WITH_SAMPLES "BUILD_CONNECTORS_WITH_SAMPLES" OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
message("> CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")


# set C++ compiler options
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
	if(${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER 4.9)
		set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wpedantic -Werror -Wno-deprecated -Woverloaded-virtual -Wsign-promo -Wno-noexcept-type -fdiagnostics-color=auto")
	else ()
		set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wpedantic -Werror -Wno-deprecated -Woverloaded-virtual -Wsign-promo -Wno-noexcept-type")
	endif ()
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0") # -g already defined
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11")
else ()
	message (FATAL_ERROR "unsupported C++ compiler ${CMAKE_CXX_COMPILER_ID}")
endif ()


# set link dependencies
function(trz_target_link_libraries test_name depedency)
        target_link_libraries(${test_name} ${depedency} gtest_main  ${CMAKE_THREAD_LIBS_INIT} )
endfunction()

#add_executable(main src/main.cpp)
#target_link_libraries(main tredzone)

########################
##      UNIT TESTS
########################
# helper function
# function to add a unit test. Must be defined BEFORE add_subdirectory()
function(trz_add_test test_name source_file depedency)
	add_executable(${test_name} ${source_file})
	trz_target_link_libraries(${test_name} "${depedency}" )
	add_test(${test_name} ${test_name})		# add_test(TEST_NAME CMAKE_REF)
endfunction()

# THIRDPARTY
#include(thirdparty.cmake)

#Gtest
if(BUILD_TEST)
	enable_testing()
	add_subdirectory(thirdparty/googletest/googletest)
	#Unit Test
	add_subdirectory(test)
endif()

#Engine
add_subdirectory(src/engine)
#Timer
add_subdirectory(src/util)

#connectors
if(BUILD_CONNECTORS OR BUILD_CONNECTORS_WITH_SAMPLES)
	add_subdirectory(plugin/connectors)
endif()


#Tutorial
add_subdirectory(tutorials)

