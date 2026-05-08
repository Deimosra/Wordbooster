#include <gtest/gtest.h>
#include "typing_session.h"

TEST(TypingSessionTest, NewSessionIsEmpty) {
    TypingSession s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.rawText(), "");
}

TEST(TypingSessionTest, AppendStoresCharacters) {
    TypingSession s;
    s.append('b');
    s.append('o');
    s.append('n');

    EXPECT_FALSE(s.empty());
    EXPECT_EQ(s.rawText(), "bon");
}

TEST(TypingSessionTest, AppendPreservesUppercase) {
    // typing "Bonjour" should accumulate the uppercase B,
    // not flush the buffer and lose context.
    TypingSession s;
    s.append('B');
    s.append('o');
    s.append('n');

    EXPECT_EQ(s.rawText(), "Bon");
}

TEST(TypingSessionTest, BackspaceRemovesLastChar) {
    TypingSession s;
    s.append('b');
    s.append('o');
    s.backspace();

    EXPECT_EQ(s.rawText(), "b");
}

TEST(TypingSessionTest, BackspaceOnEmptyIsNoOp) {
    TypingSession s;
    s.backspace();
    EXPECT_TRUE(s.empty());
}

TEST(TypingSessionTest, ClearResetsBuffer) {
    TypingSession s;
    s.append('h');
    s.append('i');
    s.clear();
    EXPECT_TRUE(s.empty());
}

TEST(TypingSessionTest, LookupPrefixIsLowercaseOfRaw) {
    TypingSession s;
    s.append('B');
    s.append('o');
    s.append('n');

    EXPECT_EQ(s.lookupPrefix(), "bon");
}

TEST(TypingSessionTest, LookupPrefixForAllCapsIsLower) {
    TypingSession s;
    s.append('B');
    s.append('O');
    s.append('N');

    EXPECT_EQ(s.lookupPrefix(), "bon");
}

TEST(TypingSessionTest, RenderSuggestionLowerByDefault) {
    TypingSession s;
    s.append('b');
    s.append('o');
    s.append('n');

    EXPECT_EQ(s.renderSuggestion("bonjour"), "bonjour");
}

TEST(TypingSessionTest, RenderSuggestionCapitalizesIfUserTypedInitialUpper) {
    // typing "Bon" + accept dictionary suggestion "bonjour"
    // should commit "Bonjour", not "bonjour".
    TypingSession s;
    s.append('B');
    s.append('o');
    s.append('n');

    EXPECT_EQ(s.renderSuggestion("bonjour"), "Bonjour");
}

TEST(TypingSessionTest, RenderSuggestionAllUpperIfUserTypedAllUpper) {
    TypingSession s;
    s.append('B');
    s.append('O');
    s.append('N');

    EXPECT_EQ(s.renderSuggestion("bonjour"), "BONJOUR");
}

TEST(TypingSessionTest, RenderSuggestionHandlesAccentedSuggestion) {
    TypingSession s;
    s.append('E'); // user typed E (no accent on keyboard yet)
    s.append('t');

    // Suggestion from dictionary is already lowercased with accent.
    EXPECT_EQ(s.renderSuggestion("étoile"), "Étoile");
}
