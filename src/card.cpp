#include "card.h"

#include <cctype>
#include <stdexcept>

namespace poker {

static const char RANKS[] = "23456789TJQKA";
static const char SUITS[] = "cdhs";

int parse_rank(char c) {
    c = char(std::toupper((unsigned char)c));
    for (int r = 0; r < 13; r++)
        if (RANKS[r] == c) return r;
    return -1;
}

char rank_char(int rank) { return RANKS[rank]; }

int parse_card(const std::string& s) {
    if (s.size() != 2) return -1;
    int rank = parse_rank(s[0]);
    int suit = -1;
    for (int su = 0; su < 4; su++)
        if (SUITS[su] == std::tolower((unsigned char)s[1])) suit = su;
    if (rank < 0 || suit < 0) return -1;
    return make_card(rank, suit);
}

std::string card_str(int card) { return {RANKS[rank_of(card)], SUITS[suit_of(card)]}; }

std::vector<int> parse_cards(const std::string& s) {
    std::string t;
    for (char ch : s)
        if (!std::isspace((unsigned char)ch) && ch != ',') t += ch;
    if (t.size() % 2) throw std::invalid_argument("can't parse cards: " + s);

    std::vector<int> cards;
    Mask seen = 0;
    for (size_t i = 0; i < t.size(); i += 2) {
        std::string one = t.substr(i, 2);
        int c = parse_card(one);
        if (c < 0) throw std::invalid_argument("bad card: " + one);
        if (seen & bit(c)) throw std::invalid_argument("card listed twice: " + one);
        seen |= bit(c);
        cards.push_back(c);
    }
    return cards;
}

Mask parse_mask(const std::string& s) {
    Mask m = 0;
    for (int c : parse_cards(s)) m |= bit(c);
    return m;
}

std::string mask_str(Mask m) {
    // high cards first, reads nicer
    std::string out;
    for (int r = 12; r >= 0; r--)
        for (int su = 3; su >= 0; su--)
            if (m & bit(make_card(r, su))) out += card_str(make_card(r, su));
    return out;
}

}  // namespace poker
