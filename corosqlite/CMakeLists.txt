set(corosqlite_SRCS
    main.cpp
    sqlite.cpp
)

add_executable(corosqlite ${corosqlite_SRCS})
target_link_libraries(corosqlite
    SQLite::SQLite3
    benchmark::benchmark
)
