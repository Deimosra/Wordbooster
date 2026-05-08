#pragma once

#include "dictionary.h"
#include "history_store.h"

#include <string>
#include <vector>

struct Suggestion {
    std::string word;
    int score;
};

class Suggester {
public:
    Suggester(const Dictionary& dict, const HistoryStore& history);

    // Returns top candidates for prefix, sorted by score descending.
    std::vector<Suggestion> suggest(const std::string& prefix, int limit = 1) const;

private:
    const Dictionary& dict_;
    const HistoryStore& history_;
};
