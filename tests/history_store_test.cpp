#include <gtest/gtest.h>
#include "history_store.h"

#include <atomic>
#include <filesystem>
#include <stdexcept>
#include <unistd.h>

namespace {

// Generate a unique DB path per call. Tests run in parallel under ctest,
// so a fixed path causes SQLite "disk I/O error" via concurrent access on
// the same file. Use pid + counter + test name fragment for isolation.
std::atomic<int> g_counter{0};

std::filesystem::path tempDb() {
    int n = g_counter.fetch_add(1);
    return std::filesystem::temp_directory_path() /
           ("wordbooster-history-" + std::to_string(::getpid()) +
            "-" + std::to_string(n) + ".db");
}

}

TEST(HistoryStoreTest, RecordingWordGivesFrequencyOne) {
    auto path = tempDb();
    {
        HistoryStore store(path);
        store.recordWord("bonjour");
        EXPECT_EQ(store.frequencyOf("bonjour"), 1);
    }
    std::filesystem::remove(path);
}

TEST(HistoryStoreTest, RecordingSameWordMultipleTimesAccumulatesFrequency) {
    auto path = tempDb();
    {
        HistoryStore store(path);
        store.recordWord("chat");
        store.recordWord("chat");
        store.recordWord("chat");
        EXPECT_EQ(store.frequencyOf("chat"), 3);
    }
    std::filesystem::remove(path);
}

TEST(HistoryStoreTest, UnknownWordHasFrequencyZero) {
    auto path = tempDb();
    {
        HistoryStore store(path);
        EXPECT_EQ(store.frequencyOf("inconnu"), 0);
    }
    std::filesystem::remove(path);
}

TEST(HistoryStoreTest, TopByFrequencyReturnsSortedByFrequencyDescending) {
    auto path = tempDb();
    {
        HistoryStore store(path);
        store.recordWord("chat");
        store.recordWord("chaton");
        store.recordWord("chaton");
        store.recordWord("chaton");
        store.recordWord("chat");

        auto results = store.topByFrequency("cha");

        ASSERT_GE(results.size(), 2u);
        EXPECT_EQ(results[0].word, "chaton"); // freq 3 first
        EXPECT_EQ(results[1].word, "chat");   // freq 2 second
    }
    std::filesystem::remove(path);
}

TEST(HistoryStoreTest, TopByFrequencyFiltersOnPrefix) {
    auto path = tempDb();
    {
        HistoryStore store(path);
        store.recordWord("chat");
        store.recordWord("bonjour");

        auto results = store.topByFrequency("bon");

        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0].word, "bonjour");
    }
    std::filesystem::remove(path);
}

TEST(HistoryStoreTest, DataPersistsAcrossInstances) {
    auto path = tempDb();
    {
        HistoryStore store(path);
        store.recordWord("persistant");
        store.recordWord("persistant");
    }
    {
        HistoryStore store(path);
        EXPECT_EQ(store.frequencyOf("persistant"), 2);
    }
    std::filesystem::remove(path);
}

TEST(HistoryStoreTest, TopByFrequencyEscapesPercentInPrefix) {
    // Wildcard "%" in user prefix must be treated literally,
    // not as SQL LIKE wildcard.
    auto path = tempDb();
    {
        HistoryStore store(path);
        store.recordWord("100%natural");
        store.recordWord("hundred");

        auto results = store.topByFrequency("100%");

        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0].word, "100%natural");
    }
    std::filesystem::remove(path);
}

TEST(HistoryStoreTest, TopByFrequencyEscapesUnderscoreInPrefix) {
    // "_" is a single-char wildcard in LIKE; must be escaped.
    auto path = tempDb();
    {
        HistoryStore store(path);
        store.recordWord("snake_case");
        store.recordWord("snakeXcase"); // would match if _ was wildcard

        auto results = store.topByFrequency("snake_");

        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0].word, "snake_case");
    }
    std::filesystem::remove(path);
}

TEST(HistoryStoreTest, RecordWordThrowsOnReadOnlyDb) {
    // Open DB normally then reopen in a read-only context to provoke a
    // write failure that surfaces as an exception (not a silent no-op).
    //
    // We isolate this test in a dedicated sub-directory so that toggling
    // permissions on the parent does not race with other tests writing to
    // the shared system temp dir.
    auto isolatedDir = std::filesystem::temp_directory_path() /
        ("wordbooster-history-ro-" + std::to_string(::getpid()) +
         "-" + std::to_string(g_counter.fetch_add(1)));
    std::filesystem::create_directories(isolatedDir);
    auto path = isolatedDir / "history.db";

    {
        HistoryStore store(path);
        store.recordWord("seed");
    }
    // Make file read-only
    std::filesystem::permissions(path,
        std::filesystem::perms::owner_read,
        std::filesystem::perm_options::replace);

    // Make the isolated parent dir read-only so SQLite cannot create a
    // -journal file. Other tests use sibling dirs, so they are unaffected.
    std::filesystem::permissions(isolatedDir,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);

    try {
        HistoryStore store(path);
        EXPECT_THROW(store.recordWord("blocked"), std::runtime_error);
    } catch (const std::runtime_error&) {
        // Acceptable: open itself may throw on stricter sqlite versions.
        SUCCEED();
    }

    // Restore perms so cleanup works
    std::filesystem::permissions(isolatedDir,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
    std::filesystem::permissions(path,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);
    std::filesystem::remove_all(isolatedDir);
}

TEST(HistoryStoreTest, WalJournalModeIsActive) {
    // ARCHITECTURE.md guarantees WAL for crash resilience.
    // Verify the PRAGMA is actually set when the DB is opened.
    auto path = tempDb();
    {
        HistoryStore store(path);
        // WAL creates a -wal sidecar file after the first write.
        store.recordWord("wal_probe");
        auto wal = std::filesystem::path(path.string() + "-wal");
        EXPECT_TRUE(std::filesystem::exists(wal))
            << "WAL sidecar file missing — journal_mode=WAL not active";
    }
    std::filesystem::remove(path);
    std::filesystem::remove(std::filesystem::path(path.string() + "-wal"));
    std::filesystem::remove(std::filesystem::path(path.string() + "-shm"));
}

TEST(HistoryStoreTest, CreatesDbDirectoryIfMissing) {
    auto dir = std::filesystem::temp_directory_path() /
               ("wordbooster-newdir-" + std::to_string(::getpid()));
    auto path = dir / "history.db";

    {
        HistoryStore store(path);
        store.recordWord("test");
        EXPECT_EQ(store.frequencyOf("test"), 1);
    }

    std::filesystem::remove_all(dir);
}
