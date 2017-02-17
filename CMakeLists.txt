cmake_minimum_required(VERSION 2.6)
project(fast-succinct-trie)

message(STATUS "Configuring..." ${CMAKE_PROJECT_NAME})

add_subdirectory(fst)
add_subdirectory(benchmark)
add_subdirectory(third-party/art)
#add_subdirectory(third-party/tx-trie)
#add_subdirectory(third-party/path_decomposed_tries)
