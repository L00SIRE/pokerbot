#include "check.h"
#include "eval.h"

using namespace poker;

static uint32_t ev(const char* s) { return evaluate(parse_mask(s)); }

TEST(eval_categories) {
    CHECK(category(ev("AsKsQsJsTs")) == STRAIGHT_FLUSH);
    CHECK(category(ev("5h4h3h2hAh")) == STRAIGHT_FLUSH);
    CHECK(category(ev("AhAdAcAs2c")) == QUADS);
    CHECK(category(ev("KhKdKc2s2c")) == FULL_HOUSE);
    CHECK(category(ev("Kh9h7h3h2h")) == FLUSH);
    CHECK(category(ev("5d4h3c2sAh")) == STRAIGHT);
    CHECK(category(ev("7h7d7c2sAh")) == TRIPS);
    CHECK(category(ev("7h7dKcKsAh")) == TWO_PAIR);
    CHECK(category(ev("7h7dKc2sAh")) == PAIR);
    CHECK(category(ev("7h9dKc2sAh")) == HIGH_CARD);
}

TEST(eval_ordering) {
    CHECK(ev("AsKsQsJsTs") > ev("KsQsJsTs9s"));
    CHECK(ev("6d5h4c3s2h") > ev("5d4h3c2sAh"));  // wheel is the smallest straight
    CHECK(ev("AhAd2c2s3c") < ev("AhAd3c3s2c"));
    CHECK(ev("KhKdQcQsAc") > ev("KhKdQcQsJc"));  // kicker
    CHECK(ev("AhKhQh9h7h") > ev("AhKhQh9h6h"));
    CHECK(ev("AhAdKcKsQc") == ev("AsAcKhKdQd"));  // chop
    CHECK(ev("2h2d2c3s3d") > ev("AhKhQh9h7h"));  // any boat beats any flush
}

TEST(eval_seven_cards) {
    CHECK(ev("AhAdKcKs7d2c3h") == ev("AsAcKcKs7d2c3h"));
    // three pair -> top two pair plus the best leftover card
    CHECK(category(ev("AhAdKcKsQdQc2h")) == TWO_PAIR);
    CHECK(ev("AhAdKcKsQdQc2h") == ev("AhAdKcKsQd3c2h"));
    // two sets -> full house
    CHECK(category(ev("9h9d9c5s5d5c2h")) == FULL_HOUSE);
    CHECK(category(ev("9h8h7h6h5d4c2s")) == STRAIGHT);
    CHECK(category(ev("9h8h7h6h5h4c2s")) == STRAIGHT_FLUSH);
    // six to a flush, best five count
    CHECK(ev("AhKh9h7h5h3h2c") == ev("AhKh9h7h5hTc2c"));
}

TEST(eval_five_card_distribution) {
    // known 5 card counts out of 2,598,960
    const long expected[9] = {1302540, 1098240, 123552, 54912, 10200, 5108, 3744, 624, 40};
    long got[9] = {};
    for_each_subset(FULL_DECK, 5, [&](Mask m) { got[category(evaluate(m))]++; });
    for (int i = 0; i < 9; i++) CHECK(got[i] == expected[i]);

    CHECK(num_hand_classes() == 7462);
    CHECK(hand_class(ev("AsKsQsJsTs")) == 7462);
    CHECK(hand_class(ev("7s5d4c3h2h")) == 1);
}
