#include "sqlite.h"

#include <memory>
#include <cassert>
#include <vector>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>

#include <benchmark/benchmark.h>

using namespace corosqlite;
using namespace std::chrono_literals;

auto coroFetch() {
    SQLite3 conn("test.sqlite");
    SQLite3Stmt stmt = conn.prepare("SELECT * FROM test_table WHERE b % 2 = 0");
    auto result = stmt.exec();
    int i = 0;
    std::vector<int64_t> values;
    values.reserve(1'000'000);
    while (!result.done()) {
        ++i;
        auto row = result.next();
        values.push_back(row.valueInt64(1));
    }
    return values;
}

auto normalFetch() {
    SQLite3 conn("test.sqlite");
    SQLite3Stmt stmt = conn.prepare("SELECT * FROM test_table WHERE b % 2 = 0");
    int i = 0;
    std::vector<int64_t> values;
    values.reserve(1'000'000);
    while (!stmt.done()) {
        ++i;
        auto row = stmt.next();
        values.push_back(row.valueInt64(1));
    }
    return values;
}

void initDb() {
    std::random_device rd;
    std::mt19937 gen{rd()};
    SQLite3 conn("test.sqlite");
    std::cout << "Initializing database..." << std::endl;
    conn.exec("DROP TABLE IF EXISTS test_table");
    conn.exec("CREATE TABLE test_table (a INTEGER, b INTEGER)");
    SQLite3Transaction trx(conn);
    auto stmt = conn.prepare("INSERT INTO test_table (a, b) VALUES (?, ?)");
    for (int i = 0; i < 1'000'000; ++i) {
        stmt.reset();
        stmt.bind(i, static_cast<int64_t>(gen()));
        stmt.exec();
    }
    trx.commit();
}


static void BM_coroutine_fetch(benchmark::State &state) {
    if (state.thread_index == 0) {
        initDb();
    }

    for (auto _ : state) {
        coroFetch();
    }
}

static void BM_normal_fetch(benchmark::State &state) {
    if (state.thread_index == 0) {
        initDb();
    }

    for (auto _ : state) {
        normalFetch();
    }
}

BENCHMARK(BM_coroutine_fetch);
BENCHMARK(BM_normal_fetch);

BENCHMARK_MAIN();
