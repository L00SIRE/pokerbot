#include "showdown.h"

#include <algorithm>

#include "eval.h"

namespace poker {

RiverShowdown::RiverShowdown(Mask b) : board(b) {
    const auto& cs = combos();
    for (int k = 0; k < NUM_COMBOS; k++) {
        if (cs[k].mask & board) continue;
        strength[k] = evaluate(cs[k].mask | board);
        order.push_back(k);
    }
    std::sort(order.begin(), order.end(), [&](int x, int y) { return strength[x] < strength[y]; });
}

void RiverShowdown::values(const double* opp, double payoff, double* out) const {
    const auto& cs = combos();
    const size_t n = order.size();
    std::fill(out, out + NUM_COMBOS, 0.0);

    double card[NUM_CARDS];
    double total = 0;

    // weakest -> strongest: what we beat
    std::fill(card, card + NUM_CARDS, 0.0);
    for (size_t i = 0; i < n;) {
        size_t j = i;
        while (j < n && strength[order[j]] == strength[order[i]]) j++;
        for (size_t k = i; k < j; k++) {
            const Combo& c = cs[order[k]];
            out[order[k]] = total - card[c.c1] - card[c.c2];
        }
        for (size_t k = i; k < j; k++) {
            int h = order[k];
            total += opp[h];
            card[cs[h].c1] += opp[h];
            card[cs[h].c2] += opp[h];
        }
        i = j;
    }

    // strongest -> weakest: what beats us
    std::fill(card, card + NUM_CARDS, 0.0);
    total = 0;
    for (size_t i = n; i > 0;) {
        size_t j = i;
        while (j > 0 && strength[order[j - 1]] == strength[order[i - 1]]) j--;
        for (size_t k = j; k < i; k++) {
            const Combo& c = cs[order[k]];
            out[order[k]] -= total - card[c.c1] - card[c.c2];
        }
        for (size_t k = j; k < i; k++) {
            int h = order[k];
            total += opp[h];
            card[cs[h].c1] += opp[h];
            card[cs[h].c2] += opp[h];
        }
        i = j;
    }

    for (int h : order) out[h] *= payoff;
}

void RiverShowdown::live_weight(const double* opp, double* out) const {
    const auto& cs = combos();
    std::fill(out, out + NUM_COMBOS, 0.0);
    double card[NUM_CARDS] = {};
    double total = 0;
    for (int h : order) {
        total += opp[h];
        card[cs[h].c1] += opp[h];
        card[cs[h].c2] += opp[h];
    }
    // the hand itself got subtracted twice (once per card), add it back once
    for (int h : order) out[h] = total - card[cs[h].c1] - card[cs[h].c2] + opp[h];
}

}  // namespace poker
