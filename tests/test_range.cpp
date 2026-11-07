#include "check.h"
#include "equity.h"

using namespace poker;

TEST(range_parse) {
    CHECK(Range::parse("AA").live() == 6);
    CHECK(Range::parse("AKs").live() == 4);
    CHECK(Range::parse("AKo").live() == 12);
    CHECK(Range::parse("AK").live() == 16);
    CHECK(Range::parse("QQ+").live() == 18);
    CHECK(Range::parse("ATs+").live() == 16);
    CHECK(Range::parse("22+").live() == 78);
    CHECK(Range::parse("AsKs").live() == 1);
    CHECK(Range::parse("random").live() == 1326);
    CHECK(Range::parse("QQ+, AKs, AsKd").live() == 23);

    Range half = Range::parse("AA:0.5");
    CHECK_NEAR(half.total(), 3.0, 1e-9);

    int t = Range::parse("top10").live();
    CHECK(t >= 120 && t <= 150);
}

TEST(range_blockers) {
    Range r = Range::parse("AA");
    r.remove(parse_mask("As"));
    CHECK(r.live() == 3);  // card removal: only 3 aces left -> 3 combos
    r.remove(parse_mask("AhKd2c"));
    CHECK(r.live() == 1);
}

TEST(range_sample) {
    std::mt19937_64 rng(1);
    Range r = Range::parse("AsKs,QdQc:3");
    int qq = 0;
    for (int i = 0; i < 4000; i++) qq += r.sample(rng) == combo_index(parse_mask("QdQc"));
    CHECK_NEAR(qq / 4000.0, 0.75, 0.03);
}

TEST(equity_outs) {
    // classic numbers: 9 out flush draw ~35% by the river, ~19% on the turn
    CHECK_NEAR(hit_by_river(9), 0.3497, 1e-3);
    CHECK_NEAR(hit_next(9, 47), 0.1915, 1e-3);
    CHECK_NEAR(hit_by_river(15), 0.5414, 1e-3);

    // nut flush draw + two overs vs top pair: 9 hearts + 3 aces + 3 kings
    CHECK(count_outs(parse_mask("AhKh"), parse_mask("QsJs"), parse_mask("Qh7h2c")) == 15);
    CHECK(count_outs(parse_mask("QsJs"), parse_mask("AhKh"), parse_mask("Qh7h2c")) == 0);
}

TEST(equity_exact_and_range) {
    double aa_kk = equity_exact(parse_mask("AsAh"), parse_mask("KdKc"), 0);
    CHECK(aa_kk > 0.81 && aa_kk < 0.83);

    // river, no cards to come
    CHECK_NEAR(equity_exact(parse_mask("AhKh"), parse_mask("QsJs"), parse_mask("Qh7h2c9h3d")), 1.0, 1e-12);

    std::mt19937_64 rng(7);
    double vs_random = equity_vs_range(parse_mask("AsAh"), Range::uniform(), 0, 40000, rng);
    CHECK_NEAR(vs_random, 0.852, 0.01);

    // turn is exact: compare against summing hand vs hand
    Mask hero = parse_mask("AhKh"), board = parse_mask("Qh7h2c9s");
    Range villain = Range::parse("QsJs,7c7d");
    double e = equity_vs_range(hero, villain, board, 0, rng);
    double manual = (equity_exact(hero, parse_mask("QsJs"), board) + equity_exact(hero, parse_mask("7c7d"), board)) / 2;
    CHECK_NEAR(e, manual, 1e-9);
}
