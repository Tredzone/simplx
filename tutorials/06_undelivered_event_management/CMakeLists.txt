# single tuto
cmake_minimum_required(VERSION 3.0)
set(TARGET_NAME undelivered_event_management)

set(SOURCE_FILES undelivered_event_management.cpp)

add_executable(${TARGET_NAME} ${SOURCE_FILES})
target_link_libraries(${TARGET_NAME} engine)
