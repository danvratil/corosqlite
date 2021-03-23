cmake_minimum_required(VERSION 3.12)
project(corosqlite LANGUAGES CXX VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)

# ---------------------------------------------------- #
# Compiler flags
# ---------------------------------------------------- #

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wpedantic -Werror")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts -stdlib=libc++")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /await")
else()
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported (yet).")
endif()

# ---------------------------------------------------- #
# Dependencies
# ---------------------------------------------------- #

find_package(SQLite3 REQUIRED)
find_package(benchmark REQUIRED)

# ---------------------------------------------------- #
# Subdirectories 
# ---------------------------------------------------- #

add_subdirectory(corosqlite)

