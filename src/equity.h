#pragma once

#include <random>

#include "range.h"

namespace poker {

// chance of hitting one of `outs` clean outs on the next card
inline double hit_next(int outs, int unseen) { return double(outs) / unseen; }

// from the flop with two cards to come: 1 - (47-k)/47 * (46-k)/46
inline double hit_by_river(int outs, int unseen = 47) {
    return 1.0 - double(unseen - outs) / unseen * double(unseen - 1 - outs) / (unseen - 1);
}

// cards that put hero ahead of villain on the next street (0 if hero is already ahead)
int count_outs(Mask hero, Mask villain, Mask board);

// exact, enumerates every remaining board. preflop is 1.7M boards so ~50ms
double equity_exact(Mask hero, Mask villain, Mask board);

// hero vs a weighted range. exact on the turn/river, monte carlo before that
double equity_vs_range(Mask hero, const Range& villain, Mask board, int samples,
                       std::mt19937_64& rng);

}  // namespace poker
