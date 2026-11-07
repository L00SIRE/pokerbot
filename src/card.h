#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace poker {

// A set of cards is a 64 bit mask. Card ids are suit-major (id = suit * 13 + rank)
// so every suit sits in its own 13 bit block, which is what the evaluator wants.
// rank 0 = deuce ... 12 = ace, suits are c d h s.
using Mask = uint64_t;

constexpr int NUM_CARDS = 52;
constexpr Mask FULL_DECK = (1ULL << NUM_CARDS) - 1;

constexpr int make_card(int rank, int suit) { return suit * 13 + rank; }
constexpr int rank_of(int card) { return card % 13; }
constexpr int suit_of(int card) { return card / 13; }
constexpr Mask bit(int card) { return 1ULL << card; }

inline int popcount(Mask m) { return __builtin_popcountll(m); }
inline int lowest_card(Mask m) { return __builtin_ctzll(m); }

int parse_rank(char c);                              // 'T' -> 8, -1 if not a rank
char rank_char(int rank);
int parse_card(const std::string& s);                // "Ah" -> id, -1 if it isn't a card
std::string card_str(int card);
std::vector<int> parse_cards(const std::string& s);  // "AhKd 7c", throws on junk / dupes
Mask parse_mask(const std::string& s);
std::string mask_str(Mask m);

// calls f(subset) for every k card subset of avail
template <class F>
void for_each_subset(Mask avail, int k, F&& f, Mask acc = 0) {
    if (k == 0) {
        f(acc);
        return;
    }
    while (popcount(avail) >= k) {
        Mask low = avail & -avail;
        avail ^= low;
        for_each_subset(avail, k - 1, f, acc | low);
    }
}

}  // namespace poker
