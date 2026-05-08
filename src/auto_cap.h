#pragma once

namespace auto_cap {

// Returns true when `c` is a character that ends a sentence, meaning the
// next typable word should be automatically capitalised.
//
// Sentence-ending characters: . ! ? and \n (including Return).
// All other characters (space, comma, letters, etc.) return false.
//
// This function works on raw ASCII char values as received from the
// keyboard event system — no Unicode / UTF-8 needed.
bool isSentenceEnd(char c);

}
