#include "bots.h"
#include "bully.h"
#include "check.h"

using namespace poker;

// duplicate match: every deal is played twice with the seats swapped
static double bb_per_100(Bot& hero, Bot& villain, int pairs, uint64_t seed) {
    Table t;
    std::mt19937_64 rng(seed);
    long won = 0;
    for (int i = 0; i < pairs; i++) {
        auto c = Table::deal(rng);
        HandResult a = t.play(hero, villain, i % 2, c);
        HandResult b = t.play(villain, hero, i % 2, c);
        CHECK(a.net[0] + a.net[1] == 0 && b.net[0] + b.net[1] == 0);
        won += a.net[0] + b.net[1];
    }
    return 100.0 * won / (2.0 * pairs) / t.big_blind();
}

TEST(bully_reads_a_maniac) {
    BullyConfig cfg;
    cfg.use_solver = false;
    BullyBot bully(cfg);
    auto maniac = make_bot("maniac");
    double r = bb_per_100(bully, *maniac, 150, 11);
    CHECK(bully.profile().maniac());
    CHECK(bully.profile().get(SHOVE) > 0.8);
    CHECK(r > 0);
}

TEST(bully_beats_station_and_random) {
    BullyConfig cfg;
    cfg.use_solver = false;
    BullyBot a(cfg), b(cfg);
    auto station = make_bot("station"), random = make_bot("random", 2);
    CHECK(bb_per_100(a, *station, 150, 12) > 0);
    CHECK(bb_per_100(b, *random, 150, 13) > 0);
}

TEST(bully_river_solver_plays) {
    BullyBot bully;  // solver on
    auto station = make_bot("station");
    bb_per_100(bully, *station, 15, 14);
    CHECK(bully.river_solves() > 0);
}
