#pragma once
#include "db/Database.hpp"
#include "model/Allocation.hpp"

#include <optional>
#include <vector>

namespace studyctl::repo {

/// Owns the `allocations` table: a full audit trail of controller output.
/// Never begins a transaction: SQLite has no nested transactions, so
/// multi-statement atomicity is the caller's job via db::Transaction.
/// Throws db::SqliteError on database failure.
///
/// `allocations` has UNIQUE(course_id, year, week_no, computed_at), and
/// computed_at's DEFAULT has only one-second resolution, so two allocations
/// written for the same course/week within the same second collide. insert()
/// therefore *requires* a caller-supplied computedAt. The correct usage: one
/// controller run produces one timestamp (see nowTimestamp), shared by every
/// course row that run writes - the UNIQUE constraint is then satisfied
/// naturally because course_id differs across rows in the same run.
class AllocationRepository {
public:
    explicit AllocationRepository(db::Database& db) : db_(db) {}

    // Throws std::invalid_argument if a.computedAt is empty.
    std::int64_t insert(const model::Allocation& a);

    std::optional<model::Allocation> findLatest(std::int64_t courseId, int year, int weekNo);
    std::vector<model::Allocation>   listLatestForWeek(int year, int weekNo);   // one row per course
    std::vector<model::Allocation>   listHistory(std::int64_t courseId, int limit);   // newest first
    std::optional<double>            lastFinalHours(std::int64_t courseId, int year, int weekNo);

    // Millisecond-resolution "now", suitable for computed_at. Reduces but
    // does not eliminate collisions - callers should still share one
    // timestamp across all rows written by a single controller run.
    static std::string nowTimestamp(db::Database& db);

private:
    db::Database& db_;
};

}  // namespace studyctl::repo
