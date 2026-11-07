#include "buckets.h"
#include "check.h"

using namespace poker;

TEST(buckets_emd) {
    Histogram a{}, b{}, c{};
    a[0] = 1;              // always 0% equity
    b[HIST_BINS - 1] = 1;  // always ~100%
    c[0] = 0.5, c[HIST_BINS - 1] = 0.5;
    CHECK(emd(a, a) == 0);
    CHECK_NEAR(emd(a, b), emd(b, a), 1e-12);
    CHECK_NEAR(emd(a, b), (HIST_BINS - 1.0) / HIST_BINS, 1e-12);
    CHECK_NEAR(emd(a, c) + emd(c, b), emd(a, b), 1e-12);  // c sits halfway
}

TEST(buckets_draw_vs_pair) {
    Mask flop = parse_mask("Jh8h3c");
    auto hist = flop_histograms(flop);
    const Histogram& draw = hist[combo_index(parse_mask("Ah5h"))];  // nut flush draw
    const Histogram& pair = hist[combo_index(parse_mask("8s7d"))];  // middle pair
    double mass = 0;
    for (double x : draw) mass += x;
    CHECK_NEAR(mass, 1.0, 1e-9);
    CHECK(hist[combo_index(parse_mask("Jh2c"))][0] == 0);  // touches the flop, empty

    // the draw lives at the ends (hits or misses), the pair sits in the middle
    double draw_ends = 0, pair_ends = 0;
    for (int i = 0; i < 10; i++) draw_ends += draw[i] + draw[HIST_BINS - 1 - i];
    for (int i = 0; i < 10; i++) pair_ends += pair[i] + pair[HIST_BINS - 1 - i];
    CHECK(draw_ends > pair_ends);
}
