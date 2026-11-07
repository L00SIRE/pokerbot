#include <algorithm>
#include <random>
#include <vector>

#include "check.h"
#include "showdown.h"

using namespace poker;

static Mask random_board(std::mt19937_64& rng) {
    Mask b = 0;
    while (popcount(b) < 5) b |= bit(int(rng() % 52));
    return b;
}

TEST(showdown_matches_naive) {
    std::mt19937_64 rng(3);
    const auto& cs = combos();
    for (int trial = 0; trial < 5; trial++) {
        RiverShowdown sd(random_board(rng));
        std::vector<double> opp(NUM_COMBOS, 0.0);
        for (int k : sd.order) opp[k] = (rng() % 4 == 0) ? 0.0 : double(rng() % 1000) / 1000.0;

        std::vector<double> fast(NUM_COMBOS), live(NUM_COMBOS);
        sd.values(opp.data(), 2.5, fast.data());
        sd.live_weight(opp.data(), live.data());

        double worst = 0, worst_live = 0;
        for (int h : sd.order) {
            double slow = 0, slow_live = 0;
            for (int k : sd.order) {
                if (cs[h].mask & cs[k].mask) continue;
                slow_live += opp[k];
                if (sd.strength[h] > sd.strength[k]) slow += opp[k];
                else if (sd.strength[h] < sd.strength[k]) slow -= opp[k];
            }
            worst = std::max(worst, std::fabs(slow * 2.5 - fast[h]));
            worst_live = std::max(worst_live, std::fabs(slow_live - live[h]));
        }
        CHECK(worst < 1e-8);
        CHECK(worst_live < 1e-8);
    }
}

TEST(showdown_ties_are_zero) {
    // board is a royal flush, everyone chops
    RiverShowdown sd(parse_mask("AsKsQsJsTs"));
    std::vector<double> opp(NUM_COMBOS, 1.0), out(NUM_COMBOS);
    sd.values(opp.data(), 1.0, out.data());
    for (int h : sd.order) CHECK(out[h] == 0.0);
}
