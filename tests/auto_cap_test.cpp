#include <gtest/gtest.h>
#include "auto_cap.h"

using auto_cap::isSentenceEnd;

TEST(AutoCapTest, PeriodEndsSentence) {
    EXPECT_TRUE(isSentenceEnd('.'));
}

TEST(AutoCapTest, ExclamationEndsSentence) {
    EXPECT_TRUE(isSentenceEnd('!'));
}

TEST(AutoCapTest, QuestionEndsSentence) {
    EXPECT_TRUE(isSentenceEnd('?'));
}

TEST(AutoCapTest, NewlineEndsSentence) {
    EXPECT_TRUE(isSentenceEnd('\n'));
}

TEST(AutoCapTest, SpaceDoesNotEndSentence) {
    EXPECT_FALSE(isSentenceEnd(' '));
}

TEST(AutoCapTest, CommaDoesNotEndSentence) {
    EXPECT_FALSE(isSentenceEnd(','));
}

TEST(AutoCapTest, SemicolonDoesNotEndSentence) {
    EXPECT_FALSE(isSentenceEnd(';'));
}

TEST(AutoCapTest, ColonDoesNotEndSentence) {
    EXPECT_FALSE(isSentenceEnd(':'));
}

TEST(AutoCapTest, LettersDoNotEndSentence) {
    EXPECT_FALSE(isSentenceEnd('a'));
    EXPECT_FALSE(isSentenceEnd('Z'));
}
