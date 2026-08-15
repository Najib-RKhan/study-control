#pragma once
#include "Database.hpp"
#include <filesystem>

namespace studyctl::db {

/// Applies versioned SQL migrations from a directory of NNN_description.sql
/// files, tracking what has been applied in the schema_version table.
class Migrator {
public:
    Migrator() = delete;

    /// Highest applied migration version, or 0 on a fresh database.
    /// Creates schema_version if it does not exist yet.
    static int currentVersion(Database& db);

    /// Applies every migration newer than currentVersion(), in version order.
    /// Each file runs in its own transaction, so a failure part-way through
    /// leaves the database at the last successfully applied version.
    static void migrate(Database& db, const std::filesystem::path& sqlDir);
};

}
