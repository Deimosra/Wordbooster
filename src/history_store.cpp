#include "history_store.h"

#include <sqlite3.h>
#include <stdexcept>
#include <filesystem>
#include <string>

namespace {

// RAII guard for sqlite3_stmt — guarantees finalize even on throw.
struct StmtGuard {
    sqlite3_stmt* stmt = nullptr;
    ~StmtGuard() { if (stmt) sqlite3_finalize(stmt); }
};

[[noreturn]] void throwSqlite(sqlite3* db, const std::string& context) {
    std::string msg = context + ": ";
    msg += sqlite3_errmsg(db);
    throw std::runtime_error(msg);
}

void prepareOrThrow(sqlite3* db, const char* sql, sqlite3_stmt** stmt) {
    if (sqlite3_prepare_v2(db, sql, -1, stmt, nullptr) != SQLITE_OK) {
        throwSqlite(db, "sqlite3_prepare_v2 failed");
    }
}

// Read a text column safely. sqlite3_column_text() can return NULL on OOM
// even for non-NULL stored values — guard against constructing std::string
// from nullptr (UB / crash).
std::string columnText(sqlite3_stmt* stmt, int col) {
    const unsigned char* raw = sqlite3_column_text(stmt, col);
    if (!raw) return {};
    return reinterpret_cast<const char*>(raw);
}

}

struct HistoryStore::Impl {
    sqlite3* db = nullptr;

    void exec(const char* sql) {
        char* err = nullptr;
        if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = err ? err : "unknown error";
            sqlite3_free(err);
            throw std::runtime_error("SQLite error: " + msg);
        }
    }
};

HistoryStore::HistoryStore(const std::filesystem::path& dbPath)
    : impl_(new Impl()) {

    // Create parent directory if needed
    auto parent = dbPath.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        // Ignore ec — sqlite3_open will surface a clear error if dir creation
        // really mattered (e.g. read-only parent).
    }

    if (sqlite3_open(dbPath.c_str(), &impl_->db) != SQLITE_OK) {
        std::string msg = "Cannot open history db: " + dbPath.string();
        if (impl_->db) {
            msg += " (";
            msg += sqlite3_errmsg(impl_->db);
            msg += ")";
            sqlite3_close(impl_->db);
            impl_->db = nullptr;
        }
        delete impl_;
        impl_ = nullptr;
        throw std::runtime_error(msg);
    }

    // WAL mode: writes don't block readers, more resilient to crashes than
    // the default DELETE journal. 
    impl_->exec("PRAGMA journal_mode=WAL");

    impl_->exec(R"(
        CREATE TABLE IF NOT EXISTS history (
            word      TEXT PRIMARY KEY,
            frequency INTEGER NOT NULL DEFAULT 0,
            last_used INTEGER NOT NULL DEFAULT (strftime('%s','now'))
        )
    )");
}

HistoryStore::~HistoryStore() {
    if (impl_) {
        if (impl_->db) sqlite3_close(impl_->db);
        delete impl_;
    }
}

void HistoryStore::recordWord(const std::string& word) {
    const char* sql = R"(
        INSERT INTO history(word, frequency, last_used)
        VALUES(?, 1, strftime('%s','now'))
        ON CONFLICT(word) DO UPDATE SET
            frequency = frequency + 1,
            last_used = strftime('%s','now')
    )";
    StmtGuard g;
    prepareOrThrow(impl_->db, sql, &g.stmt);
    // SQLITE_TRANSIENT: SQLite copies the string immediately so it is safe
    // even if the caller's std::string is destroyed before sqlite3_step().
    // SQLITE_STATIC would be UB if the string is a temporary.
    if (sqlite3_bind_text(g.stmt, 1, word.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        throwSqlite(impl_->db, "bind word for recordWord");
    }
    int rc = sqlite3_step(g.stmt);
    if (rc != SQLITE_DONE) {
        throwSqlite(impl_->db, "recordWord step failed");
    }
}

int HistoryStore::frequencyOf(const std::string& word) const {
    const char* sql = "SELECT frequency FROM history WHERE word = ?";
    StmtGuard g;
    prepareOrThrow(impl_->db, sql, &g.stmt);
    // SQLITE_TRANSIENT: safe copy, avoids SQLITE_STATIC UB on temporaries.
    if (sqlite3_bind_text(g.stmt, 1, word.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        throwSqlite(impl_->db, "bind word for frequencyOf");
    }

    int freq = 0;
    int rc = sqlite3_step(g.stmt);
    if (rc == SQLITE_ROW) {
        freq = sqlite3_column_int(g.stmt, 0);
    } else if (rc != SQLITE_DONE) {
        throwSqlite(impl_->db, "frequencyOf step failed");
    }
    return freq;
}

std::vector<WordFrequency> HistoryStore::topByFrequency(
    const std::string& prefix, int limit) const {

    const char* sql = R"(
        SELECT word, frequency FROM history
        WHERE word LIKE ? ESCAPE '\'
        ORDER BY frequency DESC
        LIMIT ?
    )";
    StmtGuard g;
    prepareOrThrow(impl_->db, sql, &g.stmt);

    // Escape LIKE special chars (% _ \) in user-provided prefix so they are
    // matched literally rather than interpreted as wildcards.
    std::string escaped;
    escaped.reserve(prefix.size());
    for (char c : prefix) {
        if (c == '\\' || c == '%' || c == '_') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    std::string pattern = escaped + "%";
    if (sqlite3_bind_text(g.stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        throwSqlite(impl_->db, "bind pattern for topByFrequency");
    }
    if (sqlite3_bind_int(g.stmt, 2, limit) != SQLITE_OK) {
        throwSqlite(impl_->db, "bind limit for topByFrequency");
    }

    std::vector<WordFrequency> results;
    int rc;
    while ((rc = sqlite3_step(g.stmt)) == SQLITE_ROW) {
        results.push_back({
            columnText(g.stmt, 0),
            sqlite3_column_int(g.stmt, 1)
        });
    }
    if (rc != SQLITE_DONE) {
        throwSqlite(impl_->db, "topByFrequency step failed");
    }
    return results;
}
