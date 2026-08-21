#pragma once
#include "db/Database.hpp"
#include "model/ControllerParams.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace studyctl::repo {

/// Owns the `config` key/value table. Never begins a transaction: SQLite has
/// no nested transactions, so multi-statement atomicity (e.g.
/// saveControllerParams' 8 writes) is the caller's job via db::Transaction.
/// Throws db::SqliteError on database failure.
class ConfigRepository {
public:
    explicit ConfigRepository(db::Database& db) : db_(db) {}

    std::optional<std::string> get(const std::string& key);
    std::string getOr(const std::string& key, const std::string& fallback);
    void set(const std::string& key, const std::string& value);   // upsert

    std::optional<double> getDouble(const std::string& key);
    double getDoubleOr(const std::string& key, double fallback);
    void setDouble(const std::string& key, double value);

    std::optional<std::int64_t> getInt(const std::string& key);
    void setInt(const std::string& key, std::int64_t value);

    bool remove(const std::string& key);
    std::vector<std::pair<std::string, std::string>> listAll();   // ORDER BY key

    model::ControllerParams loadControllerParams();
    // Writes all 8 fields as separate setDouble() calls; wrap in
    // db::Transaction if atomicity across the whole set matters.
    void saveControllerParams(const model::ControllerParams& p);

private:
    db::Database& db_;
};

}  // namespace studyctl::repo
