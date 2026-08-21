#include "doctest.h"

#include "db/Database.hpp"
#include "db/Migrator.hpp"
#include "db/SqliteError.hpp"
#include "db/Transaction.hpp"
#include "model/Allocation.hpp"
#include "model/Course.hpp"
#include "model/CourseWeekView.hpp"
#include "model/ControllerParams.hpp"
#include "model/Entry.hpp"
#include "model/Term.hpp"
#include "repo/AllocationRepository.hpp"
#include "repo/ConfigRepository.hpp"
#include "repo/CourseRepository.hpp"
#include "repo/EntryRepository.hpp"
#include "repo/TermRepository.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

using namespace studyctl;

namespace {

struct Fixture {
    db::Database db{":memory:"};

    Fixture() { db::Migrator::migrate(db, STUDYCTL_SQL_DIR); }

    std::int64_t seedTerm(const std::string& label = "Fall 2026") {
        repo::TermRepository terms(db);
        model::Term t;
        t.label = label;
        t.startYear = 2026; t.startWeek = 36;
        t.endYear = 2026;   t.endWeek = 51;
        const std::int64_t id = terms.insert(t);
        terms.setActive(id);
        return id;
    }

    std::int64_t seedCourse(std::int64_t termId, const std::string& code = "ENEE244") {
        repo::CourseRepository courses(db);
        model::Course c;
        c.termId = termId;
        c.code = code;
        c.name = "Digital Logic";
        c.creditHours = 3.0;
        return courses.insert(c);
    }
};

}  // namespace

// ---------------------------------------------------------------------
// TermRepository
// ---------------------------------------------------------------------

TEST_CASE("TermRepository round-trips all fields") {
    Fixture f;
    repo::TermRepository terms(f.db);

    model::Term t;
    t.label = "Spring 2027";
    t.startYear = 2027; t.startWeek = 4;
    t.endYear = 2027;   t.endWeek = 18;
    t.isActive = true;

    const std::int64_t id = terms.insert(t);
    CHECK(id > 0);

    auto found = terms.findById(id);
    REQUIRE(found.has_value());
    CHECK(found->id == id);
    CHECK(found->label == "Spring 2027");
    CHECK(found->startYear == 2027);
    CHECK(found->startWeek == 4);
    CHECK(found->endYear == 2027);
    CHECK(found->endWeek == 18);
    CHECK(found->isActive == true);
}

TEST_CASE("TermRepository rejects a duplicate label") {
    Fixture f;
    repo::TermRepository terms(f.db);
    model::Term t;
    t.label = "Fall 2026";
    t.startYear = 2026; t.startWeek = 36; t.endYear = 2026; t.endWeek = 51;
    terms.insert(t);
    CHECK_THROWS_AS(terms.insert(t), db::SqliteError);
}

TEST_CASE("TermRepository::findActive is nullopt until setActive is called") {
    Fixture f;
    repo::TermRepository terms(f.db);
    model::Term t;
    t.label = "Fall 2026";
    t.startYear = 2026; t.startWeek = 36; t.endYear = 2026; t.endWeek = 51;
    const std::int64_t id = terms.insert(t);

    CHECK_FALSE(terms.findActive().has_value());

    CHECK(terms.setActive(id));
    auto active = terms.findActive();
    REQUIRE(active.has_value());
    CHECK(active->id == id);
}

TEST_CASE("TermRepository::setActive clears the previously active term") {
    Fixture f;
    repo::TermRepository terms(f.db);
    model::Term a; a.label = "Fall 2026";   a.startYear = 2026; a.startWeek = 36; a.endYear = 2026; a.endWeek = 51;
    model::Term b; b.label = "Spring 2027"; b.startYear = 2027; b.startWeek = 4;  b.endYear = 2027; b.endWeek = 18;
    const std::int64_t idA = terms.insert(a);
    const std::int64_t idB = terms.insert(b);

    terms.setActive(idA);
    terms.setActive(idB);

    CHECK(terms.findById(idA)->isActive == false);
    CHECK(terms.findById(idB)->isActive == true);
    CHECK(terms.findActive()->id == idB);
}

TEST_CASE("TermRepository::setActive on an unknown id returns false and changes nothing") {
    Fixture f;
    repo::TermRepository terms(f.db);
    const std::int64_t id = f.seedTerm();

    CHECK_FALSE(terms.setActive(99999));
    CHECK(terms.findActive()->id == id);
}

// ---------------------------------------------------------------------
// CourseRepository
// ---------------------------------------------------------------------

TEST_CASE("CourseRepository round-trips with overrides null and set") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    repo::CourseRepository courses(f.db);

    model::Course c;
    c.termId = termId;
    c.code = "ENEE244";
    c.name = "Digital Logic";
    c.creditHours = 3.0;

    const std::int64_t id = courses.insert(c);
    auto found = courses.findById(id);
    REQUIRE(found.has_value());
    CHECK_FALSE(found->kpOverride.has_value());
    CHECK_FALSE(found->kiOverride.has_value());
    CHECK(found->workloadFactor == doctest::Approx(1.0));
    CHECK(found->targetScore == doctest::Approx(90.0));
    CHECK(found->isArchived == false);
    CHECK_FALSE(found->createdAt.empty());

    model::Course c2;
    c2.termId = termId;
    c2.code = "ENEE245";
    c2.name = "Signals";
    c2.creditHours = 4.0;
    c2.kpOverride = 0.5;
    c2.kiOverride = 0.1;
    const std::int64_t id2 = courses.insert(c2);
    auto found2 = courses.findById(id2);
    REQUIRE(found2.has_value());
    REQUIRE(found2->kpOverride.has_value());
    CHECK(*found2->kpOverride == doctest::Approx(0.5));
    REQUIRE(found2->kiOverride.has_value());
    CHECK(*found2->kiOverride == doctest::Approx(0.1));
}

TEST_CASE("CourseRepository rejects a duplicate (term_id, code)") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    repo::CourseRepository courses(f.db);
    model::Course c;
    c.termId = termId; c.code = "ENEE244"; c.name = "Digital Logic"; c.creditHours = 3.0;
    courses.insert(c);
    CHECK_THROWS_AS(courses.insert(c), db::SqliteError);
}

TEST_CASE("CourseRepository rejects an unknown term_id via foreign key") {
    Fixture f;
    repo::CourseRepository courses(f.db);
    model::Course c;
    c.termId = 999; c.code = "X"; c.name = "Y"; c.creditHours = 3.0;
    CHECK_THROWS_AS(courses.insert(c), db::SqliteError);
}

TEST_CASE("CourseRepository enforces CHECK constraints") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    repo::CourseRepository courses(f.db);

    auto make = [&](auto mutate) {
        model::Course c;
        c.termId = termId; c.code = "ENEE244"; c.name = "Digital Logic"; c.creditHours = 3.0;
        mutate(c);
        return c;
    };

    CHECK_THROWS_AS(courses.insert(make([](model::Course& c) { c.creditHours = 0; })),
                    db::SqliteError);
    CHECK_THROWS_AS(courses.insert(make([](model::Course& c) { c.creditHours = 7; })),
                    db::SqliteError);
    CHECK_THROWS_AS(courses.insert(make([](model::Course& c) { c.maxShareOfBudget = 1.5; })),
                    db::SqliteError);
    CHECK_THROWS_AS(courses.insert(make([](model::Course& c) { c.targetScore = 0; })),
                    db::SqliteError);
}

TEST_CASE("CourseRepository::update mutates fields but not is_archived or created_at") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    repo::CourseRepository courses(f.db);
    const std::int64_t id = f.seedCourse(termId);

    courses.archive(id);
    auto before = courses.findById(id);

    model::Course c = *before;
    c.name = "Digital Logic II";
    c.targetScore = 95.0;
    CHECK(courses.update(c));

    auto after = courses.findById(id);
    CHECK(after->name == "Digital Logic II");
    CHECK(after->targetScore == doctest::Approx(95.0));
    CHECK(after->isArchived == true);          // update() must not touch this
    CHECK(after->createdAt == before->createdAt);
}

TEST_CASE("CourseRepository::update on an unknown id returns false") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    repo::CourseRepository courses(f.db);
    model::Course c;
    c.id = 99999; c.termId = termId; c.code = "X"; c.name = "Y"; c.creditHours = 3.0;
    CHECK_FALSE(courses.update(c));
}

TEST_CASE("CourseRepository archive/unarchive and listActive") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    repo::CourseRepository courses(f.db);
    const std::int64_t id = f.seedCourse(termId, "ENEE244");

    CHECK(courses.listActive(termId).size() == 1);
    CHECK(courses.archive(id));
    CHECK(courses.listActive(termId).size() == 0);
    CHECK(courses.unarchive(id));
    CHECK(courses.listActive(termId).size() == 1);
    CHECK_FALSE(courses.archive(99999));
}

TEST_CASE("CourseRepository::listActive is scoped to one term and ordered by code") {
    Fixture f;
    const std::int64_t term1 = f.seedTerm("Fall 2026");
    const std::int64_t term2 = f.seedTerm("Spring 2027");
    repo::CourseRepository courses(f.db);
    f.seedCourse(term1, "ENEE245");
    f.seedCourse(term1, "ENEE244");
    f.seedCourse(term2, "ENEE300");

    auto list = courses.listActive(term1);
    REQUIRE(list.size() == 2);
    CHECK(list[0].code == "ENEE244");
    CHECK(list[1].code == "ENEE245");
}

TEST_CASE("CourseRepository::findByCode hits and misses") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    repo::CourseRepository courses(f.db);
    f.seedCourse(termId, "ENEE244");

    CHECK(courses.findByCode(termId, "ENEE244").has_value());
    CHECK_FALSE(courses.findByCode(termId, "ENEE999").has_value());
}

// ---------------------------------------------------------------------
// EntryRepository
// ---------------------------------------------------------------------

TEST_CASE("EntryRepository round-trips each kind") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::EntryRepository entries(f.db);

    const std::int64_t timeId = entries.insertTime(courseId, 2026, 37, 5.5, std::string("note"));
    auto time = entries.findById(timeId);
    REQUIRE(time.has_value());
    CHECK(time->kind == model::EntryKind::Time);
    CHECK(time->courseId == courseId);
    REQUIRE(time->hours.has_value());
    CHECK(*time->hours == doctest::Approx(5.5));
    REQUIRE(time->note.has_value());
    CHECK(*time->note == "note");

    const std::int64_t scoreId = entries.insertScore(courseId, 2026, 37, "Exam 1", 85.0, 100.0,
                                                       std::optional<double>(0.4), std::nullopt);
    auto score = entries.findById(scoreId);
    REQUIRE(score.has_value());
    CHECK(score->kind == model::EntryKind::Score);
    REQUIRE(score->label.has_value());
    CHECK(*score->label == "Exam 1");
    CHECK(*score->rawScore == doctest::Approx(85.0));
    CHECK(*score->maxScore == doctest::Approx(100.0));
    REQUIRE(score->weight.has_value());
    CHECK(*score->weight == doctest::Approx(0.4));
    CHECK_FALSE(score->note.has_value());

    const std::int64_t stressId = entries.insertStress(courseId, 2026, 37, 6, std::nullopt);
    auto stress = entries.findById(stressId);
    REQUIRE(stress.has_value());
    CHECK(stress->kind == model::EntryKind::Stress);
    REQUIRE(stress->stress.has_value());
    CHECK(*stress->stress == 6);
}

TEST_CASE("EntryRepository::insertStress accepts a null courseId for global stress") {
    Fixture f;
    repo::EntryRepository entries(f.db);
    const std::int64_t id = entries.insertStress(std::nullopt, 2026, 37, 4, std::nullopt);
    auto e = entries.findById(id);
    REQUIRE(e.has_value());
    CHECK_FALSE(e->courseId.has_value());
}

TEST_CASE("EntryRepository enforces CHECK constraints") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::EntryRepository entries(f.db);

    CHECK_THROWS_AS(entries.insertTime(courseId, 2026, 37, 61.0, std::nullopt), db::SqliteError);
    CHECK_THROWS_AS(entries.insertScore(courseId, 2026, 37, "X", 5.0, 0.0, std::nullopt, std::nullopt),
                    db::SqliteError);
    CHECK_THROWS_AS(entries.insertTime(courseId, 2026, 0, 1.0, std::nullopt), db::SqliteError);
    CHECK_THROWS_AS(entries.insertTime(courseId, 2026, 54, 1.0, std::nullopt), db::SqliteError);
}

TEST_CASE("EntryRepository::totalLiveHours sums only that course/week") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t c1 = f.seedCourse(termId, "ENEE244");
    const std::int64_t c2 = f.seedCourse(termId, "ENEE245");
    repo::EntryRepository entries(f.db);

    CHECK(entries.totalLiveHours(c1, 2026, 37) == doctest::Approx(0.0));

    entries.insertTime(c1, 2026, 37, 3.0, std::nullopt);
    entries.insertTime(c1, 2026, 37, 2.0, std::nullopt);
    entries.insertTime(c1, 2026, 38, 10.0, std::nullopt);   // different week
    entries.insertTime(c2, 2026, 37, 10.0, std::nullopt);   // different course

    CHECK(entries.totalLiveHours(c1, 2026, 37) == doctest::Approx(5.0));
}

TEST_CASE("EntryRepository::supersede sets superseded_by and excludes the row from live views") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::EntryRepository entries(f.db);

    const std::int64_t oldId = entries.insertTime(courseId, 2026, 37, 3.0, std::nullopt);
    CHECK(entries.totalLiveHours(courseId, 2026, 37) == doctest::Approx(3.0));

    const std::int64_t newId = entries.insertTime(courseId, 2026, 37, 5.0, std::nullopt);
    entries.supersede(oldId, newId);

    auto old = entries.findById(oldId);
    REQUIRE(old->supersededBy.has_value());
    CHECK(*old->supersededBy == newId);

    CHECK(entries.totalLiveHours(courseId, 2026, 37) == doctest::Approx(5.0));

    auto live = entries.listLive(courseId, 2026, 37);
    CHECK(live.size() == 1);
    CHECK(live[0].id == newId);
}

TEST_CASE("EntryRepository::supersede rejects a double-supersede") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::EntryRepository entries(f.db);

    const std::int64_t oldId = entries.insertTime(courseId, 2026, 37, 3.0, std::nullopt);
    const std::int64_t newId = entries.insertTime(courseId, 2026, 37, 5.0, std::nullopt);
    const std::int64_t newerId = entries.insertTime(courseId, 2026, 37, 6.0, std::nullopt);

    entries.supersede(oldId, newId);
    CHECK_THROWS_AS(entries.supersede(oldId, newerId), std::runtime_error);
}

TEST_CASE("EntryRepository::supersede rejects an entry superseding itself") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::EntryRepository entries(f.db);
    const std::int64_t id = entries.insertTime(courseId, 2026, 37, 3.0, std::nullopt);
    CHECK_THROWS_AS(entries.supersede(id, id), std::invalid_argument);
}

TEST_CASE("EntryRepository::supersede rejects an unknown newId via foreign key") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::EntryRepository entries(f.db);
    const std::int64_t oldId = entries.insertTime(courseId, 2026, 37, 3.0, std::nullopt);
    CHECK_THROWS_AS(entries.supersede(oldId, 99999), db::SqliteError);
}

TEST_CASE("insert-then-supersede inside a rolled-back transaction changes nothing") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::EntryRepository entries(f.db);
    const std::int64_t oldId = entries.insertTime(courseId, 2026, 37, 3.0, std::nullopt);

    {
        db::Transaction tx(f.db);
        const std::int64_t newId = entries.insertTime(courseId, 2026, 37, 5.0, std::nullopt);
        entries.supersede(oldId, newId);
        // no commit() -> rollback on scope exit
    }

    auto old = entries.findById(oldId);
    REQUIRE(old.has_value());
    CHECK_FALSE(old->supersededBy.has_value());
    CHECK(entries.listAllIncludingSuperseded(courseId, 2026, 37).size() == 1);
}

TEST_CASE("EntryRepository::weightedScore matches a hand-computed weighted mean") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::EntryRepository entries(f.db);

    CHECK_FALSE(entries.weightedScore(courseId, 2026, 37).has_value());

    // 80/100 weight 0.6, 90/100 weight null (treated as 1.0)
    entries.insertScore(courseId, 2026, 37, "HW1", 80.0, 100.0, std::optional<double>(0.6), std::nullopt);
    entries.insertScore(courseId, 2026, 37, "Exam", 90.0, 100.0, std::nullopt, std::nullopt);

    const double expected = (80.0 * 0.6 + 90.0 * 1.0) / (0.6 + 1.0);
    auto ws = entries.weightedScore(courseId, 2026, 37);
    REQUIRE(ws.has_value());
    CHECK(*ws == doctest::Approx(expected));
}

TEST_CASE("EntryRepository::meanStress averages across courses and null-course entries") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t c1 = f.seedCourse(termId, "ENEE244");
    const std::int64_t c2 = f.seedCourse(termId, "ENEE245");
    repo::EntryRepository entries(f.db);

    CHECK_FALSE(entries.meanStress(2026, 37).has_value());

    entries.insertStress(c1, 2026, 37, 4, std::nullopt);
    entries.insertStress(c2, 2026, 37, 6, std::nullopt);
    entries.insertStress(std::nullopt, 2026, 37, 8, std::nullopt);

    auto mean = entries.meanStress(2026, 37);
    REQUIRE(mean.has_value());
    CHECK(*mean == doctest::Approx((4.0 + 6.0 + 8.0) / 3.0));
}

TEST_CASE("deleting a course cascades its entries away") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::EntryRepository entries(f.db);
    const std::int64_t entryId = entries.insertTime(courseId, 2026, 37, 3.0, std::nullopt);

    f.db.exec("DELETE FROM courses WHERE id = " + std::to_string(courseId) + ";");

    CHECK_FALSE(entries.findById(entryId).has_value());
}

// ---------------------------------------------------------------------
// AllocationRepository
// ---------------------------------------------------------------------

TEST_CASE("AllocationRepository round-trips all fields") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::AllocationRepository allocations(f.db);

    model::Allocation a;
    a.courseId = courseId;
    a.year = 2026; a.weekNo = 37;
    a.computedAt = "2026-08-21 10:00:00";
    a.smoothedScore = 82.5; a.error = -7.5; a.integral = -3.0;
    a.kpEffective = 0.35; a.kiEffective = 0.08;
    a.deltaP = -2.6; a.deltaI = -0.24;
    a.hoursPrev = 5.0; a.hoursRaw = 4.0; a.hoursSaturated = 4.0; a.hoursFinal = 4.2;
    a.wasClampedLow = true; a.wasClampedHigh = false;
    a.normalizerIters = 3;

    const std::int64_t id = allocations.insert(a);
    auto found = allocations.findLatest(courseId, 2026, 37);
    REQUIRE(found.has_value());
    CHECK(found->id == id);
    CHECK(found->computedAt == "2026-08-21 10:00:00");
    CHECK(found->smoothedScore == doctest::Approx(82.5));
    CHECK(found->error == doctest::Approx(-7.5));
    CHECK(found->integral == doctest::Approx(-3.0));
    CHECK(found->kpEffective == doctest::Approx(0.35));
    CHECK(found->kiEffective == doctest::Approx(0.08));
    CHECK(found->deltaP == doctest::Approx(-2.6));
    CHECK(found->deltaI == doctest::Approx(-0.24));
    CHECK(found->hoursPrev == doctest::Approx(5.0));
    CHECK(found->hoursRaw == doctest::Approx(4.0));
    CHECK(found->hoursSaturated == doctest::Approx(4.0));
    CHECK(found->hoursFinal == doctest::Approx(4.2));
    CHECK(found->wasClampedLow == true);
    CHECK(found->wasClampedHigh == false);
    CHECK(found->normalizerIters == 3);
}

TEST_CASE("AllocationRepository::insert rejects an empty computedAt") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::AllocationRepository allocations(f.db);
    model::Allocation a;
    a.courseId = courseId; a.year = 2026; a.weekNo = 37;
    CHECK_THROWS_AS(allocations.insert(a), std::invalid_argument);
}

TEST_CASE("AllocationRepository rejects two rows with the same course/year/week/computed_at") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::AllocationRepository allocations(f.db);
    model::Allocation a;
    a.courseId = courseId; a.year = 2026; a.weekNo = 37;
    a.computedAt = "2026-08-21 10:00:00";

    allocations.insert(a);
    CHECK_THROWS_AS(allocations.insert(a), db::SqliteError);
}

TEST_CASE("AllocationRepository::findLatest picks the newest of several rows") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::AllocationRepository allocations(f.db);

    model::Allocation a;
    a.courseId = courseId; a.year = 2026; a.weekNo = 37;

    a.computedAt = "2026-08-21 10:00:00"; a.hoursFinal = 1.0; allocations.insert(a);
    a.computedAt = "2026-08-21 10:00:02"; a.hoursFinal = 2.0; allocations.insert(a);
    a.computedAt = "2026-08-21 10:00:01"; a.hoursFinal = 3.0; allocations.insert(a);

    auto latest = allocations.findLatest(courseId, 2026, 37);
    REQUIRE(latest.has_value());
    CHECK(latest->computedAt == "2026-08-21 10:00:02");
    CHECK(latest->hoursFinal == doctest::Approx(2.0));
}

TEST_CASE("AllocationRepository::listLatestForWeek returns one row per course") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t c1 = f.seedCourse(termId, "ENEE244");
    const std::int64_t c2 = f.seedCourse(termId, "ENEE245");
    repo::AllocationRepository allocations(f.db);

    model::Allocation a;
    a.year = 2026; a.weekNo = 37;

    a.courseId = c1; a.computedAt = "2026-08-21 10:00:00"; a.hoursFinal = 1.0; allocations.insert(a);
    a.courseId = c1; a.computedAt = "2026-08-21 10:00:01"; a.hoursFinal = 2.0; allocations.insert(a);
    a.courseId = c2; a.computedAt = "2026-08-21 10:00:00"; a.hoursFinal = 5.0; allocations.insert(a);

    auto latest = allocations.listLatestForWeek(2026, 37);
    REQUIRE(latest.size() == 2);
    for (const auto& row : latest) {
        if (row.courseId == c1) CHECK(row.hoursFinal == doctest::Approx(2.0));
        if (row.courseId == c2) CHECK(row.hoursFinal == doctest::Approx(5.0));
    }
}

TEST_CASE("AllocationRepository::listHistory is newest-first and honors limit") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId);
    repo::AllocationRepository allocations(f.db);

    model::Allocation a;
    a.courseId = courseId; a.year = 2026; a.weekNo = 37;
    a.computedAt = "2026-08-21 10:00:00"; a.hoursFinal = 1.0; allocations.insert(a);
    a.computedAt = "2026-08-21 10:00:01"; a.hoursFinal = 2.0; allocations.insert(a);
    a.computedAt = "2026-08-21 10:00:02"; a.hoursFinal = 3.0; allocations.insert(a);

    auto history = allocations.listHistory(courseId, 2);
    REQUIRE(history.size() == 2);
    CHECK(history[0].hoursFinal == doctest::Approx(3.0));
    CHECK(history[1].hoursFinal == doctest::Approx(2.0));
}

// ---------------------------------------------------------------------
// ConfigRepository
// ---------------------------------------------------------------------

TEST_CASE("ConfigRepository::get on a missing key is nullopt, getOr falls back") {
    Fixture f;
    repo::ConfigRepository config(f.db);
    CHECK_FALSE(config.get("missing.key").has_value());
    CHECK(config.getOr("missing.key", "fallback") == "fallback");
}

TEST_CASE("ConfigRepository::set then get round-trips, and set twice leaves one row") {
    Fixture f;
    repo::ConfigRepository config(f.db);
    config.set("some.key", "v1");
    CHECK(config.get("some.key") == "v1");

    config.set("some.key", "v2");
    CHECK(config.get("some.key") == "v2");

    auto all = config.listAll();
    int count = 0;
    for (const auto& [k, v] : all) if (k == "some.key") ++count;
    CHECK(count == 1);
}

TEST_CASE("ConfigRepository::setDouble/getDouble round-trip exactly") {
    Fixture f;
    repo::ConfigRepository config(f.db);
    for (double v : {0.1, 1.0 / 3.0, 1e-9, 1e300}) {
        config.setDouble("x", v);
        auto got = config.getDouble("x");
        REQUIRE(got.has_value());
        CHECK(*got == v);
    }
}

TEST_CASE("ConfigRepository::getDouble on non-numeric text returns nullopt") {
    Fixture f;
    repo::ConfigRepository config(f.db);
    config.set("bad", "not a number");
    CHECK_FALSE(config.getDouble("bad").has_value());
}

TEST_CASE("ConfigRepository::remove returns true then false") {
    Fixture f;
    repo::ConfigRepository config(f.db);
    config.set("k", "v");
    CHECK(config.remove("k"));
    CHECK_FALSE(config.remove("k"));
}

TEST_CASE("ConfigRepository::loadControllerParams returns defaults on empty config") {
    Fixture f;
    repo::ConfigRepository config(f.db);
    model::ControllerParams defaults;
    auto loaded = config.loadControllerParams();
    CHECK(loaded.kp == doctest::Approx(defaults.kp));
    CHECK(loaded.ki == doctest::Approx(defaults.ki));
    CHECK(loaded.ewmaAlpha == doctest::Approx(defaults.ewmaAlpha));
    CHECK(loaded.integralClamp == doctest::Approx(defaults.integralClamp));
    CHECK(loaded.weeklyBudgetHours == doctest::Approx(defaults.weeklyBudgetHours));
    CHECK(loaded.stressThreshold == doctest::Approx(defaults.stressThreshold));
    CHECK(loaded.stressBudgetScale == doctest::Approx(defaults.stressBudgetScale));
    CHECK(loaded.stressGainScale == doctest::Approx(defaults.stressGainScale));
}

TEST_CASE("ConfigRepository::saveControllerParams then loadControllerParams round-trips") {
    Fixture f;
    repo::ConfigRepository config(f.db);
    model::ControllerParams p;
    p.kp = 0.5; p.ki = 0.2; p.ewmaAlpha = 0.6; p.integralClamp = 30.0;
    p.weeklyBudgetHours = 20.0; p.stressThreshold = 8.0;
    p.stressBudgetScale = 0.7; p.stressGainScale = 0.4;

    config.saveControllerParams(p);
    auto loaded = config.loadControllerParams();
    CHECK(loaded.kp == doctest::Approx(0.5));
    CHECK(loaded.ki == doctest::Approx(0.2));
    CHECK(loaded.ewmaAlpha == doctest::Approx(0.6));
    CHECK(loaded.integralClamp == doctest::Approx(30.0));
    CHECK(loaded.weeklyBudgetHours == doctest::Approx(20.0));
    CHECK(loaded.stressThreshold == doctest::Approx(8.0));
    CHECK(loaded.stressBudgetScale == doctest::Approx(0.7));
    CHECK(loaded.stressGainScale == doctest::Approx(0.4));
}

// ---------------------------------------------------------------------
// CourseRepository::weekView
// ---------------------------------------------------------------------

TEST_CASE("weekView: a course with no data yields zeros and nullopts") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    f.seedCourse(termId, "ENEE244");
    repo::CourseRepository courses(f.db);

    auto view = courses.weekView(termId, 2026, 37);
    REQUIRE(view.size() == 1);
    CHECK(view[0].loggedHours == doctest::Approx(0.0));
    CHECK_FALSE(view[0].weightedScore.has_value());
    CHECK_FALSE(view[0].allocatedHours.has_value());
    CHECK_FALSE(view[0].remainingHours.has_value());
}

TEST_CASE("weekView: excludes superseded time entries, includes only live sum") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId, "ENEE244");
    repo::CourseRepository courses(f.db);
    repo::EntryRepository entries(f.db);

    const std::int64_t oldId = entries.insertTime(courseId, 2026, 37, 3.0, std::nullopt);
    const std::int64_t newId = entries.insertTime(courseId, 2026, 37, 5.0, std::nullopt);
    entries.supersede(oldId, newId);

    auto view = courses.weekView(termId, 2026, 37);
    REQUIRE(view.size() == 1);
    CHECK(view[0].loggedHours == doctest::Approx(5.0));
}

TEST_CASE("weekView: weighted score matches a hand-computed weighted mean") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId, "ENEE244");
    repo::CourseRepository courses(f.db);
    repo::EntryRepository entries(f.db);

    entries.insertScore(courseId, 2026, 37, "HW1", 80.0, 100.0, std::optional<double>(0.6), std::nullopt);
    entries.insertScore(courseId, 2026, 37, "Exam", 90.0, 100.0, std::nullopt, std::nullopt);
    const double expected = (80.0 * 0.6 + 90.0 * 1.0) / (0.6 + 1.0);

    auto view = courses.weekView(termId, 2026, 37);
    REQUIRE(view.size() == 1);
    REQUIRE(view[0].weightedScore.has_value());
    CHECK(*view[0].weightedScore == doctest::Approx(expected));
}

TEST_CASE("weekView: with two allocations the later computed_at wins, remainingHours computed") {
    Fixture f;
    const std::int64_t termId = f.seedTerm();
    const std::int64_t courseId = f.seedCourse(termId, "ENEE244");
    repo::CourseRepository courses(f.db);
    repo::EntryRepository entries(f.db);
    repo::AllocationRepository allocations(f.db);

    entries.insertTime(courseId, 2026, 37, 2.0, std::nullopt);

    model::Allocation a;
    a.courseId = courseId; a.year = 2026; a.weekNo = 37;
    a.computedAt = "2026-08-21 10:00:00"; a.hoursFinal = 4.0; allocations.insert(a);
    a.computedAt = "2026-08-21 10:00:05"; a.hoursFinal = 6.0; allocations.insert(a);

    auto view = courses.weekView(termId, 2026, 37);
    REQUIRE(view.size() == 1);
    REQUIRE(view[0].allocatedHours.has_value());
    CHECK(*view[0].allocatedHours == doctest::Approx(6.0));
    REQUIRE(view[0].remainingHours.has_value());
    CHECK(*view[0].remainingHours == doctest::Approx(4.0));   // 6.0 - 2.0
}

TEST_CASE("weekView: excludes archived courses and other terms' courses") {
    Fixture f;
    const std::int64_t term1 = f.seedTerm("Fall 2026");
    const std::int64_t term2 = f.seedTerm("Spring 2027");
    repo::CourseRepository courses(f.db);

    const std::int64_t archivedId = f.seedCourse(term1, "ENEE244");
    f.seedCourse(term1, "ENEE245");
    f.seedCourse(term2, "ENEE300");
    courses.archive(archivedId);

    auto view = courses.weekView(term1, 2026, 37);
    REQUIRE(view.size() == 1);
    CHECK(view[0].code == "ENEE245");
}
