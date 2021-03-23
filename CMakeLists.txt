cmake_minimum_required(VERSION 3.12)
project(corosqlite LANGUAGES CXX VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
endif()

# ---------------------------------------------------- #
# Dependencies
# ---------------------------------------------------- #

find_package(SQLite3 REQUIRED)
find_package(benchmark REQUIRED)



add_subdirectory(corosqlite)




