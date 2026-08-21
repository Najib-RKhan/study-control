#include "CourseRepository.hpp"

namespace studyctl::repo {
namespace {

constexpr const char* kCourseCols =
    "id, term_id, code, name, credit_hours, workload_factor, target_score,"
    " kp_override, ki_override, min_hours_per_week, max_share_of_budget,"
    " is_archived, created_at";

model::Course readCourse(const db::Statement& st) {
    model::Course c;
    c.id                = st.columnInt64(0);
    c.termId             = st.columnInt64(1);
    c.code               = st.columnText(2);
    c.name               = st.columnText(3);
    c.creditHours        = st.columnDouble(4);
    c.workloadFactor      = st.columnDouble(5);
    c.targetScore         = st.columnDouble(6);
    c.kpOverride          = st.columnOptDouble(7);
    c.kiOverride          = st.columnOptDouble(8);
    c.minHoursPerWeek     = st.columnDouble(9);
    c.maxShareOfBudget    = st.columnDouble(10);
    c.isArchived          = st.columnInt(11) != 0;
    c.createdAt           = st.columnText(12);
    return c;
}

}  // namespace

std::int64_t CourseRepository::insert(const model::Course& c) {
    db::Statement st = db_.prepare(
        "INSERT INTO courses(term_id, code, name, credit_hours, workload_factor,"
        " target_score, kp_override, ki_override, min_hours_per_week,"
        " max_share_of_budget, is_archived)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
    st.bind(1, c.termId)
      .bind(2, c.code)
      .bind(3, c.name)
      .bind(4, c.creditHours)
      .bind(5, c.workloadFactor)
      .bind(6, c.targetScore);
    st.bindOptDouble(7, c.kpOverride);
    st.bindOptDouble(8, c.kiOverride);
    st.bind(9, c.minHoursPerWeek)
      .bind(10, c.maxShareOfBudget)
      .bind(11, c.isArchived ? 1 : 0);
    st.execute();
    return db_.lastInsertRowId();
}

bool CourseRepository::update(const model::Course& c) {
    db::Statement st = db_.prepare(
        "UPDATE courses SET term_id = ?, code = ?, name = ?, credit_hours = ?,"
        " workload_factor = ?, target_score = ?, kp_override = ?, ki_override = ?,"
        " min_hours_per_week = ?, max_share_of_budget = ? WHERE id = ?;");
    st.bind(1, c.termId)
      .bind(2, c.code)
      .bind(3, c.name)
      .bind(4, c.creditHours)
      .bind(5, c.workloadFactor)
      .bind(6, c.targetScore);
    st.bindOptDouble(7, c.kpOverride);
    st.bindOptDouble(8, c.kiOverride);
    st.bind(9, c.minHoursPerWeek)
      .bind(10, c.maxShareOfBudget)
      .bind(11, c.id);
    st.execute();
    return db_.changes() == 1;
}

bool CourseRepository::archive(std::int64_t id) {
    db::Statement st = db_.prepare("UPDATE courses SET is_archived = 1 WHERE id = ?;");
    st.bind(1, id);
    st.execute();
    return db_.changes() == 1;
}

bool CourseRepository::unarchive(std::int64_t id) {
    db::Statement st = db_.prepare("UPDATE courses SET is_archived = 0 WHERE id = ?;");
    st.bind(1, id);
    st.execute();
    return db_.changes() == 1;
}

std::optional<model::Course> CourseRepository::findById(std::int64_t id) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kCourseCols + " FROM courses WHERE id = ?;");
    st.bind(1, id);
    if (!st.step()) return std::nullopt;
    return readCourse(st);
}

std::optional<model::Course> CourseRepository::findByCode(std::int64_t termId,
                                                           const std::string& code) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kCourseCols
        + " FROM courses WHERE term_id = ? AND code = ?;");
    st.bind(1, termId).bind(2, code);
    if (!st.step()) return std::nullopt;
    return readCourse(st);
}

std::vector<model::Course> CourseRepository::listActive(std::int64_t termId) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kCourseCols
        + " FROM courses WHERE term_id = ? AND is_archived = 0 ORDER BY code;");
    st.bind(1, termId);
    std::vector<model::Course> out;
    while (st.step()) out.push_back(readCourse(st));
    return out;
}

std::vector<model::Course> CourseRepository::listAll(std::int64_t termId) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kCourseCols
        + " FROM courses WHERE term_id = ? ORDER BY code;");
    st.bind(1, termId);
    std::vector<model::Course> out;
    while (st.step()) out.push_back(readCourse(st));
    return out;
}

std::vector<model::CourseWeekView> CourseRepository::weekView(std::int64_t termId,
                                                                int year, int weekNo) {
    db::Statement st = db_.prepare(
        "SELECT c.id, c.code, c.name, c.credit_hours, c.workload_factor, c.target_score,"
        "       COALESCE(t.hours, 0.0)                    AS logged_hours,"
        "       s.weighted_score                          AS weighted_score,"
        "       a.hours_final                             AS allocated_hours,"
        "       a.hours_final - COALESCE(t.hours, 0.0)    AS remaining_hours"
        " FROM courses c"
        " LEFT JOIN ("
        "     SELECT course_id, SUM(hours) AS hours"
        "     FROM entries"
        "     WHERE kind = 'TIME' AND superseded_by IS NULL"
        "       AND year = ?2 AND week_no = ?3"
        "     GROUP BY course_id"
        " ) t ON t.course_id = c.id"
        " LEFT JOIN ("
        "     SELECT course_id,"
        "            SUM((raw_score / max_score) * 100.0 * COALESCE(weight, 1.0))"
        "              / NULLIF(SUM(COALESCE(weight, 1.0)), 0) AS weighted_score"
        "     FROM entries"
        "     WHERE kind = 'SCORE' AND superseded_by IS NULL"
        "       AND year = ?2 AND week_no = ?3"
        "     GROUP BY course_id"
        " ) s ON s.course_id = c.id"
        " LEFT JOIN ("
        "     SELECT course_id, hours_final FROM ("
        "         SELECT course_id, hours_final,"
        "                ROW_NUMBER() OVER (PARTITION BY course_id"
        "                                   ORDER BY computed_at DESC, id DESC) AS rn"
        "         FROM allocations"
        "         WHERE year = ?2 AND week_no = ?3"
        "     ) WHERE rn = 1"
        " ) a ON a.course_id = c.id"
        " WHERE c.term_id = ?1 AND c.is_archived = 0"
        " ORDER BY c.code;");
    st.bind(1, termId).bind(2, year).bind(3, weekNo);

    std::vector<model::CourseWeekView> out;
    while (st.step()) {
        model::CourseWeekView v;
        v.courseId       = st.columnInt64(0);
        v.code            = st.columnText(1);
        v.name            = st.columnText(2);
        v.creditHours     = st.columnDouble(3);
        v.workloadFactor  = st.columnDouble(4);
        v.targetScore     = st.columnDouble(5);
        v.loggedHours     = st.columnDouble(6);
        v.weightedScore   = st.columnOptDouble(7);
        v.allocatedHours  = st.columnOptDouble(8);
        v.remainingHours  = st.columnOptDouble(9);
        out.push_back(v);
    }
    return out;
}

}  // namespace studyctl::repo
