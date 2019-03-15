# single tuto
cmake_minimum_required(VERSION 3.7.2)
set(TARGET_NAME undelivered_event_management.bin)

include("../../common_simplx.cmake")

trz_set_cxx_flags()

set(SOURCE_FILES
                    ${CMAKE_CURRENT_SOURCE_DIR}/undelivered_event_management.cpp
            )

trz_add_topdir(src/engine)

add_executable(${TARGET_NAME} ${SOURCE_FILES})
target_link_libraries(${TARGET_NAME} engine)
