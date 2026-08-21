#pragma once

namespace studyctl::model {

// Config keys these fields are persisted under via ConfigRepository. Declared
// here so ConfigRepository and the controller can't drift apart on the key
// strings.
inline constexpr const char* kConfigKeyKp                 = "controller.kp";
inline constexpr const char* kConfigKeyKi                 = "controller.ki";
inline constexpr const char* kConfigKeyEwmaAlpha           = "controller.ewma_alpha";
inline constexpr const char* kConfigKeyIntegralClamp       = "controller.integral_clamp";
inline constexpr const char* kConfigKeyWeeklyBudgetHours   = "budget.weekly_hours";
inline constexpr const char* kConfigKeyStressThreshold     = "stress.threshold";
inline constexpr const char* kConfigKeyStressBudgetScale   = "stress.budget_scale";
inline constexpr const char* kConfigKeyStressGainScale     = "stress.gain_scale";

// Not a table - persisted as key/value pairs in `config` via
// ConfigRepository::loadControllerParams/saveControllerParams. Defaults
// below are placeholders for M5 to tune.
struct ControllerParams {
    double kp = 0.35;
    double ki = 0.08;
    double ewmaAlpha = 0.40;
    double integralClamp = 25.0;
    double weeklyBudgetHours = 25.0;
    double stressThreshold = 7.0;
    double stressBudgetScale = 0.80;
    double stressGainScale = 0.50;
};

}  // namespace studyctl::model
