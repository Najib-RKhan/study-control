# study-control
A closed-loop study time allocator. Instead of running by a static weekly schedule, study-control uses exam scores, logged study hours, and self-reported stress to feed a PI controller that recomputes how much time each course should get in the following week. The goal is to hit the target scores set while using the minimum possible time, improving study efficiency.

## How it works
Each course has a target score and baseline based on credits. Every new exam scores generates an error signal, which the PI controller converts into a time-correction. This time-correction is scaled based on how many hours were already invested, so classes with more room to improve get a proportionally bigger change. Stress level acts as a damper, reducing the aggressiveness of the changes and shrinking the weekly budget for studying when stress crosses a threshold. This is done to prevent any burnout spiral.

## Notation

| Symbol       | Meaning                                                       | Source               |
|--------------|---------------------------------------------------------------|----------------------|
| `i`          | course index                                                  | —                    |
| `k`          | week index                                                    | —                    |
| `s[i,k]`     | raw score percentage this week (weighted mean of assessments) | entries              |
| `s^[i,k]`    | EWMA-smoothed score                                           | computed             |
| `r[i]`       | target score                                                  | courses.target_score |
| `e[i,k]`     | normalized error                                              | computed             |
| `I[i,k]`     | clamped integral of error                                     | computed             |
| `T[i,k]`     | hours actually logged in week k                               | entries              |
| `T*[i,k+1]`  | allocated hours for week k+1                                  | allocations           |
| `sig[k]`     | mean stress in week k                                         | entries              |
| `B[k+1]`     | total weekly study budget                                     | config               |
| `c[i]`       | credit hours                                                  | courses               |
| `w[i]`       | workload factor                                               | courses               |
