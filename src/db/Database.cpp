#include "Database.hpp"
#include "SqliteError.hpp"
#include <string>

namespace studyctl::db {

Database::Database(const std::string& path) {
    const int rc = sqlite3_open_v2(
        path.c_str(), &conn_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    if (rc != SQLITE_OK) {
        std::string msg = conn_ ? sqlite3_errmsg(conn_) : "open failed";
        sqlite3_close(conn_);
        conn_ = nullptr;
        throw SqliteError(rc, msg);
    }

    exec("PRAGMA foreign_keys = ON;");
    exec("PRAGMA journal_mode = WAL;");
    exec("PRAGMA synchronous = NORMAL;");
    exec("PRAGMA busy_timeout = 3000;");
}

Database::~Database() { if (conn_) sqlite3_close_v2(conn_); }

Database::Database(Database&& o) noexcept : conn_(o.conn_) {
    o.conn_ = nullptr;
}

Database& Database::operator=(Database&& o) noexcept {
    if (this != &o) {
        if (conn_) sqlite3_close_v2(conn_);
        conn_ = o.conn_;
        o.conn_ = nullptr;
    }
    return *this;
}

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    const int rc = sqlite3_exec(conn_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "exec failed";
        sqlite3_free(err);
        throw SqliteError(rc, msg);
    }
}

Statement Database::prepare(const std::string& sql) {
    // Statement borrows conn_ and owns only the sqlite3_stmt, so it must not
    // outlive this Database. Returned as a prvalue - no copy, no move.
    return Statement(conn_, sql);
}

std::int64_t Database::lastInsertRowId() const {
    return sqlite3_last_insert_rowid(conn_);
}

}

