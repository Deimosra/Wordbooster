#include "suggester.h"

#include <algorithm>
#include <unordered_map>
#include <cstdlib>

static constexpr int DICT_BASE_SCORE  = 1;
static constexpr int HISTORY_MULTIPLIER = 10;

Suggester::Suggester(const Dictionary& dict, const HistoryStore& history)
    : dict_(dict), history_(history) {}

std::vector<Suggestion> Suggester::suggest(const std::string& prefix, int limit) const {
    if (prefix.empty() || limit <= 0) return {};

    // Collect scores per word, merging dict + history sources.
    std::unordered_map<std::string, int> scores;

    // Dict contribution. Bound the fetch: history-boosted words can lift any
    // dict word above a dict-only candidate, but only up to `dictFetchLimit`
    // alphabetically-first matches contribute. We over-fetch (×8) to give the
    // merge enough room — keeps lookup O(limit) rather than O(corpus).
    const int dictFetchLimit = std::max(limit * 8, 50);
    for (const auto& w : dict_.lookup(prefix, dictFetchLimit)) {
        scores[w] += DICT_BASE_SCORE;
    }

    // History contribution. Fetch only what we need; SQLite already orders
    // by frequency DESC so the top-K of history is sufficient.
    const int historyFetchLimit = std::max(limit * 4, 50);
    for (const auto& wf : history_.topByFrequency(prefix, historyFetchLimit)) {
        scores[wf.word] += wf.frequency * HISTORY_MULTIPLIER;
    }

    // Build vector and partial-sort the top `limit` entries.
    // std::sort would cost O(n log n); std::partial_sort is O(n log limit).
    // Saves time when limit=1 (the typical case) on large dict fetches.
    std::vector<Suggestion> results;
    results.reserve(scores.size());
    for (auto& [word, score] : scores) {
        results.push_back({word, score});
    }

    auto cmp = [](const Suggestion& a, const Suggestion& b) {
        return a.score > b.score;
    };
    if (static_cast<int>(results.size()) > limit) {
        std::partial_sort(results.begin(), results.begin() + limit,
                          results.end(), cmp);
        results.resize(limit);
    } else {
        std::sort(results.begin(), results.end(), cmp);
    }

    return results;
}
