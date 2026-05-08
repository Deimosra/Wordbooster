#include "auto_cap.h"

namespace auto_cap {

    bool isSentenceEnd(char c) {
        return c == '.' || c == '!' || c == '?' || c == '\n';
    }
}
