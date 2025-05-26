#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <string>
#include <vector> // Required if ConversationTurn uses std::vector or if it's a common include

namespace Common { // Encapsulate in a namespace

struct ConversationTurn {
    std::string role;
    std::string text;
};

// typedef std::vector<ConversationTurn> ConversationHistory; // Optional: if used frequently

} // namespace Common

#endif // COMMON_TYPES_H
