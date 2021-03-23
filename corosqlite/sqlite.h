// SPDX-FileCopyrightText: 2021 Daniel Vrátil <me@dvratil.cz>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "coro.h"

#include <functional>
#include <vector>
#include <string_view>

#include <sqlite3.h>

namespace corosqlite
{

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

    auto columnCount() const -> int;
    auto columnType(int colIdx) const -> ColumnType;

    auto valueText(int colIdx) const -> std::string;
    auto valueBlob(int colIdx) const -> std::string;
    auto valueInt(int colIdx) const -> int;
    auto valueInt64(int colIdx) const -> int64_t;
    auto valueDouble(int colIdx) const -> double;

private:
    explicit SQLite3Row(sqlite3_stmt *stmt);

    sqlite3_stmt *stmt_ = {};
};

class SQLite3Stmt
{
    friend class SQLite3;
public:
    auto exec() -> ResultGenerator<SQLite3Row>;

    auto done() const -> bool;

    auto next() -> SQLite3Row;
    auto reset() -> void;

    template<typename ... Args>
    auto bind(Args&& ... args) -> void {
        doBind<1>(std::forward<Args>(args) ...);
    }

private:
    template<int I, typename Arg0>
    auto doBind(Arg0 &&arg0) -> void {
        bindArgument(I, std::forward<Arg0>(arg0));
    }

    template<int I, typename Arg0, typename ... Tail>
    auto doBind(Arg0 &&arg0, Tail && ... tail) -> void {
        bindArgument(I, std::forward<Arg0>(arg0));
        doBind<I + 1>(std::forward<Tail>(tail) ...);
    }

    auto bindArgument(int i, int val) -> void;
    auto bindArgument(int i, std::int64_t val) -> void;
    auto bindArgument(int i, double val) -> void;
    auto bindArgument(int i, const std::string &val) -> void;
    auto bindArgument(int i, const std::vector<std::uint8_t> &val) -> void;

    explicit SQLite3Stmt() = default;

private:
    sqlite3_stmt *stmt_ = {};
    bool done_ = false;
};

class SQLite3
{
public:
    explicit SQLite3(std::string_view filename);
    ~SQLite3();

    auto prepare(std::string_view query) -> SQLite3Stmt;
    auto exec(std::string_view query) -> void;

private:
    sqlite3 *_conn = {};
};

class SQLite3Transaction {
public:
    explicit SQLite3Transaction(SQLite3 &conn);
    SQLite3Transaction(SQLite3Transaction &) = delete;
    SQLite3Transaction(SQLite3Transaction &&) = delete;
    ~SQLite3Transaction();

    auto commit() -> void;
    auto rollback() -> void;

private:
    SQLite3 &conn_;
    bool closed_ = false;
};

} // namespace corosqlite
