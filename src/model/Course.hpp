#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace studyctl::model {

struct Course {
    std::int64_t id = 0;
    std::int64_t termId = 0;
    std::string  code;
    std::string  name;
    double creditHours = 3.0;
    double workloadFactor = 1.0;
    double targetScore = 90.0;
    std::optional<double> kpOverride;   // nullopt = use global
    std::optional<double> kiOverride;   // nullopt = use global
    double minHoursPerWeek = 1.0;
    double maxShareOfBudget = 0.40;
    bool isArchived = false;
    std::string createdAt;
};

}  // namespace studyctl::model
