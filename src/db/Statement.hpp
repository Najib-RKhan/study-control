#pragma once
#include <sqlite3.h>
#include <string>
#include <optional>
#include <cstdint>

namespace studyctl::db {

class Statement {
public:
    Statement(sqlite3* conn, const std::string& sql);
    ~Statement();
    Statement(const Statement&)             = delete;
    Statement& operator=(const Statement&)  = delete;
    Statement(Statement&& o) noexcept;
    Statement& operator=(Statement&& o) noexcept;

    // 1-based indices, matches sqlite3 API
    Statement& bind(int i, int v);
    Statement& bind(int i, std::int64_t v);
    Statement& bind(int i, double v);
    Statement& bind(int i, const std::string& v);
    Statement& bind(int i, std::nullptr_t);
    Statement& bind(int i, const std:: optional<double>& v);

    /// returns true while rows remain. throws on error
    bool step();
    void reset();

    // 0-based indices, matching C API
    int                     columnInt(int i) const;
    std::int64_t            columnInt64(int i) const;
    double                  columnDouble(int i) const;
    std::string             columnText(int i) const;
    bool                    isNull(int i) const;
    std::optional<double>   columnOptDouble(int i) const;

    /// convenience for INSERT/UPDATE/DELETE: steps once, expects SQLITE_DONE
    void execute();

private:
    sqlite3*        conn_ = nullptr;
    sqlite3_stmt*   stmt_ = nullptr;
};
}