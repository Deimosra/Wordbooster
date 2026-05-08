#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Dictionary loads one or more wordlists (hunspell .dic format or plain
// one-word-per-line) and offers a bounded prefix lookup.
//
// Words are stored lowercased + deduplicated + sorted so lookup() can run a
// binary search and stop after `limit` matches. This keeps per-keystroke
// latency stable on 50k+-word corpora.
class Dictionary {
public:
    // Loads a wordlist file. Hunspell-style affix flags (`/S`, `/AB`) and
    // morphological tabs are stripped. The first line is skipped if it is
    // a pure integer (hunspell word-count header).
    // Missing/unreadable files are silently ignored.
    void loadWordlist(const std::filesystem::path& path);

    // Returns up to `limit` words starting with `prefix` (case-insensitive).
    // `limit <= 0` means unbounded (used in tests). Returns an empty vector
    // for an empty prefix.
    std::vector<std::string> lookup(const std::string& prefix, int limit = 50) const;

private:
    std::vector<std::string> words_; // sorted, lowercased, deduplicated
};
