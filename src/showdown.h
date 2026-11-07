#pragma once

#include <array>
#include <vector>

#include "combos.h"

namespace poker {

// River showdown between two ranges in O(n) instead of O(n^2).
//
// Sort the live hands by strength. Walking up from the bottom we keep a running
// total S of opponent weight and C[c] = the part of S that touches card c. For a
// hand {a, b} the weight it beats is S - C[a] - C[b] (inclusion-exclusion; the
// both-cards term is the hand itself, which is never strictly weaker). Ties are
// handled by finishing a whole strength group before adding it to the sums.
// Same thing walking down gives the weight that beats us.
struct RiverShowdown {
    explicit RiverShowdown(Mask board);

    Mask board;
    std::vector<int> order;                       // live combos, weakest first
    std::array<uint32_t, NUM_COMBOS> strength{};  // 0 for combos touching the board

    // out[h] = payoff * (opp weight we beat - opp weight that beats us)
    void values(const double* opp, double payoff, double* out) const;
    // out[h] = opp weight that doesn't share a card with h (for fold payoffs)
    void live_weight(const double* opp, double* out) const;
};

}  // namespace poker
