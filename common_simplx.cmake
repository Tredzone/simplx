# common cmake util functions for simplx
cmake_minimum_required(VERSION 3.7.2)
project(root)

# root SIMPLX dir
set(SIMPLX_DIR ${CMAKE_CURRENT_LIST_DIR})

option(TREDZONE_E2E "TREDZONE_E2E" OFF)

INCLUDE(Dart)

# topmost include
include_directories(${SIMPLX_DIR}/include)

#---- Set CXX Flags ------------------------------------------------------------

function(trz_set_cxx_flags)
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")
    
    # search for user-defined c++ version
    string(FIND ${CMAKE_CXX_FLAGS} "--std=c++" USER_CXX_VERSION_POS)
    if (USER_CXX_VERSION_POS GREATER -1)
        # user-defined C++ version
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        # default is c++11
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")
    endif()

    # add pthread
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")    

    # set C++ compiler options
    if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        # gcc
        if(${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER 4.9)
            # gcc > 4.9
            if(${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER 7)
                # gcc 7+
                set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror -Wno-deprecated -Woverloaded-virtual -Wsign-promo -Wno-noexcept-type -fdiagnostics-color=auto ")
            else()
                # gcc ] 4.9 ; 7 ]
                set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror -Wno-deprecated -Woverloaded-virtual -Wsign-promo -fdiagnostics-color=auto")
            endif()
        else()
            # gcc <= 4.9
            set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror -Wno-deprecated -Woverloaded-virtual -Wsign-promo -Wno-noexcept-type")
        endif()
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")       			# -g already defined
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")					# adding flags here doesn't work
    elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11")
    else()
        message (FATAL_ERROR "unsupported C++ compiler ${CMAKE_CXX_COMPILER_ID}")
    endif()
    
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
		# disable warning/errors after assert(v) makes v an unused variable
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable")
    endif()
    
    # re-export to parent -- should be CACHE variable?
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} PARENT_SCOPE)

endfunction()

#---- Add (relative-from-top) directory MACRO ----------------------------------

    # must be MACRO because function would add its own scope and then be lost

macro(trz_add_topdir)

    # message("@ trz_add_topdir: TREDZONE_E2E = ${TREDZONE_E2E}")
    
    if (${TREDZONE_E2E})
        add_definitions(-DTREDZONE_E2E=1)     # only valid in current directory
    else()
        add_definitions(-DTREDZONE_E2E=0)     # only valid in current directory
    endif()
    
    set(reldir "${ARGV0}")
    
    set(dir1 "${SIMPLX_DIR}/${reldir}")
    set(dir2 "${CMAKE_BINARY_DIR}/${reldir}")
    
    add_subdirectory(${dir1} ${dir2})
    
    # barfs cause # of arguments?
    # list(REMOVE_DUPLICATES ${SOURCE_FILES})

endmacro()

#---- Add Unit Test ------------------------------------------------------------

function(trz_add_test test_name source_file dependency)

	add_executable(${test_name} ${source_file})
	trz_target_link_libraries(${test_name} "${dependency}")
	add_test(${test_name} ${test_name})		# add_test(TEST_NAME CMAKE_REF)

endfunction()

#---- Set Link Dependencies ----------------------------------------------------

function(trz_target_link_libraries test_name dependency)

    target_link_libraries(${test_name} ${dependency} gtest_main ${CMAKE_THREAD_LIBS_INIT} )

endfunction()

#---- Remove e2e stub ----------------------------------------------------------

function(trz_remove_e2e_stub)

    MESSAGE("@ ${TARGET_NAME} bf remove")
    MESSAGE("  SOURCE_FILES = ${SOURCE_FILES}")

    list(FILTER SOURCE_FILES EXCLUDE REGEX "^/.+/e2e_stub\\.cpp$")

    MESSAGE("@ ${TARGET_NAME} af remove")
    MESSAGE("  SOURCE_FILES = ${SOURCE_FILES}")

    # re-export to parent
    set(SOURCE_FILES ${SOURCE_FILES} PARENT_SCOPE)
    
endfunction()

# can NOT re-export to parent, cause has no parent?
# set(SIMPLX_DIR ${SIMPLX_DIR} CACHE)


