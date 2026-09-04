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
    src/bitboard.cpp
    src/bboard.cpp
)

target_include_directories(engine PUBLIC include)

add_executable(nox_engine src/main.cpp)

target_link_libraries(nox_engine PRIVATE engine)

# Standalone validation harness for the bitboard core (Phase 1 of the
# bitboard rewrite). Not linked into nox_engine, this exercises the
# bitboard.cpp attack tables/magic bitboards in isolation, independent of
# the rest of the engine, which still runs on the mailbox Board for now.
add_executable(bitboard_validate tools_bitboard_validate.cpp)
add_executable(bboard_validate tools_bboard_validate.cpp)
target_link_libraries(bboard_validate PRIVATE engine)
target_link_libraries(bitboard_validate PRIVATE engine)

# Optimize defaults for release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(engine PRIVATE -O3 -Wall -Wextra -Wpedantic)
  target_compile_options(nox_engine PRIVATE -O3 -Wall -Wextra -Wpedantic)
  target_compile_options(bitboard_validate PRIVATE -O3 -Wall -Wextra -Wpedantic)
  target_compile_options(bboard_validate PRIVATE -O3 -Wall -Wextra -Wpedantic)
endif()