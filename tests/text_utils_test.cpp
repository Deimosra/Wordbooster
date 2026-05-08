#include <gtest/gtest.h>
#include "text_utils.h"

using text_utils::toLower;
using text_utils::detectCasing;
using text_utils::applyCasing;
using text_utils::Casing;

TEST(TextUtilsToLowerTest, AsciiLowercased) {
    EXPECT_EQ(toLower("Bonjour"), "bonjour");
    EXPECT_EQ(toLower("HELLO"), "hello");
}

TEST(TextUtilsToLowerTest, EmptyStringStaysEmpty) {
    EXPECT_EQ(toLower(""), "");
}

TEST(TextUtilsToLowerTest, FrenchAccentsLowercased) {
    EXPECT_EQ(toLower("Étoile"), "étoile");
    EXPECT_EQ(toLower("ÀÇÉÈÊÎÔÙÛ"), "àçéèêîôùû");
}

TEST(TextUtilsToLowerTest, AlreadyLowerIsIdempotent) {
    EXPECT_EQ(toLower("éléphant"), "éléphant");
}

TEST(TextUtilsCasingTest, AllLowerDetectedAsLower) {
    EXPECT_EQ(detectCasing("bonjour"), Casing::Lower);
}

TEST(TextUtilsCasingTest, InitialUpperDetected) {
    EXPECT_EQ(detectCasing("Bonjour"), Casing::InitialUpper);
}

TEST(TextUtilsCasingTest, AllUpperDetected) {
    EXPECT_EQ(detectCasing("BONJOUR"), Casing::AllUpper);
}

TEST(TextUtilsCasingTest, SingleUpperLetterIsAllUpper) {
    // "A" alone — could be either; treat as AllUpper so "A" stays "A".
    EXPECT_EQ(detectCasing("A"), Casing::AllUpper);
}

TEST(TextUtilsCasingTest, EmptyIsLower) {
    EXPECT_EQ(detectCasing(""), Casing::Lower);
}

TEST(TextUtilsCasingTest, FrenchAccentInitialUpper) {
    EXPECT_EQ(detectCasing("Étoile"), Casing::InitialUpper);
}

TEST(TextUtilsCasingTest, LeadingDigitIgnoredForCasingDetection) {
    // "1Bonjour" — first codepoint is a digit, first *letter* is 'B' (upper).
    // Should detect InitialUpper, not Lower.
    EXPECT_EQ(detectCasing("1Bonjour"), Casing::InitialUpper);
}

TEST(TextUtilsCasingTest, LeadingSymbolIgnoredForCasingDetection) {
    // "@HELLO" — first codepoint is '@', first letter is 'H' (upper).
    EXPECT_EQ(detectCasing("@HELLO"), Casing::AllUpper);
}

TEST(TextUtilsCasingTest, OnlyNonLettersIsLower) {
    // "123" — no letters at all → Lower.
    EXPECT_EQ(detectCasing("123"), Casing::Lower);
}

TEST(TextUtilsApplyCasingTest, LowerLeavesWordUnchanged) {
    EXPECT_EQ(applyCasing("bonjour", Casing::Lower), "bonjour");
}

TEST(TextUtilsApplyCasingTest, InitialUpperCapitalizesFirstLetter) {
    EXPECT_EQ(applyCasing("bonjour", Casing::InitialUpper), "Bonjour");
}

TEST(TextUtilsApplyCasingTest, AllUpperUppercasesEverything) {
    EXPECT_EQ(applyCasing("bonjour", Casing::AllUpper), "BONJOUR");
}

TEST(TextUtilsApplyCasingTest, InitialUpperOnAccent) {
    EXPECT_EQ(applyCasing("étoile", Casing::InitialUpper), "Étoile");
}

TEST(TextUtilsApplyCasingTest, AllUpperOnAccent) {
    EXPECT_EQ(applyCasing("étoile", Casing::AllUpper), "ÉTOILE");
}

TEST(TextUtilsApplyCasingTest, EmptyStringStaysEmpty) {
    EXPECT_EQ(applyCasing("", Casing::InitialUpper), "");
}
