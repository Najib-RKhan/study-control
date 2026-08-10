#include "db/connection.h"

#include <sqlite3.h>

#include <stdexcept>

namespace studyctl::db {

void Sqlite3Deleter::operator()(sqlite3* db) const noexcept {
    sqlite3_close(db);
}

namespace {

void exec_pragma(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string message = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error(std::string("failed to set pragma '") + sql + "': " + message);
    }
}

}  // namespace

ConnectionHandle open_connection(const std::string& path) {
    sqlite3* raw = nullptr;
    if (sqlite3_open(path.c_str(), &raw) != SQLITE_OK) {
        std::string message = raw ? sqlite3_errmsg(raw) : "unknown error";
        sqlite3_close(raw);
        throw std::runtime_error("failed to open database '" + path + "': " + message);
    }
    ConnectionHandle db(raw);

    // foreign_keys defaults OFF and busy_timeout resets to 0 on every new
    // connection, so both must be reapplied here even though journal_mode
    // and synchronous persist in the database file after the first set.
    exec_pragma(db.get(), "PRAGMA foreign_keys = ON;");
    exec_pragma(db.get(), "PRAGMA journal_mode = WAL;");
    exec_pragma(db.get(), "PRAGMA synchronous = NORMAL;");
    exec_pragma(db.get(), "PRAGMA busy_timeout = 3000;");

    return db;
}

}  // namespace studyctl::db
