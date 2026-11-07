#include "equity.h"

#include <algorithm>
#include <vector>

#include "eval.h"

namespace poker {

static inline double result(uint32_t a, uint32_t b) { return a > b ? 1.0 : a == b ? 0.5 : 0.0; }

int count_outs(Mask hero, Mask villain, Mask board) {
    if (evaluate(hero | board) >= evaluate(villain | board)) return 0;
    Mask unseen = FULL_DECK & ~(hero | villain | board);
    int outs = 0;
    for (Mask m = unseen; m; m &= m - 1) {
        Mask c = m & -m;
        if (evaluate(hero | board | c) > evaluate(villain | board | c)) outs++;
    }
    return outs;
}

double equity_exact(Mask hero, Mask villain, Mask board) {
    Mask avail = FULL_DECK & ~(hero | villain | board);
    int need = 5 - popcount(board);
    double score = 0;
    long n = 0;
    for_each_subset(avail, need, [&](Mask rest) {
        Mask b = board | rest;
        score += result(evaluate(hero | b), evaluate(villain | b));
        n++;
    });
    return score / n;
}

double equity_vs_range(Mask hero, const Range& villain, Mask board, int samples,
                       std::mt19937_64& rng) {
    const auto& cs = combos();
    Range r = villain;
    r.remove(hero | board);
    if (r.total() <= 0) return 0.5;

    const int known = popcount(board);
    if (known >= 4) {
        // turn: loop over the river, river: just the one board. weight is w[k] per (river, k)
        double score = 0, weight = 0;
        Mask rivers = known == 5 ? 0 : FULL_DECK & ~(hero | board);
        auto run = [&](Mask b) {
            uint32_t h = evaluate(hero | b);
            for (int k = 0; k < NUM_COMBOS; k++) {
                if (r.w[k] <= 0 || (cs[k].mask & b)) continue;
                score += r.w[k] * result(h, evaluate(cs[k].mask | b));
                weight += r.w[k];
            }
        };
        if (known == 5) {
            run(board);
        } else {
            for (Mask m = rivers; m; m &= m - 1) run(board | (m & -m));
        }
        return weight > 0 ? score / weight : 0.5;
    }

    std::vector<double> cum(NUM_COMBOS);
    double t = 0;
    for (int k = 0; k < NUM_COMBOS; k++) cum[k] = t += r.w[k];

    std::uniform_real_distribution<double> u(0, t);
    double score = 0;
    for (int s = 0; s < samples; s++) {
        int k = int(std::upper_bound(cum.begin(), cum.end(), u(rng)) - cum.begin());
        k = std::min(k, NUM_COMBOS - 1);
        Mask v = cs[k].mask;
        Mask used = hero | board | v;
        Mask b = board;
        for (int i = known; i < 5; i++) {
            int c;
            do c = int(rng() % NUM_CARDS);
            while (used & bit(c));
            used |= bit(c);
            b |= bit(c);
        }
        score += result(evaluate(hero | b), evaluate(v | b));
    }
    return score / samples;
}

}  // namespace poker
