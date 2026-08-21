#include "TermRepository.hpp"

namespace studyctl::repo {
namespace {

constexpr const char* kTermCols =
    "id, label, start_year, start_week, end_year, end_week, is_active";

model::Term readTerm(const db::Statement& st) {
    model::Term t;
    t.id         = st.columnInt64(0);
    t.label      = st.columnText(1);
    t.startYear  = st.columnInt(2);
    t.startWeek  = st.columnInt(3);
    t.endYear    = st.columnInt(4);
    t.endWeek    = st.columnInt(5);
    t.isActive   = st.columnInt(6) != 0;
    return t;
}

}  // namespace

std::int64_t TermRepository::insert(const model::Term& t) {
    db::Statement st = db_.prepare(
        "INSERT INTO terms(label, start_year, start_week, end_year, end_week, is_active)"
        " VALUES (?, ?, ?, ?, ?, ?);");
    st.bind(1, t.label)
      .bind(2, t.startYear)
      .bind(3, t.startWeek)
      .bind(4, t.endYear)
      .bind(5, t.endWeek)
      .bind(6, t.isActive ? 1 : 0);
    st.execute();
    return db_.lastInsertRowId();
}

std::optional<model::Term> TermRepository::findById(std::int64_t id) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kTermCols + " FROM terms WHERE id = ?;");
    st.bind(1, id);
    if (!st.step()) return std::nullopt;
    return readTerm(st);
}

std::optional<model::Term> TermRepository::findByLabel(const std::string& label) {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kTermCols + " FROM terms WHERE label = ?;");
    st.bind(1, label);
    if (!st.step()) return std::nullopt;
    return readTerm(st);
}

std::optional<model::Term> TermRepository::findActive() {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kTermCols + " FROM terms WHERE is_active = 1;");
    if (!st.step()) return std::nullopt;
    return readTerm(st);
}

std::vector<model::Term> TermRepository::listAll() {
    db::Statement st = db_.prepare(
        std::string("SELECT ") + kTermCols
        + " FROM terms ORDER BY start_year, start_week;");
    std::vector<model::Term> out;
    while (st.step()) out.push_back(readTerm(st));
    return out;
}

bool TermRepository::setActive(std::int64_t id) {
    db::Statement exists = db_.prepare("SELECT 1 FROM terms WHERE id = ?;");
    exists.bind(1, id);
    if (!exists.step()) return false;

    db::Statement st = db_.prepare("UPDATE terms SET is_active = (id = ?1);");
    st.bind(1, id);
    st.execute();
    return true;
}

}  // namespace studyctl::repo
