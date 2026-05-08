#include "dictionary.h"

#include "text_utils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_set>

namespace {

    // Parse one line from a hunspell .dic file (or a plain wordlist).
    //
    // Hunspell .dic format (see https://www.systutorials.com/docs/linux/man/4-hunspell/):
    //   - Line 1 may be an integer (approximate word count). Skip it.
    //   - Each subsequent line: WORD[/AFFIX_FLAGS][\ttab-comment]
    //     e.g. "bonjour/S", "chat/AB", "voiture\tpo:nom"
    //   - Comments start with "#" (rare).
    //   - Trailing CR (CRLF files) must be stripped.
    //
    // Plain wordlists (one word per line) are a degenerate case of the same format
    // without affix flags or counts, so the same parser works.
    //
    // Returns the bare word, or an empty string if the line should be ignored.
    std::string parseHunspellLine(std::string line, bool isFirstLine) {
        if (line.empty()) return {};
        if (line.back() == '\r') line.pop_back();
        if (line.front() == '#') return {};
    
        // First line: pure-integer line is the hunspell word count, skip it.
        if (isFirstLine) {
            bool allDigits = std::all_of(line.begin(), line.end(),
                [](unsigned char c) { return std::isdigit(c); });
            if (allDigits) return {};
        }
    
        // Strip affix flags after '/' (e.g. "bonjour/S" -> "bonjour").
        auto slash = line.find('/');
        if (slash != std::string::npos) line.resize(slash);
    
        // Strip morphological data after a tab (e.g. "voiture\tpo:nom").
        auto tab = line.find('\t');
        if (tab != std::string::npos) line.resize(tab);
    
        while (!line.empty() &&
               (line.back() == ' ' || line.back() == '\r' || line.back() == '\t')) {
            line.pop_back();
        }
        return line;
    }

}

void Dictionary::loadWordlist(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in.is_open()) return;

    // Track already-stored words to deduplicate across multiple wordlists
    // (e.g. "color" present in both fr and en wordlists should appear once).
    std::unordered_set<std::string> seen(words_.begin(), words_.end());

    std::string raw;
    bool firstLine = true;
    while (std::getline(in, raw)) {
        auto word = parseHunspellLine(std::move(raw), firstLine);
        firstLine = false;
        if (word.empty()) continue;

        auto lower = text_utils::toLower(word);
        if (lower.empty()) continue; // invalid UTF-8 skipped silently

        if (seen.insert(lower).second) {
            words_.push_back(std::move(lower));
        }
    }

    // Keep words_ sorted so lookup() can binary-search for the prefix range
    // instead of scanning all 50k+ entries on every keystroke.
    std::sort(words_.begin(), words_.end());
}

std::vector<std::string> Dictionary::lookup(const std::string& prefix, int limit) const {
    if (prefix.empty()) return {};

    auto lprefix = text_utils::toLower(prefix);
    if (lprefix.empty()) return {};

    // Binary search the contiguous range of matching words. Both endpoints
    // run in O(log n); enumeration is bounded by `limit`.
    auto begin = std::lower_bound(words_.begin(), words_.end(), lprefix);

    std::vector<std::string> results;
    results.reserve(limit > 0 ? static_cast<size_t>(limit) : 0);

    for (auto it = begin; it != words_.end(); ++it) {
        if (it->size() < lprefix.size() ||
            it->compare(0, lprefix.size(), lprefix) != 0) {
            break; // left the prefix range
        }
        results.push_back(*it);
        if (limit > 0 && static_cast<int>(results.size()) >= limit) break;
    }
    return results;
}
