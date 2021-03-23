// SPDX-FileCopyrightText: 2021 Daniel Vrátil <me@dvratil.cz>
//
// SPDX-License-Identifier: MIT

#include "sqlite.h"

#include <iostream>

using namespace corosqlite;

SQLite3Row::SQLite3Row(sqlite3_stmt *stmt)
    : stmt_(stmt) {}

auto SQLite3Row::columnCount() const -> int {
    return sqlite3_column_count(stmt_);
}

auto SQLite3Row::columnType(int colIdx) const -> ColumnType {
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

auto SQLite3Row::valueText(int colIdx) const -> std::string {
    return std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt_, colIdx)),
                       sqlite3_column_bytes(stmt_, colIdx));
}

auto SQLite3Row::valueBlob(int colIdx) const -> std::string {
    const auto *data = sqlite3_column_blob(stmt_, colIdx);
    const auto size = sqlite3_column_bytes(stmt_, colIdx);
    return std::string(reinterpret_cast<const char *>(data), size);
}

auto SQLite3Row::valueInt(int colIdx) const -> int {
    return sqlite3_column_int(stmt_, colIdx);
}

auto SQLite3Row::valueInt64(int colIdx) const -> int64_t {
    return sqlite3_column_int64(stmt_, colIdx);
}

auto SQLite3Row::valueDouble(int colIdx) const -> double {
    return sqlite3_column_double(stmt_, colIdx);
}


////////////////////////////////////////////////////////////////


auto SQLite3Stmt::exec() -> ResultGenerator<SQLite3Row> {
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

auto SQLite3Stmt::done() const -> bool {
    return done_;
}

auto SQLite3Stmt::next() -> SQLite3Row {
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


auto SQLite3Stmt::reset() -> void {
    sqlite3_reset(stmt_);
}

auto SQLite3Stmt::bindArgument(int i, int val) -> void {
    sqlite3_bind_int(stmt_, i, val);
}

auto SQLite3Stmt::bindArgument(int i, std::int64_t val) -> void {
    sqlite3_bind_int64(stmt_, i, val);
}

auto SQLite3Stmt::bindArgument(int i, double val) -> void {
    sqlite3_bind_double(stmt_, i, val);
}

auto SQLite3Stmt::bindArgument(int i, const std::string &val) -> void {
    sqlite3_bind_text(stmt_, i, val.data(), val.size(), SQLITE_TRANSIENT);
}

auto SQLite3Stmt::bindArgument(int i, const std::vector<std::uint8_t> &val) -> void {
    sqlite3_bind_blob(stmt_, i, val.data(), val.size(), SQLITE_TRANSIENT);
}


////////////////////////////////////////////////////////////////


SQLite3::SQLite3(std::string_view filename) {
    sqlite3_open(filename.data(), &_conn);
}

SQLite3::~SQLite3() {
    sqlite3_close(_conn);
    _conn = {};
}

auto SQLite3::prepare(std::string_view query) -> SQLite3Stmt {
    SQLite3Stmt stmt;
    if (const int r = sqlite3_prepare_v2(_conn, query.data(), query.size(), &stmt.stmt_, {}); r != SQLITE_OK) {
        std::cerr << "Error preparing query: " << sqlite3_errmsg(_conn) << std::endl;
    }
    return stmt;
}

auto SQLite3::exec(std::string_view query) -> void {
    char *errmsg = {};
    if (const int r = sqlite3_exec(_conn, query.data(), nullptr, nullptr, &errmsg); r != SQLITE_OK) {
        std::cerr << "Error executing a query: " << errmsg << std::endl;
        sqlite3_free(errmsg);
    }
}


////////////////////////////////////////////////////////////////


SQLite3Transaction::SQLite3Transaction(SQLite3 &conn)
    : conn_(conn) {
    conn_.exec("BEGIN TRANSACTION");
}

SQLite3Transaction::~SQLite3Transaction() {
    if (!closed_) {
        rollback();
    }
}

auto SQLite3Transaction::commit() -> void {
    assert(!closed_);
    conn_.exec("COMMIT TRANSACTION");
    closed_ = true;
}

auto SQLite3Transaction::rollback() -> void {
    assert(!closed_);
    conn_.exec("ROLLBACK TRANSACTION");
    closed_ = true;
}

