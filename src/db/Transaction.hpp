#pragma once
#include "Database.hpp"

namespace studyctl::db {
class Transaction {
public:
    explicit Transaction(Database& db) : db_(db) { db_.exec("BEGIN IMMEDIATE;"); }
    ~Transaction() {
        if (!done_) {
            try { db_.exec("ROLLBACK;"); } catch (...) {} // never throw from a dtor
        }
    }
    void commit() { db_.exec("COMMIT;"); done_ = true; }
    Transaction(const Transaction&)            = delete;
    Transaction& operator=(const Transaction&) = delete;
private:
    Database& db_;
    bool      done_ = false;
};
}