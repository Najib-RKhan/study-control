# studyctl

## Notation

| Symbol       | Meaning                                          | Source              |
|--------------|---------------------------------------------------|----------------------|
| `i`          | course index                                       | —                    |
| `k`          | week index                                         | —                    |
| `s[i,k]`     | raw score percentage this week (weighted mean of assessments) | entries              |
| `s^[i,k]`    | EWMA-smoothed score                                | computed             |
| `r[i]`       | target score                                       | courses.target_score |
| `e[i,k]`     | normalized error                                   | computed             |
| `I[i,k]`     | clamped integral of error                          | computed             |
| `T[i,k]`     | hours actually logged in week k                    | entries              |
| `T*[i,k+1]`  | allocated hours for week k+1                       | allocations           |
| `sig[k]`     | mean stress in week k                              | entries              |
| `B[k+1]`     | total weekly study budget                          | config               |
| `c[i]`       | credit hours                                       | courses               |
| `w[i]`       | workload factor                                    | courses               |
