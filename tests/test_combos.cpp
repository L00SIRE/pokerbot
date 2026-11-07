#include "check.h"
#include "combos.h"

using namespace poker;

TEST(combos_basic) {
    CHECK(combos().size() == 1326);
    Mask all = 0;
    for (auto& c : combos()) {
        CHECK(c.c1 < c.c2);
        CHECK(popcount(c.mask) == 2);
        all |= c.mask;
    }
    CHECK(all == FULL_DECK);
    CHECK(combo_index(parse_mask("AsKd")) == combo_index(parse_card("Kd"), parse_card("As")));
}

TEST(combos_preflop_classes) {
    int per_class[NUM_CLASSES] = {};
    for (int i = 0; i < NUM_COMBOS; i++) per_class[combo_class(i)]++;

    int pairs = 0, suited = 0, offsuit = 0, total = 0;
    for (int cls = 0; cls < NUM_CLASSES; cls++) {
        CHECK(per_class[cls] == class_size(cls));
        CHECK(class_from_name(class_name(cls)) == cls);
        total += per_class[cls];
        int s = class_size(cls);
        pairs += s == 6, suited += s == 4, offsuit += s == 12;
    }
    CHECK(total == 1326);
    CHECK(pairs == 13 && suited == 78 && offsuit == 78);

    auto as = parse_cards("AsKs");
    CHECK(class_name(preflop_class(as[0], as[1])) == "AKs");
    auto ao = parse_cards("Kd As");
    CHECK(class_name(preflop_class(ao[0], ao[1])) == "AKo");
    CHECK(class_from_name("KAs") == class_from_name("AKs"));
    CHECK(class_from_name("AA") == 12 * 13 + 12);
    CHECK(class_from_name("AAs") == -1);
}

TEST(combos_preflop_equity) {
    // well known numbers vs a random hand: AA 85.2, KK 82.4, 72o 34.6
    CHECK_NEAR(preflop_equity(class_from_name("AA")), 0.852, 0.01);
    CHECK_NEAR(preflop_equity(class_from_name("KK")), 0.824, 0.01);
    CHECK_NEAR(preflop_equity(class_from_name("72o")), 0.346, 0.01);
    CHECK(preflop_equity(class_from_name("AKs")) > preflop_equity(class_from_name("AKo")));

    CHECK(preflop_order()[0] == class_from_name("AA"));
    CHECK(preflop_top_frac(class_from_name("AA")) < 0.003);
    // 72o isn't actually the worst vs a random hand, 32o / 42o are
    CHECK(preflop_top_frac(class_from_name("72o")) > 0.9);
    CHECK(preflop_top_frac(class_from_name("32o")) > 0.97);
}
