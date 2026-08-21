#pragma once
#include "db/Database.hpp"
#include "model/Entry.hpp"

#include <optional>
#include <vector>

namespace studyctl::repo {

/// Owns the `entries` table: an append-only observation log. The only
/// column this repo ever UPDATEs is `superseded_by` (via supersede()).
/// There is no method that mutates entry content and none that DELETEs.
/// Never begins a transaction: SQLite has no nested transactions, so
/// multi-statement atomicity (e.g. insert-then-supersede) is the caller's
/// job via db::Transaction. Throws db::SqliteError on database failure.
class EntryRepository {
public:
    explicit EntryRepository(db::Database& db) : db_(db) {}

    std::int64_t insertTime(std::int64_t courseId, int year, int weekNo, double hours,
                             const std::optional<std::string>& note);
    std::int64_t insertScore(std::int64_t courseId, int year, int weekNo,
                              const std::string& label, double rawScore, double maxScore,
                              const std::optional<double>& weight,
                              const std::optional<std::string>& note);
    // courseId may be nullopt only for a global stress reading (course_id IS NULL).
    std::int64_t insertStress(const std::optional<std::int64_t>& courseId,
                               int year, int weekNo, std::int64_t stress,
                               const std::optional<std::string>& note);

    std::optional<model::Entry> findById(std::int64_t id);
    std::vector<model::Entry>   listLive(std::int64_t courseId, int year, int weekNo);
    std::vector<model::Entry>   listLiveByKind(std::int64_t courseId, int year, int weekNo,
                                                model::EntryKind kind);
    std::vector<model::Entry>   listAllIncludingSuperseded(std::int64_t courseId, int year,
                                                             int weekNo);

    // Aggregates pushed into SQL, matching CourseRepository::weekView's
    // subqueries so the two never disagree.
    double                totalLiveHours(std::int64_t courseId, int year, int weekNo);
    std::optional<double> weightedScore(std::int64_t courseId, int year, int weekNo);
    std::optional<double> meanStress(int year, int weekNo);   // global: all courses + NULL course_id

    // Marks `oldId` as superseded by `newId`. Throws std::invalid_argument if
    // oldId == newId. Throws std::runtime_error if `oldId` does not exist or
    // is already superseded. Throws db::SqliteError if `newId` does not
    // exist (foreign key violation).
    //
    // Correcting a logged entry (caller owns the transaction):
    //   db::Transaction tx(db);
    //   auto newId = repo.insertTime(courseId, y, w, correctedHours, note);
    //   repo.supersede(oldId, newId);
    //   tx.commit();
    void supersede(std::int64_t oldId, std::int64_t newId);

private:
    db::Database& db_;
};

}  // namespace studyctl::repo
