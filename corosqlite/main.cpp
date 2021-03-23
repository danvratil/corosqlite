#include <coroutine>
#include <memory>
#include <cassert>
#include <vector>

#include <sqlite3.h>

class SQLite3;
class SQLite3Stmt;

class SQLite3Row
{
    friend class SQLite3Stmt;
private:
    enum class ColumnType {
        Null,
        Blob,
        Float,
        Int,
        Text
    };

    SQLite3Row(sqlite3_stmt *stmt)
        : _stmt(stmt)
    {}

    int columnCount() const
    {
        return sqlite3_column_count(_stmt);
    }

    ColumnType columnType(int colIdx) const
    {
        switch (sqlite3_column_type(_stmt, colIdx)) {
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

    template<typename T>
    T value(int colIdx) const;

    template<>
    std::string value<std::string>(int colIdx) const
    {
        return std::string(reinterpret_cast<const char *>(sqlite3_column_text(_stmt, colIdx)),
                           sqlite3_column_bytes(_stmt, colIdx));
    }

    template<>
    std::vector<std::byte> value<std::vector<std::byte>>(int colIdx) const
    {
        const auto *data = sqlite3_column_blob(_stmt, colIdx);
        const auto size = sqlite3_column_bytes(_stmt, colIdx);
        std::vector<std::byte> rv(size);
        std::copy(static_cast<const unsigned char *>(data),
                  static_cast<const unsigned char *>(data) + size, rv.begin());
        return rv;
    }

    template<>
    int value<int>(int colIdx) const
    {
        return sqlite3_column_int(_stmt, colIdx);
    }

    template<>
    int64_t value<int64_t>(int colIdx) const
    {
        return sqlite3_column_int64(_stmt, colIdx);
    }

    template<>
    double value<double>(int colIdx) const
    {
        return sqlite3_column_double(_stmt, colIdx);
    }

private:
    sqlite3_stmt * const _stmt = {};
};

class SQLite3Stmt
{
    friend class SQLite3;
private:
    explicit SQLite3Stmt();

    resumable exec()
    {
        auto status = SQLITE_ERROR;
        do {
            status = sqlite3_step(_stmt);
            co_yield SQLite3Row(_stmt);
        } while (status == SQLITE_OK);
        sqlite3_finalize(_stmt);
        _stmt = {};
        co_return;
    }

private:
    sqlite3_stmt *_stmt = {};
}


class SQLite3
{
public:
    explicit SQLite3(std::string_view filename)
    {
        sqlite3_open(filename.data(), &_conn);
    }

    ~SQLite3()
    {
        sqlite3_close(_conn);
        _conn = {};
    }

    SQLite3Stmt prepare(std::string_view query)
    {
        SQLite3Stmt stmt;
        sqlite3_prepare_v2(_conn, query.data(), query.size(), &stmt._stmt, {});
        return stmt;
    }

private:
    sqlite3 *_conn = {};
}

int main()
{

}
