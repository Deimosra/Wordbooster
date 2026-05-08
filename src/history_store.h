#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct WordFrequency {
    std::string word;
    int frequency;
};

class HistoryStore {
public:
    explicit HistoryStore(const std::filesystem::path& dbPath);
    ~HistoryStore();

    void recordWord(const std::string& word);
    int frequencyOf(const std::string& word) const;
    std::vector<WordFrequency> topByFrequency(const std::string& prefix, int limit = 10) const;

private:
    struct Impl;
    Impl* impl_;
};
