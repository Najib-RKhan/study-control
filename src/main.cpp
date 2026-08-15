#include "db/Database.hpp"
#include "db/Migrator.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

constexpr const char* kDbFile = "studyctl.db";

/// Migrations ship next to the executable (CMake copies sql/ there after the
/// build), so the tool works from any working directory. Falls back to ./sql
/// for the case where it is run straight out of the source tree.
fs::path sqlDir(const char* argv0) {
    if (argv0) {
        std::error_code ec;
        const fs::path beside = fs::absolute(argv0, ec).parent_path() / "sql";
        if (!ec && fs::is_directory(beside)) return beside;
    }
    return "sql";
}

} // namespace

int main(int argc, char** argv) {
    try {
        studyctl::db::Database db(kDbFile);

        const int before = studyctl::db::Migrator::currentVersion(db);
        studyctl::db::Migrator::migrate(db, sqlDir(argc > 0 ? argv[0] : nullptr));
        const int after = studyctl::db::Migrator::currentVersion(db);

        if (after > before)
            std::cout << "migrated schema v" << before << " -> v" << after << "\n";
        else
            std::cout << "schema up to date (v" << after << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "studyctl: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
