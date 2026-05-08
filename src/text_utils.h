#pragma once

#include <string>

namespace text_utils {

// UTF-8 aware lowercase. Handles Latin accents (É → é, À → à, etc.)
// Returns empty string on invalid UTF-8 input rather than throwing.
std::string toLower(const std::string& input);

// Casing pattern observed in user input. Used to re-apply user's casing
// onto a lowercased dictionary suggestion at commit time.
enum class Casing {
    Lower,        // "bonjour"
    InitialUpper, // "Bonjour"
    AllUpper,     // "BONJOUR"
};

// Detect casing pattern from a UTF-8 string. Empty string → Lower.
Casing detectCasing(const std::string& input);

// Apply a casing pattern to a lowercased UTF-8 string.
// Used to render dictionary suggestions matching user's intent.
std::string applyCasing(const std::string& lowerWord, Casing casing);

}
