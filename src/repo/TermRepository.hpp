#pragma once
#include "db/Database.hpp"
#include "model/Term.hpp"

#include <optional>
#include <vector>

namespace studyctl::repo {

/// Owns the `terms` table. Never begins a transaction: SQLite has no nested
/// transactions, so multi-statement atomicity is the caller's job via
/// db::Transaction. Throws db::SqliteError on database failure.
class TermRepository {
public:
    explicit TermRepository(db::Database& db) : db_(db) {}

    std::int64_t insert(const model::Term& t);   // ignores t.id, returns new id
    std::optional<model::Term> findById(std::int64_t id);
    std::optional<model::Term> findByLabel(const std::string& label);
    std::optional<model::Term> findActive();      // is_active = 1
    std::vector<model::Term>   listAll();         // ORDER BY start_year, start_week

    // Clears is_active on every other term and sets it on `id`, atomically
    // (single UPDATE statement). Returns false, without mutating anything,
    // if no term with `id` exists. Callers needing the existence check and
    // the update to be atomic together should wrap the call in
    // db::Transaction.
    bool setActive(std::int64_t id);

private:
    db::Database& db_;
};

}  // namespace studyctl::repo
