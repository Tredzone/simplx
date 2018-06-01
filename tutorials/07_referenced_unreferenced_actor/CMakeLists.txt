# single tuto
cmake_minimum_required(VERSION 3.0)
set(TARGET_NAME referenced_unreferenced_actor)

set(SOURCE_FILES ref_unref_actor.cpp)

add_executable(${TARGET_NAME} ${SOURCE_FILES})
target_link_libraries(${TARGET_NAME} engine)
