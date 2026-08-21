#pragma once
#include <cstdint>
#include <string>

namespace studyctl::model {

struct Allocation {
    std::int64_t id = 0;
    std::int64_t courseId = 0;
    int year = 0;
    int weekNo = 0;
    std::string computedAt;   // must be set explicitly; see AllocationRepository

    // controller internals, persisted for tuning
    double smoothedScore = 0.0;
    double error = 0.0;
    double integral = 0.0;
    double kpEffective = 0.0;
    double kiEffective = 0.0;
    double deltaP = 0.0;
    double deltaI = 0.0;
    double hoursPrev = 0.0;
    double hoursRaw = 0.0;         // before saturation
    double hoursSaturated = 0.0;   // after per-course clamp
    double hoursFinal = 0.0;       // after budget normalization
    bool wasClampedLow = false;
    bool wasClampedHigh = false;
    int normalizerIters = 0;
};

}  // namespace studyctl::model
