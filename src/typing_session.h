#pragma once

#include "suggester.h"

#include <string>

// TypingSession owns the in-progress word being typed by the user.
//
// Responsibilities:
//  - accumulate characters as the user types (any casing);
//  - expose a lowercased prefix for the Suggester to query;
//  - re-apply the user's observed casing pattern (Lower / InitialUpper /
//    AllUpper) onto a chosen suggestion at commit time, so typing "Bon" +
//    accept yields "Bonjour", and typing "BON" yields "BONJOUR".
//
// The session is testable in isolation — no fcitx5 dependency.
class TypingSession {
public:
    // True when no characters have been typed yet.
    bool empty() const { return raw_.empty(); }

    // Append a character (preserving its case in the raw buffer).
    void append(char c);

    // Remove the last character. No-op when empty.
    void backspace();

    // Reset the session to empty.
    void clear();

    // The raw text as typed by the user, preserving casing.
    // This is what gets shown in the preedit area.
    const std::string& rawText() const { return raw_; }

    // Lowercased text used to query the Suggester.
    std::string lookupPrefix() const;

    // Apply the user's casing pattern (detected from rawText) onto a
    // lowercased suggestion. Used at commit time so the committed string
    // matches user intent (capitalization, all caps, etc.).
    std::string renderSuggestion(const std::string& lowerSuggestion) const;

private:
    std::string raw_;
};
