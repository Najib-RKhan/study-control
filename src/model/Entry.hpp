#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace studyctl::model {

enum class EntryKind { Time, Score, Stress };

inline const char* toSql(EntryKind k) {
    switch (k) {
        case EntryKind::Time:   return "TIME";
        case EntryKind::Score:  return "SCORE";
        case EntryKind::Stress: return "STRESS";
    }
    throw std::invalid_argument("unknown EntryKind");
}

inline EntryKind entryKindFromSql(const std::string& s) {
    if (s == "TIME")   return EntryKind::Time;
    if (s == "SCORE")  return EntryKind::Score;
    if (s == "STRESS") return EntryKind::Stress;
    throw std::invalid_argument("unknown entry kind: " + s);
}

struct Entry {
    std::int64_t id = 0;
    std::optional<std::int64_t> courseId;   // NULL only legal for STRESS
    int year = 0;
    int weekNo = 0;
    EntryKind kind = EntryKind::Time;

    // TIME
    std::optional<double> hours;

    // SCORE
    std::optional<std::string> label;
    std::optional<double> rawScore;
    std::optional<double> maxScore;
    std::optional<double> weight;

    // STRESS
    std::optional<std::int64_t> stress;

    std::optional<std::string> note;
    std::string createdAt;
    std::optional<std::int64_t> supersededBy;
};

}  // namespace studyctl::model
