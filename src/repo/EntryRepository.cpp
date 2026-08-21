#include "EntryRepository.hpp"

#include <stdexcept>

namespace studyctl::repo {
namespace {

constexpr const char* kEntryCols =
    "id, course_id, year, week_no, kind, hours, label, raw_score, max_score,"
    " weight, stress, note, created_at, superseded_by";

model::Entry readEntry(const db::Statement& st) {
    model::Entry e;
    e.id             = st.columnInt64(0);
    e.courseId       = st.columnOptInt64(1);
    e.year           = st.columnInt(2);
    e.weekNo         = st.columnInt(3);
    e.kind           = model::entryKindFromSql(st.columnText(4));
    e.hours          = st.columnOptDouble(5);
    e.label          = st.columnOptText(6);
    e.rawScore       = st.columnOptDouble(7);
    e.maxScore       = st.columnOptDouble(8);
    e.weight         = st.columnOptDouble(9);
    e.stress         = st.columnOptInt64(10);
    e.note           = st.columnOptText(11);
    e.createdAt      = st.columnText(12);
    e.supersededBy   = st.columnOptInt64(13);
    return e;
}

}  // namespace

std::int64_t EntryRepository::insertTime(std::int64_t courseId, int year, int weekNo,
                                          double hours,
                                          const std::optional<std::string>& note) {
    db::Statement st = db_.prepare(
        "INSERT INTO entries(course_id, year, week_no, kind, hours, note)"
        " VALUES (?, ?, ?, 'TIME', ?, ?);");
    st.bind(1, courseId).bind(2, year).bind(3, weekNo).bind(4, hours);
    st.bindOptText(5, note);
    st.execute();
    return db_.lastInsertRowId();
}

std::int64_t EntryRepository::insertScore(std::int64_t courseId, int year, int weekNo,
                                           const std::string& label, double rawScore,
                                           double maxScore,
                                           const std::optional<double>& weight,
                                           const std::optional<std::string>& note) {
    db::Statement st = db_.prepare(
        "INSERT INTO entries(course_id, year, week_no, kind, label, raw_score,"
        " max_score, weight, note)"
        " VALUES (?, ?, ?, 'SCORE', ?, ?, ?, ?, ?);");
    st.bind(1, courseId).bind(2, year).bind(3, weekNo).bind(4, label)
      .bind(5, rawScore).bind(6, maxScore);
    st.bindOptDouble(7, weight);
    st.bindOptText(8, note);
    st.execute();
    return db_.lastInsertRowId();
}

std::int64_t EntryRepository::insertStress(const std::optional<std::int64_t>& courseId,
                                            int year, int weekNo, std::int64_t stress,
                                            const std::optional<std::string>& note) {
    db::Statement st = db_.prepare(
        "INSERT INTO entries(course_id, year, week_no, kind, stress, note)"
        " VALUES (?, ?, ?, 'STRESS', ?, ?);");
    st.bindOptInt64(1, courseId);
    st.bind(2, year).bind(3, weekNo).bind(4, stress);
    st.bindOptText(5, note);
    st.execute();
    return db_.lastInsertRowId();
}

std::optional<model::Entry> EntryRepository::findById(std::int64_t id) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kEntryCols + " FROM entries WHERE id = ?;");
    st.bind(1, id);
    if (!st.step()) return std::nullopt;
    return readEntry(st);
}

std::vector<model::Entry> EntryRepository::listLive(std::int64_t courseId, int year,
                                                      int weekNo) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kEntryCols
        + " FROM entries WHERE course_id = ? AND year = ? AND week_no = ?"
          " AND superseded_by IS NULL ORDER BY id;");
    st.bind(1, courseId).bind(2, year).bind(3, weekNo);
    std::vector<model::Entry> out;
    while (st.step()) out.push_back(readEntry(st));
    return out;
}

std::vector<model::Entry> EntryRepository::listLiveByKind(std::int64_t courseId, int year,
                                                            int weekNo,
                                                            model::EntryKind kind) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kEntryCols
        + " FROM entries WHERE course_id = ? AND year = ? AND week_no = ?"
          " AND kind = ? AND superseded_by IS NULL ORDER BY id;");
    st.bind(1, courseId).bind(2, year).bind(3, weekNo);
    st.bind(4, std::string(model::toSql(kind)));
    std::vector<model::Entry> out;
    while (st.step()) out.push_back(readEntry(st));
    return out;
}

std::vector<model::Entry> EntryRepository::listAllIncludingSuperseded(
    std::int64_t courseId, int year, int weekNo) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kEntryCols
        + " FROM entries WHERE course_id = ? AND year = ? AND week_no = ? ORDER BY id;");
    st.bind(1, courseId).bind(2, year).bind(3, weekNo);
    std::vector<model::Entry> out;
    while (st.step()) out.push_back(readEntry(st));
    return out;
}

double EntryRepository::totalLiveHours(std::int64_t courseId, int year, int weekNo) {
    db::Statement st = db_.prepare(
        "SELECT COALESCE(SUM(hours), 0.0) FROM entries"
        " WHERE kind = 'TIME' AND superseded_by IS NULL"
        "   AND course_id = ? AND year = ? AND week_no = ?;");
    st.bind(1, courseId).bind(2, year).bind(3, weekNo);
    st.step();
    return st.columnDouble(0);
}

std::optional<double> EntryRepository::weightedScore(std::int64_t courseId, int year,
                                                       int weekNo) {
    db::Statement st = db_.prepare(
        "SELECT SUM((raw_score / max_score) * 100.0 * COALESCE(weight, 1.0))"
        "         / NULLIF(SUM(COALESCE(weight, 1.0)), 0)"
        " FROM entries"
        " WHERE kind = 'SCORE' AND superseded_by IS NULL"
        "   AND course_id = ? AND year = ? AND week_no = ?;");
    st.bind(1, courseId).bind(2, year).bind(3, weekNo);
    st.step();
    return st.columnOptDouble(0);
}

std::optional<double> EntryRepository::meanStress(int year, int weekNo) {
    db::Statement st = db_.prepare(
        "SELECT AVG(stress) FROM entries"
        " WHERE kind = 'STRESS' AND superseded_by IS NULL"
        "   AND year = ? AND week_no = ?;");
    st.bind(1, year).bind(2, weekNo);
    st.step();
    return st.columnOptDouble(0);
}

void EntryRepository::supersede(std::int64_t oldId, std::int64_t newId) {
    if (oldId == newId)
        throw std::invalid_argument("an entry cannot supersede itself");

    db::Statement st = db_.prepare(
        "UPDATE entries SET superseded_by = ?1 WHERE id = ?2 AND superseded_by IS NULL;");
    st.bind(1, newId).bind(2, oldId);
    st.execute();

    if (db_.changes() != 1)
        throw std::runtime_error("entry not found or already superseded");
}

}  // namespace studyctl::repo
