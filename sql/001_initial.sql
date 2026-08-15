PRAGMA foreign_keys = ON;

----------------------------------------------------------------------------
-- Migration bookkeeping
----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS schema_version (
    version     INTEGER PRIMARY KEY,
    applied_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

----------------------------------------------------------------------------
-- Global and per-term configuration (key/value, typed by convention)
----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS config(
    key             TEXT PRIMARY KEY,
    value           TEXT NOT NULL,
    updated_at      TEXT NOT NULL DEFAULT (datetime('now'))
);

----------------------------------------------------------------------------
-- Courses
----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS courses (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    term_id             INTEGER NOT NULL REFERENCES terms(id) ON DELETE CASCADE,
    code                TEXT NOT NULL,                  -- 'ENEE244'
    name                TEXT NOT NULL,
    credit_hours        REAL NOT NULL CHECK (credit_hours > 0 AND credit_hours <= 6),
    workload_factor     REAL NOT NULL DEFAULT 1.0
                            CHECK (workload_factor >= 0.25 AND workload_factor <= 4.0),
    target_score        REAL NOT NULL DEFAULT 90.0
                            CHECK (target_score > 0 AND target_score <= 100),
    kp_override         REAL,       -- NULL = use global
    ki_override         REAL,       -- NULL = use global
    min_hours_per_week  REAL NOT NULL DEFAULT 1.0 CHECK (min_hours_per_week >= 0),
    max_share_of_budget REAL NOT NULL DEFAULT 0.40
                            CHECK (max_share_of_budget > 0 AND max_share_of_budget <= 1.0),
    is_archived         INTEGER NOT NULL DEFAULT 0 CHECK (is_archived IN (0,1)),
    created_at          TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE (term_id, code)
);

CREATE INDEX IF NOT EXISTS idx_courses_term_active
    ON courses(term_id, is_archived);

----------------------------------------------------------------------------
-- Terms
----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS terms (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    label               TEXT NOT NULL UNIQUE,
    start_year          INTEGER NOT NULL,
    start_week          INTEGER NOT NULL,
    end_year            INTEGER NOT NULL,
    end_week            INTEGER NOT NULL,
    is_active           INTEGER NOT NULL DEFAULT 0 CHECK (is_active IN (0, 1))
);

----------------------------------------------------------------------------
-- Entries: append-only observation log
----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS entries (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    course_id           INTEGER REFERENCES courses(id) ON DELETE CASCADE, -- NULL for global stress
    year                INTEGER NOT NULL,
    week_no             INTEGER NOT NULL CHECK (week_no BETWEEN 1 and 53),
    kind                TEXT NOT NULL CHECK (kind IN ('TIME', 'SCORE', 'STRESS')),

    -- TIME
    hours               REAL CHECK (hours IS NULL OR (hours >= 0 AND hours <= 60)),

    -- SCORE
    label               TEXT,                       -- 'Exam 1', 'HW 3'
    raw_score           REAL CHECK (raw_score IS NULL OR raw_score >= 0),
    max_score           REAL CHECK (max_score IS NULL OR max_score > 0),
    weight              REAL CHECK (weight IS NULL OR (weight >= 0 AND weight <= 1)),

    -- STRESS
    stress              INTEGER CHECK (stress is NULL OR (stress BETWEEN 1 AND 10)),

    note                TEXT,
    created_at          TEXT NOT NULL DEFAULT (datetime('now')),
    superseded_by       INTEGER REFERENCES entries(id),

    -- structural guarantee: the right columns are populated for each kind
    CHECK (
            (kind = 'TIME' AND hours IS NOT NULL AND course_id IS NOT NULL)
        OR  (kind = 'SCORE' AND raw_score IS NOT NULL AND max_score IS NOT NULL
                            AND course_id IS NOT NULL)
        OR  (kind = 'STRESS' AND stress IS NOT NULL)
    ) 
);

CREATE INDEX IF NOT EXISTS idx_entries_course_week
    ON entries(course_id, year, week_no, kind);
CREATE INDEX IF NOT EXISTS idx_entries_live
    ON entries(year, week_no, kind) WHERE superseded_by IS NULL;

----------------------------------------------------------------------------
-- Allocations: derived controller output + full audit trail
----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS allocations(
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    course_id           INTEGER NOT NULL REFERENCES courses(id) ON DELETE CASCADE,
    year                INTEGER NOT NULL,       -- week this allocation is FOR
    week_no             INTEGER NOT NULL,
    computed_at         TEXT NOT NULL DEFAULT (datetime('now')),

    -- controller internals, persisted for tuning
    smoothed_score      REAL NOT NULL,
    error               REAL NOT NULL,
    integral            REAL NOT NULL,
    kp_effective        REAL NOT NULL,
    ki_effective        REAL NOT NULL,
    delta_p             REAL NOT NULL,
    delta_i             REAL NOT NULL,
    hours_prev          REAL NOT NULL,
    hours_raw           REAL NOT NULL, -- before saturation
    hours_saturated     REAL NOT NULL, -- after per-course clamp
    hours_final         REAL NOT NULL, -- after budget normalization
    was_clamped_low     INTEGER NOT NULL DEFAULT 0,
    was_clamped_high    INTEGER NOT NULL DEFAULT 0,
    normalizer_iters    INTEGER NOT NULL DEFAULT 0,

    UNIQUE (course_id, year, week_no, computed_at)
);

CREATE INDEX IF NOT EXISTS idx_alloc_week
    ON allocations(year, week_no, computed_at DESC);