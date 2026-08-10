#pragma once

#include <memory>
#include <string>

struct sqlite3;

namespace studyctl::db {

struct Sqlite3Deleter {
    void operator()(sqlite3* db) const noexcept;
};

using ConnectionHandle = std::unique_ptr<sqlite3, Sqlite3Deleter>;

// Opens a SQLite connection at `path` and applies the pragmas required on
// every connection: foreign_keys, journal_mode, synchronous, busy_timeout.
// Throws std::runtime_error on failure.
ConnectionHandle open_connection(const std::string& path);

}  // namespace studyctl::db
