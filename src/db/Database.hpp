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
    sqlite3*        raw() const noexcept { return conn_; }
private:
    sqlite3* conn_ = nullptr;
};
}