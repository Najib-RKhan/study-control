#include "Statement.hpp"
#include "SqliteError.hpp"

namespace studyctl::db {

Statement::Statement(sqlite3* conn, const std::string& sql) : conn_(conn) {
    const int rc = sqlite3_prepare_v2(conn_, sql.c_str(),
                                      static_cast<int>(sql.size()) + 1,
                                      &stmt_, nullptr);
    if (rc != SQLITE_OK)
        throw SqliteError(rc, std::string("prepare failed: ") + sqlite3_errmsg(conn_));
}

Statement::~Statement() { if (stmt_) sqlite3_finalize(stmt_); }

Statement::Statement(Statement&& o) noexcept : conn_(o.conn_), stmt_(o.stmt_) {
    o.stmt_ = nullptr; o.conn_ = nullptr;
}

Statement& Statement::operator=(Statement&& o) noexcept {
    if (this != &o) {
        if (stmt_) sqlite3_finalize(stmt_);
        conn_ = o.conn_; stmt_ = o.stmt_;
        o.conn_ = nullptr; o.stmt_ = nullptr;
    }
    return *this;
}

Statement& Statement::bind(int i, double v) {
    const int rc = sqlite3_bind_double(stmt_, i, v);
    if (rc != SQLITE_OK) throw SqliteError(rc, "bind double");
    return *this;
}

Statement& Statement::bind(int i, const std::string& v) {
    // SQLITE_TRANSIENT tells sqlite to copy the buffer. Using SQLITE_STATIC
    // here with a temporary std::string is a classic use-after-free.
    const int rc = sqlite3_bind_text(stmt_, i, v.c_str(),
                                     static_cast<int>(v.size()), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) throw SqliteError(rc, "bind text");
    return *this;
}

Statement& Statement::bind(int i, const std::optional<double>& v) {
    return v.has_value() ? bind(i, *v) : bind(i, nullptr);
}

Statement& Statement::bind(int i, std::nullptr_t) {
    const int rc = sqlite3_bind_null(stmt_, i);
    if (rc != SQLITE_OK) throw SqliteError(rc, "bind null");
    return *this;
}

Statement& Statement::bind(int i, int v) {
    const int rc = sqlite3_bind_int(stmt_, i, v);
    if (rc != SQLITE_OK) throw SqliteError(rc, "bind int");
    return *this;
}

Statement& Statement::bind(int i, std::int64_t v){
    const int rc = sqlite3_bind_int64(stmt_, i, v);
    if (rc != SQLITE_OK) throw SqliteError(rc, "bind int64");
    return *this;
}

bool Statement::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW)  return true;
    if (rc == SQLITE_DONE) return false;
    throw SqliteError(rc, std::string("step failed: ") + sqlite3_errmsg(conn_));
}

void Statement::execute() { if (step()) throw SqliteError(SQLITE_MISUSE, "unexpected row"); }

void Statement::reset() { sqlite3_reset(stmt_); sqlite3_clear_bindings(stmt_); }

std::string Statement::columnText(int i) const {
    const unsigned char* p = sqlite3_column_text(stmt_, i);
    return p ? reinterpret_cast<const char*>(p) : std::string{};
}

bool Statement::isNull(int i) const {
    return sqlite3_column_type(stmt_, i) == SQLITE_NULL;
}

std::optional<double> Statement::columnOptDouble(int i) const {
    if (isNull(i)) return std::nullopt;
    return sqlite3_column_double(stmt_, i);
}

double       Statement::columnDouble(int i) const { return sqlite3_column_double(stmt_, i); }
int          Statement::columnInt(int i)    const { return sqlite3_column_int(stmt_, i); }
std::int64_t Statement::columnInt64(int i)  const { return sqlite3_column_int64(stmt_, i); }
}