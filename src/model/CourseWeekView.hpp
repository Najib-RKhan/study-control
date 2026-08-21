#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace studyctl::model {

// One dashboard row per active course for a given (year, week_no), assembled
// by CourseRepository::weekView in a single SQL statement.
struct CourseWeekView {
    std::int64_t courseId = 0;
    std::string code;
    std::string name;
    double creditHours = 0.0;
    double workloadFactor = 0.0;
    double targetScore = 0.0;
    double loggedHours = 0.0;                  // 0.0 when nothing logged
    std::optional<double> weightedScore;       // nullopt when no scores this week
    std::optional<double> allocatedHours;      // nullopt when never allocated
    std::optional<double> remainingHours;      // allocated - logged
};

}  // namespace studyctl::model
