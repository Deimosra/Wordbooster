#include "typing_session.h"

#include "text_utils.h"

void TypingSession::append(char c) {
    raw_.push_back(c);
}

void TypingSession::backspace() {
    if (!raw_.empty()) raw_.pop_back();
}

void TypingSession::clear() {
    raw_.clear();
}

std::string TypingSession::lookupPrefix() const {
    return text_utils::toLower(raw_);
}

std::string TypingSession::renderSuggestion(const std::string& lowerSuggestion) const {
    auto casing = text_utils::detectCasing(raw_);
    return text_utils::applyCasing(lowerSuggestion, casing);
}
