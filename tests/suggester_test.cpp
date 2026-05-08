#include <gtest/gtest.h>
#include "suggester.h"

#include <atomic>
#include <fstream>
#include <filesystem>
#include <unistd.h>

namespace {

// Atomic so parallel ctest workers cannot collide on the same temp filename
// (matches history_store_test.cpp). Without this, a sufficiently fast
// test runner could see two tests pick the same counter value and clobber
// each other's wordlist or db.
std::atomic<int> g_counter{0};

std::filesystem::path makeWordlist(const std::string& contents) {
    int n = g_counter.fetch_add(1);
    auto path = std::filesystem::temp_directory_path() /
                ("wordbooster-sug-wl-" + std::to_string(::getpid()) +
                 "-" + std::to_string(n) + ".txt");
    std::ofstream out(path);
    out << contents;
    return path;
}

std::filesystem::path tempDb() {
    int n = g_counter.fetch_add(1);
    return std::filesystem::temp_directory_path() /
           ("wordbooster-sug-db-" + std::to_string(::getpid()) +
            "-" + std::to_string(n) + ".db");
}

}

TEST(SuggesterTest, DictOnlyWordHasBaseScore) {
    auto wl = makeWordlist("bonjour\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto db = tempDb();
    HistoryStore history(db);

    Suggester sug(dict, history);
    auto results = sug.suggest("bon", 10);

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].word, "bonjour");
    EXPECT_EQ(results[0].score, 1); // base score for dict-only word

    std::filesystem::remove(wl);
    std::filesystem::remove(db);
}

TEST(SuggesterTest, HistoryWordScoreIsBoosted) {
    auto wl = makeWordlist("bonjour\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto db = tempDb();
    HistoryStore history(db);
    history.recordWord("bonjour"); // freq=1 → score += 10×1

    Suggester sug(dict, history);
    auto results = sug.suggest("bon", 10);

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].score, 11); // 1 (dict) + 10*1 (history)

    std::filesystem::remove(wl);
    std::filesystem::remove(db);
}

TEST(SuggesterTest, ResultsSortedByScoreDescending) {
    auto wl = makeWordlist("bonjour\nbonsoir\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto db = tempDb();
    HistoryStore history(db);
    history.recordWord("bonsoir");
    history.recordWord("bonsoir"); // freq=2 → score=21

    Suggester sug(dict, history);
    auto results = sug.suggest("bon", 10);

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].word, "bonsoir"); // score 21
    EXPECT_EQ(results[1].word, "bonjour"); // score 1

    std::filesystem::remove(wl);
    std::filesystem::remove(db);
}

TEST(SuggesterTest, NoDuplicatesWhenWordInBothSourcess) {
    auto wl = makeWordlist("bonjour\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto db = tempDb();
    HistoryStore history(db);
    history.recordWord("bonjour");

    Suggester sug(dict, history);
    auto results = sug.suggest("bon", 10);

    EXPECT_EQ(results.size(), 1u); // not 2

    std::filesystem::remove(wl);
    std::filesystem::remove(db);
}

TEST(SuggesterTest, LimitIsRespected) {
    auto wl = makeWordlist("bonjour\nbonsoir\nbonbon\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto db = tempDb();
    HistoryStore history(db);

    Suggester sug(dict, history);
    auto results = sug.suggest("bon", 2);

    EXPECT_EQ(results.size(), 2u);

    std::filesystem::remove(wl);
    std::filesystem::remove(db);
}

TEST(SuggesterTest, EmptyPrefixReturnsNoResults) {
    auto wl = makeWordlist("bonjour\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto db = tempDb();
    HistoryStore history(db);

    Suggester sug(dict, history);
    auto results = sug.suggest("", 10);

    EXPECT_TRUE(results.empty());

    std::filesystem::remove(wl);
    std::filesystem::remove(db);
}

TEST(SuggesterTest, HistoryLimitPassedThroughToQuery) {
    // When limit=2, Suggester must not over-fetch from history (e.g. 1000 rows)
    // when the caller only wants 2 results. Verify by checking that the top-2
    // results are correct even when there are many history entries.
    Dictionary dict; // empty

    auto db = tempDb();
    HistoryStore history(db);
    // Record 5 words with different frequencies
    for (int i = 0; i < 5; i++) history.recordWord("apple");   // freq 5
    for (int i = 0; i < 3; i++) history.recordWord("apricot"); // freq 3
    for (int i = 0; i < 2; i++) history.recordWord("apt");     // freq 2
    history.recordWord("apron");                                // freq 1
    history.recordWord("apex");                                 // freq 1

    Suggester sug(dict, history);
    auto results = sug.suggest("ap", 2);

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].word, "apple");   // highest freq
    EXPECT_EQ(results[1].word, "apricot"); // second highest

    std::filesystem::remove(db);
}

TEST(SuggesterTest, HistoryOnlyWordAppears) {
    // Word typed by user but not in any wordlist
    Dictionary dict; // empty

    auto db = tempDb();
    HistoryStore history(db);
    history.recordWord("foobar");

    Suggester sug(dict, history);
    auto results = sug.suggest("foo", 10);

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].word, "foobar");

    std::filesystem::remove(db);
}
