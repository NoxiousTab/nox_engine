#pragma once
#include <string>

namespace eng {

struct NNUE {
    static bool load(const std::string& path);
    static bool isReady();
    static void setEnabled(bool on);
    static bool isEnabled();
    // Evaluate returns centipawns from side-to-move perspective when enabled and loaded.
    // If not ready, callers should fallback to classical eval.
    static int evaluate(const struct Board& b);
};

} // namespace eng
