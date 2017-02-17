cmake_minimum_required (VERSION 2.6)
project (benchmark)

message(STATUS "Configuring..." ${CMAKE_PROJECT_NAME})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O9 -Werror -mpopcnt -pthread -std=c++11")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../fst/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third-party/stx-btree/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third-party/stx-btree/include/stx")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third-party/art/include")

set(COMMON_DEPENDENCIES tcmalloc_minimal pthread)

add_executable(workload workload.cpp)
target_link_libraries(workload ${COMMON_DEPENDENCIES} ART CART FST)

add_executable(workload_email workload_email.cpp)
target_link_libraries(workload_email ${COMMON_DEPENDENCIES} ART CART FST)