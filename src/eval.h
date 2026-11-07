#pragma once

#include <cstdint>

#include "card.h"

namespace poker {

enum Category {
    HIGH_CARD,
    PAIR,
    TWO_PAIR,
    TRIPS,
    STRAIGHT,
    FLUSH,
    FULL_HOUSE,
    QUADS,
    STRAIGHT_FLUSH,
};

// Best 5 card hand out of 5..7 cards. Bigger is better, equal means chop.
// Layout: category in the top bits, then up to two rank fields, then a kicker bitmask.
uint32_t evaluate(Mask cards);

inline Category category(uint32_t score) { return Category(score >> 26); }
const char* category_name(Category c);

// where a score sits among the 7462 distinct 5 card hands (1 = 75432 offsuit, 7462 = royal)
int hand_class(uint32_t score);
int num_hand_classes();

}  // namespace poker
