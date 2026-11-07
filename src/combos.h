#pragma once

#include <array>
#include <string>

#include "card.h"

namespace poker {

constexpr int NUM_COMBOS = 1326;  // 52 choose 2
constexpr int NUM_CLASSES = 169;  // 13 pairs + 78 suited + 78 offsuit

struct Combo {
    uint8_t c1, c2;  // c1 < c2
    Mask mask;
};

const std::array<Combo, NUM_COMBOS>& combos();
int combo_index(int a, int b);  // order of a, b doesn't matter
int combo_index(Mask two_cards);

// Preflop classes live on the usual 13x13 grid: pairs on the diagonal,
// suited hands at [hi][lo], offsuit at [lo][hi]. class id = row * 13 + col.
int preflop_class(int a, int b);
int combo_class(int combo);
std::string class_name(int cls);               // "AKs", "T9o", "77"
int class_from_name(const std::string& name);  // -1 if it doesn't parse
int class_size(int cls);                       // 6 pairs, 4 suited, 12 offsuit

// all-in equity vs one random hand. monte carlo, computed once and cached
double preflop_equity(int cls);
// classes strongest first (by the equity above)
const std::array<int, NUM_CLASSES>& preflop_order();
// where a class sits in the full 1326 deck, 0 = best hand, 1 = worst (midpoint of its combos)
double preflop_top_frac(int cls);

}  // namespace poker
