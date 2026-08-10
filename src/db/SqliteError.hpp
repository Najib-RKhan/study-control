#pragma once
#include <stdexcept>
#include <string>

namespace studyctl::db {
class SqliteError : public std::runtime_error {
public:
    SqliteError(int code, std::string context)
        : std::runtime_error(context + " [sqlite rc=" + std::to_string(code) + "]")
        , code_(code) {}
    int code() const noexcept {return code_; }
private:
    int code_;
};
}