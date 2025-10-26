cmake_minimum_required(VERSION 3.12)
project(CascadeCppUCI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(engine
    src/board.cpp
    src/eval.cpp
    src/search.cpp
    src/uci.cpp
    src/zobrist.cpp
    src/nnue.cpp
)

target_include_directories(engine PUBLIC include)

add_executable(nox_engine src/main.cpp)

target_link_libraries(nox_engine PRIVATE engine)

# Optimize defaults for release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(engine PRIVATE -O3 -Wall -Wextra -Wpedantic)
  target_compile_options(nox_engine PRIVATE -O3 -Wall -Wextra -Wpedantic)
endif()
