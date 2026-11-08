cmake_minimum_required(VERSION 3.16)
project(WikiRacePathfinder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -O2)
endif()

add_library(graph
    src/graph/Graph.cpp
    src/graph/TitleMap.cpp
    src/graph/Algorithms.cpp
)
target_include_directories(graph PUBLIC include)

add_executable(graph_stats src/tools/graph_stats.cpp)
target_link_libraries(graph_stats PRIVATE graph)
add_executable(wikirace src/tools/wikirace.cpp src/tools/ProcessMemory.cpp)
target_link_libraries(wikirace PRIVATE graph)
if(WIN32)
    target_link_libraries(wikirace PRIVATE psapi)
endif()

enable_testing()
add_executable(graph_tests tests/graph_tests.cpp)
target_link_libraries(graph_tests PRIVATE graph)
target_compile_definitions(graph_tests PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
add_test(NAME graph_tests COMMAND graph_tests)
