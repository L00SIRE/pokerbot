cmake_minimum_required(VERSION 3.16)
project(pokerbot CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_compile_options(-Wall -Wextra)

add_library(poker
  src/card.cpp
  src/eval.cpp
  src/combos.cpp
  src/range.cpp
  src/equity.cpp
  src/showdown.cpp
  src/river_solver.cpp
)
target_include_directories(poker PUBLIC src)

add_executable(tests
  tests/main.cpp
  tests/test_cards.cpp
  tests/test_eval.cpp
  tests/test_combos.cpp
  tests/test_range.cpp
  tests/test_showdown.cpp
  tests/test_solver.cpp
)
target_link_libraries(tests poker)

find_package(Threads REQUIRED)

add_executable(verify_hands apps/verify_hands.cpp)
target_link_libraries(verify_hands poker Threads::Threads)

add_executable(equity apps/equity.cpp)
target_link_libraries(equity poker)

enable_testing()
add_test(NAME tests COMMAND tests)
