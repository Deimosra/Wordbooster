#include "text_utils.h"

#include <unicode/unistr.h>
#include <unicode/uchar.h>

namespace text_utils {

std::string toLower(const std::string& input) {
    icu::UnicodeString us = icu::UnicodeString::fromUTF8(input);
    if (us.isBogus()) return {};
    us.toLower();
    std::string out;
    us.toUTF8String(out);
    return out;
}

Casing detectCasing(const std::string& input) {
    icu::UnicodeString us = icu::UnicodeString::fromUTF8(input);
    if (us.isEmpty()) return Casing::Lower;

    bool firstUpper = false;
    bool anyLowerAfterFirst = false;
    bool anyUpperAfterFirst = false;
    bool sawAnyLetter = false;

    int32_t idx = 0;
    int32_t letterIndex = 0; // counts only letter codepoints, not digits/symbols
    while (idx < us.length()) {
        UChar32 c = us.char32At(idx);
        idx += U16_LENGTH(c);

        if (!u_isalpha(c)) continue; // skip non-letters without incrementing

        sawAnyLetter = true;
        bool isUpper = (u_toupper(c) == c) && (u_tolower(c) != c);
        bool isLower = (u_tolower(c) == c) && (u_toupper(c) != c);

        if (letterIndex == 0) {
            firstUpper = isUpper;
        } else {
            if (isLower) anyLowerAfterFirst = true;
            if (isUpper) anyUpperAfterFirst = true;
        }
        letterIndex++;
    }

    if (!sawAnyLetter) return Casing::Lower;
    if (firstUpper && !anyLowerAfterFirst && anyUpperAfterFirst) {
        return Casing::AllUpper;
    }
    // Single uppercase letter alone → AllUpper (matches "A" or "I").
    if (firstUpper && !anyLowerAfterFirst && !anyUpperAfterFirst) {
        return Casing::AllUpper;
    }
    if (firstUpper) return Casing::InitialUpper;
    return Casing::Lower;
}

std::string applyCasing(const std::string& lowerWord, Casing casing) {
    if (lowerWord.empty()) return lowerWord;
    icu::UnicodeString us = icu::UnicodeString::fromUTF8(lowerWord);

    switch (casing) {
        case Casing::Lower:
            break;
        case Casing::AllUpper:
            us.toUpper();
            break;
        case Casing::InitialUpper: {
            if (us.length() > 0) {
                UChar32 first = us.char32At(0);
                int32_t firstLen = U16_LENGTH(first);
                icu::UnicodeString head(first);
                head.toUpper();
                us.replace(0, firstLen, head);
            }
            break;
        }
    }

    std::string out;
    us.toUTF8String(out);
    return out;
}

}
