#pragma once
#include "db/Database.hpp"
#include "model/Course.hpp"
#include "model/CourseWeekView.hpp"

#include <optional>
#include <vector>

namespace studyctl::repo {

/// Owns the `courses` table. Never begins a transaction: SQLite has no
/// nested transactions, so multi-statement atomicity is the caller's job via
/// db::Transaction. Throws db::SqliteError on database failure.
class CourseRepository {
public:
    explicit CourseRepository(db::Database& db) : db_(db) {}

    std::int64_t insert(const model::Course& c);   // ignores c.id and c.createdAt
    bool update(const model::Course& c);            // by c.id; does NOT touch is_archived or created_at
    bool archive(std::int64_t id);                  // is_archived = 1
    bool unarchive(std::int64_t id);

    std::optional<model::Course> findById(std::int64_t id);
    std::optional<model::Course> findByCode(std::int64_t termId, const std::string& code);
    std::vector<model::Course>   listActive(std::int64_t termId);   // is_archived = 0, ORDER BY code
    std::vector<model::Course>   listAll(std::int64_t termId);      // ORDER BY code

    // One row per active course in `termId` for (year, weekNo): logged
    // hours, weighted score, latest allocation, and the delta between them.
    // Assembled by a single SQL statement - see the .cpp for the query.
    std::vector<model::CourseWeekView> weekView(std::int64_t termId, int year, int weekNo);

private:
    db::Database& db_;
};

}  // namespace studyctl::repo
