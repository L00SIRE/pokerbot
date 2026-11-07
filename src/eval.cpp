#include "eval.h"

#include <algorithm>
#include <array>
#include <vector>

namespace poker {
namespace {

// straight high card for every 13 bit rank set, -1 if no straight
std::array<int8_t, 8192> build_straights() {
    std::array<int8_t, 8192> t{};
    for (int m = 0; m < 8192; m++) {
        int r = (m << 1) | ((m >> 12) & 1);  // ace also plays low (bit 0)
        int run = r & (r >> 1) & (r >> 2) & (r >> 3) & (r >> 4);
        t[m] = run ? int8_t(31 - __builtin_clz(run) + 3) : int8_t(-1);
    }
    return t;
}

const std::array<int8_t, 8192> STRAIGHT_HIGH = build_straights();

inline int top(uint32_t m) { return 31 - __builtin_clz(m); }

inline uint32_t keep_top(uint32_t m, int n) {
    while (__builtin_popcount(m) > n) m &= m - 1;
    return m;
}

inline uint32_t pack(Category c, uint32_t v) { return (uint32_t(c) << 26) | v; }

}  // namespace

uint32_t evaluate(Mask cards) {
    const uint32_t c = cards & 0x1FFF;
    const uint32_t d = (cards >> 13) & 0x1FFF;
    const uint32_t h = (cards >> 26) & 0x1FFF;
    const uint32_t s = (cards >> 39) & 0x1FFF;

    // with 7 cards a flush rules out quads and full houses, so check it first
    for (uint32_t suit : {c, d, h, s}) {
        if (__builtin_popcount(suit) >= 5) {
            int sf = STRAIGHT_HIGH[suit];
            if (sf >= 0) return pack(STRAIGHT_FLUSH, sf << 17);
            return pack(FLUSH, keep_top(suit, 5));
        }
    }

    const uint32_t ranks = c | d | h | s;
    const uint32_t quads = c & d & h & s;
    const uint32_t trips = ((c & d) & (h | s)) | ((h & s) & (c | d));  // 3+ of a rank
    const uint32_t pairs = (c & d) | (c & h) | (c & s) | (d & h) | (d & s) | (h & s);  // 2+

    if (quads) {
        int q = top(quads);
        return pack(QUADS, (q << 17) | keep_top(ranks & ~(1u << q), 1));
    }
    if (trips) {
        int t = top(trips);
        uint32_t rest = pairs & ~(1u << t);  // a second set counts as the pair
        if (rest) return pack(FULL_HOUSE, (t << 17) | (top(rest) << 13));
    }
    if (int st = STRAIGHT_HIGH[ranks]; st >= 0) return pack(STRAIGHT, st << 17);
    if (trips) {
        int t = top(trips);
        return pack(TRIPS, (t << 17) | keep_top(ranks & ~(1u << t), 2));
    }
    if (pairs) {
        int p1 = top(pairs);
        uint32_t others = pairs & ~(1u << p1);
        if (others) {
            int p2 = top(others);
            uint32_t kick = keep_top(ranks & ~(1u << p1) & ~(1u << p2), 1);
            return pack(TWO_PAIR, (p1 << 17) | (p2 << 13) | kick);
        }
        return pack(PAIR, (p1 << 17) | keep_top(ranks & ~(1u << p1), 3));
    }
    return pack(HIGH_CARD, keep_top(ranks, 5));
}

const char* category_name(Category c) {
    static const char* names[] = {"High card", "One pair", "Two pair",
                                  "Three of a kind", "Straight", "Flush",
                                  "Full house", "Four of a kind", "Straight flush"};
    return names[c];
}

static const std::vector<uint32_t>& five_card_scores() {
    static const std::vector<uint32_t> scores = [] {
        std::vector<uint32_t> v;
        v.reserve(2598960);
        for_each_subset(FULL_DECK, 5, [&](Mask m) { v.push_back(evaluate(m)); });
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
        return v;
    }();
    return scores;
}

int hand_class(uint32_t score) {
    const auto& v = five_card_scores();
    return int(std::lower_bound(v.begin(), v.end(), score) - v.begin()) + 1;
}

int num_hand_classes() { return int(five_card_scores().size()); }

}  // namespace poker
