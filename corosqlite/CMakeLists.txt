add_executable(coro main.cpp)
target_link_libraries(coro SQLite::SQLite3 benchmark::benchmark)
