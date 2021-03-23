#include <coroutine>
#include <memory>
#include <cassert>
#include <vector>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>

#include <sqlite3.h>

#include <benchmark/benchmark.h>

using namespace std::chrono_literals;

class SQLite3;
class SQLite3Stmt;
class SQLite3Row
{
    friend class SQLite3Stmt;
public:
    enum class ColumnType {
        Null,
        Blob,
        Float,
        Int,
        Text
    };

    explicit SQLite3Row() = default;
    SQLite3Row(SQLite3Row &) = default;
    SQLite3Row(SQLite3Row &&) = default;
    SQLite3Row &operator=(SQLite3Row &) = default;
    SQLite3Row &operator=(SQLite3Row &&) = default;

    int columnCount() const {
        return sqlite3_column_count(stmt_);
    }

    ColumnType columnType(int colIdx) const {
        switch (sqlite3_column_type(stmt_, colIdx)) {
        case SQLITE_NULL:
            return ColumnType::Null;
        case SQLITE_BLOB:
            return ColumnType::Blob;
        case SQLITE_FLOAT:
            return ColumnType::Float;
        case SQLITE_INTEGER:
            return ColumnType::Int;
        case SQLITE_TEXT:
            return ColumnType::Text;
        }
        assert(false);
    }

    std::string valueText(int colIdx) const {
        return std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt_, colIdx)),
                           sqlite3_column_bytes(stmt_, colIdx));
    }

    std::string valueBlob(int colIdx) const {
        const auto *data = sqlite3_column_blob(stmt_, colIdx);
        const auto size = sqlite3_column_bytes(stmt_, colIdx);
        return std::string(reinterpret_cast<const char *>(data), size);
    }

    int valueInt(int colIdx) const {
        return sqlite3_column_int(stmt_, colIdx);
    }

    int64_t valueInt64(int colIdx) const {
        return sqlite3_column_int64(stmt_, colIdx);
    }

    double valueDouble(int colIdx) const {
        return sqlite3_column_double(stmt_, colIdx);
    }

private:
    explicit SQLite3Row(sqlite3_stmt *stmt)
        : stmt_(stmt)
    {}

    sqlite3_stmt *stmt_ = {};
};

class resumable {
public:
    struct promise_type {
        SQLite3Row row_;

        using coro_handle = std::coroutine_handle<promise_type>;
        auto get_return_object() {
            return coro_handle::from_promise(*this);
        }

        constexpr std::suspend_never initial_suspend() { return {}; };
        constexpr std::suspend_always final_suspend() { return {}; };
        constexpr void return_void() {};

        auto yield_value(SQLite3Row &&row) {
            row_ = std::move(row);
            return std::suspend_always();
        }

        void unhandled_exception() {
            std::terminate();
        }
    };

    using coro_handle = std::coroutine_handle<promise_type>;

    resumable(coro_handle handle)
        : handle_(handle) {};

    resumable(resumable &) = delete;
    resumable(resumable &&) = delete;
    ~resumable() {
        handle_.destroy();
    }

    bool done() {
        if (!handle_.done()) {
            handle_.resume();
        }
        return handle_.done();
    }

    SQLite3Row nextRow() const {
        return handle_.promise().row_;
    }
private:
    coro_handle handle_;
};

class SQLite3Stmt
{
    friend class SQLite3;
public:
    resumable exec() {
        auto status = SQLITE_ERROR;
        do {
            // simulate that obtaining the row actually takes some time
            status = sqlite3_step(stmt_);
            co_yield SQLite3Row(stmt_);
        } while (status == SQLITE_ROW);
        if (status != SQLITE_DONE) {
            std::cerr << "Error on SQL step: " << status << " " << sqlite3_errstr(status) << std::endl;
        }
        sqlite3_finalize(stmt_);
        stmt_ = {};

        co_return;
    }

    bool done() const {
        return done_;
    }

    SQLite3Row next() {
        auto status = sqlite3_step(stmt_);
        if (status != SQLITE_ROW) {
            if (status == SQLITE_DONE) {
                done_ = true;
            } else {
                std::cerr << "Error on SQL step: " << status << " " << sqlite3_errstr(status) << std::endl;
            }
        }
        return SQLite3Row(stmt_);
    }


    void reset() {
        sqlite3_reset(stmt_);
    }

    template<typename ... Args>
    void bind(Args&& ... args) {
        doBind<1>(std::forward<Args>(args) ...);
    }

private:
    template<std::size_t I, typename Arg0>
    void doBind(Arg0 &&arg0) {
        bindArgument<I>(std::forward<Arg0>(arg0));
    }

    template<std::size_t I, typename Arg0, typename ... Tail>
    void doBind(Arg0 &&arg0, Tail && ... tail) {
        bindArgument<I>(std::forward<Arg0>(arg0));
        doBind<I + 1>(std::forward<Tail>(tail) ...);
    }

    template<std::size_t I, typename T>
    void bindArgument(T &&value) {
        if constexpr (std::is_integral_v<std::remove_cvref_t<T>> && sizeof(T) < 8) {
            sqlite3_bind_int(stmt_, I, value);
        } if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::int64_t>) {
            sqlite3_bind_int64(stmt_, I, value);
        } if constexpr (std::is_same_v<std::remove_cvref_t<T>, double>) {
            sqlite3_bind_double(stmt_, I, value);
        } if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>) {
            sqlite3_bind_text(stmt_, I, value.data(), value.size(), SQLITE_TRANSIENT);
        } if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::vector<uint8_t>>) {
            sqlite3_bind_blob(stmt_, I, value.data(), value.size(), SQLITE_TRANSIENT);
        } else {
            static_assert(true, "Invalid type passed to bind()");
        }
    }

    explicit SQLite3Stmt() = default;

private:
    sqlite3_stmt *stmt_ = {};
    bool done_ = false;
};

class SQLite3
{
public:
    explicit SQLite3(std::string_view filename) {
        sqlite3_open(filename.data(), &_conn);
    }

    ~SQLite3() {
        sqlite3_close(_conn);
        _conn = {};
    }

    SQLite3Stmt prepare(std::string_view query) {
        SQLite3Stmt stmt;
        if (const int r = sqlite3_prepare_v2(_conn, query.data(), query.size(), &stmt.stmt_, {}); r != SQLITE_OK) {
            std::cerr << "Error preparing query: " << sqlite3_errmsg(_conn) << std::endl;
        }
        return stmt;
    }

    void exec(std::string_view query) {
        char *errmsg = {};
        if (const int r = sqlite3_exec(_conn, query.data(), nullptr, nullptr, &errmsg); r != SQLITE_OK) {
            std::cerr << "Error executing a query: " << errmsg << std::endl;
            sqlite3_free(errmsg);
        }
    }
private:
    sqlite3 *_conn = {};
};

class SQLite3Transaction {
public:
    explicit SQLite3Transaction(SQLite3 &conn)
        : conn_(conn) {
        conn_.exec("BEGIN TRANSACTION");
    }

    ~SQLite3Transaction() {
        if (!closed_) {
            rollback();
        }
    }

    void commit() {
        assert(!closed_);
        conn_.exec("COMMIT TRANSACTION");
        closed_ = true;
    }

    void rollback() {
        assert(!closed_);
        conn_.exec("ROLLBACK TRANSACTION");
        closed_ = true;
    }

private:
    SQLite3 &conn_;
    bool closed_ = false;
};

auto coroFetch() {
    SQLite3 conn("test.sqlite");
    SQLite3Stmt stmt = conn.prepare("SELECT * FROM test_table WHERE b % 2 = 0");
    auto result = stmt.exec();
    int i = 0;
    std::vector<int64_t> values;
    values.reserve(1'000'000);
    while (!result.done()) {
        ++i;
        auto row = result.nextRow();
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
