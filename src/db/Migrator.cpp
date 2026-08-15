#include "Migrator.hpp"

#include "SqliteError.hpp"
#include "Transaction.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace studyctl::db {
namespace {

/// Migration files are named NNN_description.sql, NNN being the version.
constexpr std::size_t kVersionDigits = 3;

std::string readFile(const std::filesystem::path& p) {
    // Binary mode: the SQL goes to sqlite verbatim, no newline translation.
    std::ifstream in(p, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open migration file: " + p.string());

    std::ostringstream out;
    out << in.rdbuf();
    // Not `if (!out)`: inserting an empty file sets failbit on the stringstream,
    // and an empty migration is legal. Only a real read error matters here.
    if (in.bad()) throw std::runtime_error("cannot read migration file: " + p.string());
    return out.str();
}

/// Version prefix of a migration filename, or nullopt if it is not one.
std::optional<int> parseVersion(const std::string& filename) {
    if (filename.size() <= kVersionDigits) return std::nullopt;
    for (std::size_t i = 0; i < kVersionDigits; ++i)
        if (!std::isdigit(static_cast<unsigned char>(filename[i]))) return std::nullopt;
    return std::stoi(filename.substr(0, kVersionDigits));
}

}  // namespace

int Migrator::currentVersion(Database& db) {
    db.exec("CREATE TABLE IF NOT EXISTS schema_version ("
            " version INTEGER PRIMARY KEY,"
            " applied_at TEXT NOT NULL DEFAULT (datetime('now')));");
    Statement st = db.prepare("SELECT COALESCE(MAX(version), 0) FROM schema_version;");
    if (!st.step()) throw SqliteError(SQLITE_ERROR, "schema_version returned no row");
    return st.columnInt(0);
}

void Migrator::migrate(Database& db, const std::filesystem::path& sqlDir) {
    if (!std::filesystem::is_directory(sqlDir))
        throw std::runtime_error("migration directory not found: " + sqlDir.string());

    const int have = currentVersion(db);

    std::vector<std::pair<int, std::filesystem::path>> pending;
    for (const auto& e : std::filesystem::directory_iterator(sqlDir)) {
        if (!e.is_regular_file() || e.path().extension() != ".sql") continue;
        // Anything not matching the NNN_ convention is not a migration.
        const std::optional<int> v = parseVersion(e.path().filename().string());
        if (v && *v > have) pending.emplace_back(*v, e.path());
    }

    std::sort(pending.begin(), pending.end());

    // Two files claiming one version would otherwise fail mid-run on the
    // schema_version primary key, after the first had already been applied.
    for (std::size_t i = 1; i < pending.size(); ++i)
        if (pending[i].first == pending[i - 1].first)
            throw std::runtime_error("duplicate migration version "
                                     + std::to_string(pending[i].first) + ": "
                                     + pending[i - 1].second.filename().string() + " and "
                                     + pending[i].second.filename().string());

    for (const auto& [v, p] : pending) {
        Transaction tx(db);
        db.exec(readFile(p));
        Statement st = db.prepare("INSERT INTO schema_version(version) VALUES (?);");
        st.bind(1, v).execute();
        tx.commit();
    }
}

}
