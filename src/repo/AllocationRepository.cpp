#include "AllocationRepository.hpp"

#include <stdexcept>

namespace studyctl::repo {
namespace {

constexpr const char* kAllocCols =
    "id, course_id, year, week_no, computed_at, smoothed_score, error, integral,"
    " kp_effective, ki_effective, delta_p, delta_i, hours_prev, hours_raw,"
    " hours_saturated, hours_final, was_clamped_low, was_clamped_high, normalizer_iters";

model::Allocation readAllocation(const db::Statement& st) {
    model::Allocation a;
    a.id               = st.columnInt64(0);
    a.courseId         = st.columnInt64(1);
    a.year             = st.columnInt(2);
    a.weekNo           = st.columnInt(3);
    a.computedAt       = st.columnText(4);
    a.smoothedScore    = st.columnDouble(5);
    a.error            = st.columnDouble(6);
    a.integral         = st.columnDouble(7);
    a.kpEffective      = st.columnDouble(8);
    a.kiEffective      = st.columnDouble(9);
    a.deltaP           = st.columnDouble(10);
    a.deltaI           = st.columnDouble(11);
    a.hoursPrev        = st.columnDouble(12);
    a.hoursRaw         = st.columnDouble(13);
    a.hoursSaturated   = st.columnDouble(14);
    a.hoursFinal       = st.columnDouble(15);
    a.wasClampedLow    = st.columnInt(16) != 0;
    a.wasClampedHigh   = st.columnInt(17) != 0;
    a.normalizerIters  = st.columnInt(18);
    return a;
}

}  // namespace

std::int64_t AllocationRepository::insert(const model::Allocation& a) {
    if (a.computedAt.empty())
        throw std::invalid_argument("Allocation::computedAt must be set");

    db::Statement st = db_.prepare(
        "INSERT INTO allocations(course_id, year, week_no, computed_at, smoothed_score,"
        " error, integral, kp_effective, ki_effective, delta_p, delta_i, hours_prev,"
        " hours_raw, hours_saturated, hours_final, was_clamped_low, was_clamped_high,"
        " normalizer_iters)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
    st.bind(1, a.courseId).bind(2, a.year).bind(3, a.weekNo).bind(4, a.computedAt)
      .bind(5, a.smoothedScore).bind(6, a.error).bind(7, a.integral)
      .bind(8, a.kpEffective).bind(9, a.kiEffective).bind(10, a.deltaP)
      .bind(11, a.deltaI).bind(12, a.hoursPrev).bind(13, a.hoursRaw)
      .bind(14, a.hoursSaturated).bind(15, a.hoursFinal)
      .bind(16, a.wasClampedLow ? 1 : 0).bind(17, a.wasClampedHigh ? 1 : 0)
      .bind(18, a.normalizerIters);
    st.execute();
    return db_.lastInsertRowId();
}

std::optional<model::Allocation> AllocationRepository::findLatest(std::int64_t courseId,
                                                                    int year, int weekNo) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kAllocCols
        + " FROM allocations WHERE course_id = ? AND year = ? AND week_no = ?"
          " ORDER BY computed_at DESC, id DESC LIMIT 1;");
    st.bind(1, courseId).bind(2, year).bind(3, weekNo);
    if (!st.step()) return std::nullopt;
    return readAllocation(st);
}

std::vector<model::Allocation> AllocationRepository::listLatestForWeek(int year, int weekNo) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kAllocCols + " FROM ("
        "    SELECT *,"
        "           ROW_NUMBER() OVER (PARTITION BY course_id"
        "                              ORDER BY computed_at DESC, id DESC) AS rn"
        "    FROM allocations"
        "    WHERE year = ? AND week_no = ?"
        ") WHERE rn = 1 ORDER BY course_id;");
    st.bind(1, year).bind(2, weekNo);
    std::vector<model::Allocation> out;
    while (st.step()) out.push_back(readAllocation(st));
    return out;
}

std::vector<model::Allocation> AllocationRepository::listHistory(std::int64_t courseId,
                                                                   int limit) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kAllocCols
        + " FROM allocations WHERE course_id = ?"
          " ORDER BY computed_at DESC, id DESC LIMIT ?;");
    st.bind(1, courseId).bind(2, limit);
    std::vector<model::Allocation> out;
    while (st.step()) out.push_back(readAllocation(st));
    return out;
}

std::optional<double> AllocationRepository::lastFinalHours(std::int64_t courseId, int year,
                                                             int weekNo) {
    db::Statement st = db_.prepare(
        "SELECT hours_final FROM allocations"
        " WHERE course_id = ? AND year = ? AND week_no = ?"
        " ORDER BY computed_at DESC, id DESC LIMIT 1;");
    st.bind(1, courseId).bind(2, year).bind(3, weekNo);
    if (!st.step()) return std::nullopt;
    return st.columnDouble(0);
}

std::string AllocationRepository::nowTimestamp(db::Database& db) {
    db::Statement st = db.prepare("SELECT strftime('%Y-%m-%d %H:%M:%f', 'now');");
    st.step();
    return st.columnText(0);
}

}  // namespace studyctl::repo
