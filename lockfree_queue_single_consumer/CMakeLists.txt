cmake_minimum_required(VERSION 3.2)
project(lockfree_queue_single_consumer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

set(SOURCE_FILES main.cpp)
add_executable(lockfree_queue_single_consumer ${SOURCE_FILES} queue1c.h)

target_link_libraries(lockfree_queue_single_consumer ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} pthread)
