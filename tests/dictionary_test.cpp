#include <gtest/gtest.h>
#include "dictionary.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <unistd.h>

namespace {

// Atomic so parallel ctest workers don't collide on the same temp filename.
std::atomic<int> g_counter{0};

std::filesystem::path makeWordlist(const std::string& contents) {
    int n = g_counter.fetch_add(1);
    auto path = std::filesystem::temp_directory_path() /
                ("wordbooster-test-" + std::to_string(::getpid()) +
                 "-" + std::to_string(n) + ".txt");
    std::ofstream out(path);
    out << contents;
    return path;
}

bool contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

}

TEST(DictionaryTest, ReturnsWordsMatchingPrefix) {
    auto wl = makeWordlist("bonjour\nbonsoir\nchat\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto results = dict.lookup("bon");

    EXPECT_EQ(results.size(), 2u);
    EXPECT_TRUE(contains(results, "bonjour"));
    EXPECT_TRUE(contains(results, "bonsoir"));
    std::filesystem::remove(wl);
}

TEST(DictionaryTest, ExcludesWordsNotMatchingPrefix) {
    auto wl = makeWordlist("bonjour\nchat\ncheval\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto results = dict.lookup("bon");

    EXPECT_FALSE(contains(results, "chat"));
    EXPECT_FALSE(contains(results, "cheval"));
    std::filesystem::remove(wl);
}

TEST(DictionaryTest, EmptyPrefixReturnsNoResults) {
    auto wl = makeWordlist("bonjour\nchat\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto results = dict.lookup("");

    EXPECT_TRUE(results.empty());
    std::filesystem::remove(wl);
}

TEST(DictionaryTest, MissingWordlistFileDoesNotCrash) {
    Dictionary dict;
    dict.loadWordlist("/tmp/nonexistent-wordlist-wordbooster.txt");

    auto results = dict.lookup("bon");

    EXPECT_TRUE(results.empty());
}

TEST(DictionaryTest, LookupIsCaseInsensitive) {
    auto wl = makeWordlist("bonjour\nBonsoir\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto results = dict.lookup("bon");

    EXPECT_EQ(results.size(), 2u);
    EXPECT_TRUE(contains(results, "bonjour"));
    EXPECT_TRUE(contains(results, "bonsoir")); // stored lowercased
    std::filesystem::remove(wl);
}

TEST(DictionaryTest, LookupMatchesAccentedWordsCaseInsensitively) {
    // Word stored with capital accent; lookup with lowercase accent prefix.
    auto wl = makeWordlist("Étoile\nétage\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto results = dict.lookup("ét");

    EXPECT_EQ(results.size(), 2u);
    EXPECT_TRUE(contains(results, "étoile"));
    EXPECT_TRUE(contains(results, "étage"));
    std::filesystem::remove(wl);
}

TEST(DictionaryTest, LookupWithUppercasedAccentedPrefixMatches) {
    auto wl = makeWordlist("éléphant\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto results = dict.lookup("ÉL");

    EXPECT_EQ(results.size(), 1u);
    EXPECT_TRUE(contains(results, "éléphant"));
    std::filesystem::remove(wl);
}

TEST(DictionaryTest, DuplicateWordsAcrossWordlistsDeduplicated) {
    // "cat" appears in both wordlists — should appear once in lookup.
    auto fr = makeWordlist("chat\ncar\n");
    auto en = makeWordlist("car\ncat\n");
    Dictionary dict;
    dict.loadWordlist(fr);
    dict.loadWordlist(en);

    auto results = dict.lookup("car");

    EXPECT_EQ(results.size(), 1u);
    std::filesystem::remove(fr);
    std::filesystem::remove(en);
}

TEST(DictionaryTest, MultipleWordlistsMerged) {
    auto fr = makeWordlist("bonjour\nchat\n");
    auto en = makeWordlist("hello\ncat\n");
    Dictionary dict;
    dict.loadWordlist(fr);
    dict.loadWordlist(en);

    auto results = dict.lookup("c");

    EXPECT_TRUE(contains(results, "chat"));
    EXPECT_TRUE(contains(results, "cat"));
    std::filesystem::remove(fr);
    std::filesystem::remove(en);
}

// --- Hunspell .dic format support ---

TEST(DictionaryTest, HunspellHeaderCountIsSkipped) {
    // First line of a hunspell .dic file is the (approximate) word count.
    // Loading "3\nbonjour\nchat\nchien\n" must not insert "3" as a word.
    auto wl = makeWordlist("3\nbonjour\nchat\nchien\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto threes = dict.lookup("3", 10);
    EXPECT_TRUE(threes.empty()) << "hunspell header count should not be a word";

    auto bons = dict.lookup("bon", 10);
    EXPECT_TRUE(contains(bons, "bonjour"));
    std::filesystem::remove(wl);
}

TEST(DictionaryTest, HunspellAffixFlagsAreStripped) {
    // "bonjour/S" in hunspell means "bonjour, with affix flag S".
    // The loaded word must be "bonjour" only.
    auto wl = makeWordlist("2\nbonjour/S\nchat/AB\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto bons = dict.lookup("bon", 10);
    ASSERT_EQ(bons.size(), 1u);
    EXPECT_EQ(bons[0], "bonjour");

    auto chats = dict.lookup("chat", 10);
    ASSERT_EQ(chats.size(), 1u);
    EXPECT_EQ(chats[0], "chat");
    std::filesystem::remove(wl);
}

TEST(DictionaryTest, HunspellMorphologicalDataAfterTabIsStripped) {
    // hunspell allows "word\tpo:nom" annotations; everything after the tab
    // is metadata and must not appear in the word.
    auto wl = makeWordlist("1\nvoiture\tpo:nom\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto results = dict.lookup("voi", 10);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], "voiture");
    std::filesystem::remove(wl);
}

TEST(DictionaryTest, HunspellApostropheWordPreserved) {
    // French words with internal apostrophe must survive parsing.
    auto wl = makeWordlist("1\naujourd'hui/S\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto results = dict.lookup("aujourd", 10);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], "aujourd'hui");
    std::filesystem::remove(wl);
}

TEST(DictionaryTest, HunspellHyphenWordPreserved) {
    // French compound words with internal hyphen must survive parsing.
    auto wl = makeWordlist("1\npeut-être\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto results = dict.lookup("peut", 10);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], "peut-être");
    std::filesystem::remove(wl);
}

TEST(DictionaryTest, LookupRespectsLimit) {
    // lookup must stop after `limit` matches rather than scan the whole corpus.
    auto wl = makeWordlist("aa\nab\nac\nad\nae\naf\nag\nah\nai\naj\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto top3 = dict.lookup("a", 3);
    EXPECT_EQ(top3.size(), 3u);
    std::filesystem::remove(wl);
}

TEST(DictionaryTest, LookupPrefixOutOfRangeReturnsEmpty) {
    auto wl = makeWordlist("bonjour\nchat\n");
    Dictionary dict;
    dict.loadWordlist(wl);

    auto results = dict.lookup("zz", 10);
    EXPECT_TRUE(results.empty());
    std::filesystem::remove(wl);
}
