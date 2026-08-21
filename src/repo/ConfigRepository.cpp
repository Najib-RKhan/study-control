#include "ConfigRepository.hpp"

#include <cstdlib>
#include <cstdio>

namespace studyctl::repo {
namespace {

std::optional<double> parseDouble(const std::string& s) {
    if (s.empty()) return std::nullopt;
    const char* cstr = s.c_str();
    char* endptr = nullptr;
    const double v = std::strtod(cstr, &endptr);
    if (endptr == cstr || *endptr != '\0') return std::nullopt;
    return v;
}

std::optional<std::int64_t> parseInt(const std::string& s) {
    if (s.empty()) return std::nullopt;
    const char* cstr = s.c_str();
    char* endptr = nullptr;
    const long long v = std::strtoll(cstr, &endptr, 10);
    if (endptr == cstr || *endptr != '\0') return std::nullopt;
    return static_cast<std::int64_t>(v);
}

}  // namespace

std::optional<std::string> ConfigRepository::get(const std::string& key) {
    db::Statement st = db_.prepare("SELECT value FROM config WHERE key = ?;");
    st.bind(1, key);
    if (!st.step()) return std::nullopt;
    return st.columnText(0);
}

std::string ConfigRepository::getOr(const std::string& key, const std::string& fallback) {
    auto v = get(key);
    return v ? *v : fallback;
}

void ConfigRepository::set(const std::string& key, const std::string& value) {
    db::Statement st = db_.prepare(
        "INSERT INTO config(key, value, updated_at) VALUES (?1, ?2, datetime('now'))"
        " ON CONFLICT(key) DO UPDATE SET value = excluded.value,"
        " updated_at = datetime('now');");
    st.bind(1, key).bind(2, value);
    st.execute();
}

std::optional<double> ConfigRepository::getDouble(const std::string& key) {
    auto v = get(key);
    if (!v) return std::nullopt;
    return parseDouble(*v);
}

double ConfigRepository::getDoubleOr(const std::string& key, double fallback) {
    auto v = getDouble(key);
    return v ? *v : fallback;
}

void ConfigRepository::setDouble(const std::string& key, double value) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.15g", value);
    if (std::strtod(buf, nullptr) != value)   // 15 digits was not enough
        std::snprintf(buf, sizeof buf, "%.17g", value);
    set(key, buf);
}

std::optional<std::int64_t> ConfigRepository::getInt(const std::string& key) {
    auto v = get(key);
    if (!v) return std::nullopt;
    return parseInt(*v);
}

void ConfigRepository::setInt(const std::string& key, std::int64_t value) {
    set(key, std::to_string(value));
}

bool ConfigRepository::remove(const std::string& key) {
    db::Statement st = db_.prepare("DELETE FROM config WHERE key = ?;");
    st.bind(1, key);
    st.execute();
    return db_.changes() == 1;
}

std::vector<std::pair<std::string, std::string>> ConfigRepository::listAll() {
    db::Statement st = db_.prepare("SELECT key, value FROM config ORDER BY key;");
    std::vector<std::pair<std::string, std::string>> out;
    while (st.step()) out.emplace_back(st.columnText(0), st.columnText(1));
    return out;
}

model::ControllerParams ConfigRepository::loadControllerParams() {
    model::ControllerParams p;   // defaults
    p.kp                = getDoubleOr(model::kConfigKeyKp, p.kp);
    p.ki                = getDoubleOr(model::kConfigKeyKi, p.ki);
    p.ewmaAlpha          = getDoubleOr(model::kConfigKeyEwmaAlpha, p.ewmaAlpha);
    p.integralClamp      = getDoubleOr(model::kConfigKeyIntegralClamp, p.integralClamp);
    p.weeklyBudgetHours   = getDoubleOr(model::kConfigKeyWeeklyBudgetHours, p.weeklyBudgetHours);
    p.stressThreshold     = getDoubleOr(model::kConfigKeyStressThreshold, p.stressThreshold);
    p.stressBudgetScale   = getDoubleOr(model::kConfigKeyStressBudgetScale, p.stressBudgetScale);
    p.stressGainScale     = getDoubleOr(model::kConfigKeyStressGainScale, p.stressGainScale);
    return p;
}

void ConfigRepository::saveControllerParams(const model::ControllerParams& p) {
    setDouble(model::kConfigKeyKp, p.kp);
    setDouble(model::kConfigKeyKi, p.ki);
    setDouble(model::kConfigKeyEwmaAlpha, p.ewmaAlpha);
    setDouble(model::kConfigKeyIntegralClamp, p.integralClamp);
    setDouble(model::kConfigKeyWeeklyBudgetHours, p.weeklyBudgetHours);
    setDouble(model::kConfigKeyStressThreshold, p.stressThreshold);
    setDouble(model::kConfigKeyStressBudgetScale, p.stressBudgetScale);
    setDouble(model::kConfigKeyStressGainScale, p.stressGainScale);
}

}  // namespace studyctl::repo
