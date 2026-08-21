#pragma once
#include "Statement.hpp"
#include <string>

namespace studyctl::db {
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&)               = delete;
    Database& operator=(const Database&)    = delete;
    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;

    void            exec(const std::string& sql);       // multi-statement OK
    Statement       prepare(const std::string& sql);
    std::int64_t    lastInsertRowId() const;
    // Rows matched and written by the most recently executed statement on
    // this connection. Note this counts *matched* rows, not *changed* ones:
    // an UPDATE ... WHERE id=? that writes identical values still returns 1,
    // which is exactly the "did this row exist?" signal repos want.
    int             changes() const noexcept;
    sqlite3*        raw() const noexcept { return conn_; }
private:
    sqlite3* conn_ = nullptr;
};
}