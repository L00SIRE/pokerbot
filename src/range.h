#pragma once

#include <array>
#include <random>
#include <string>

#include "combos.h"

namespace poker {

// A range is just a weight per combo (r[k] in the notes). Not normalised to sum 1,
// everything that uses it divides by the live total anyway.
struct Range {
    std::array<double, NUM_COMBOS> w{};

    static Range uniform();
    static Range top(double frac);  // best `frac` of all hands by preflop equity
    // comma separated: "QQ+, AKs, ATs+, KQ, top20, AsKs:0.5, random"
    static Range parse(const std::string& text);

    // card removal: anything sharing a card with `dead` can't be held
    void remove(Mask dead);
    double total() const;
    int live() const;      // combos with weight > 0
    void normalize();      // biggest weight -> 1, keeps numbers sane after many updates
    int sample(std::mt19937_64& rng) const;  // weighted pick, -1 if empty
};

}  // namespace poker
