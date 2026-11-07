#include <stdexcept>

#include "card.h"
#include "check.h"

using namespace poker;

TEST(card_roundtrip) {
    for (int c = 0; c < NUM_CARDS; c++) CHECK(parse_card(card_str(c)) == c);
    CHECK(parse_card("As") == make_card(12, 3));
    CHECK(parse_card("2c") == 0);
    CHECK(parse_card("ts") == make_card(8, 3));
    CHECK(parse_card("1x") == -1);
    CHECK(parse_card("A") == -1);
}

TEST(card_lists) {
    CHECK(parse_cards("Ah Kd, 7c").size() == 3);
    CHECK(popcount(parse_mask("AhKd7c")) == 3);
    CHECK(mask_str(parse_mask("7cAhKd")) == "AhKd7c");

    bool threw = false;
    try {
        parse_cards("AhAh");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(card_subsets) {
    int n = 0;
    for_each_subset(parse_mask("AhKhQhJhTh"), 2, [&](Mask m) { n += popcount(m) == 2; });
    CHECK(n == 10);
}
