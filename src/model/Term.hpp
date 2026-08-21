#pragma once
#include <cstdint>
#include <string>

namespace studyctl::model {

struct Term {
    std::int64_t id = 0;
    std::string  label;
    int startYear = 0;
    int startWeek = 0;
    int endYear = 0;
    int endWeek = 0;
    bool isActive = false;
};

}  // namespace studyctl::model
