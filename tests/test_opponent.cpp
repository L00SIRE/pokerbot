#include <random>

#include "check.h"
#include "opponent.h"

using namespace poker;

TEST(opponent_beta) {
    BetaStat s(0.5, 2);
    CHECK_NEAR(s.mean(), 0.5, 1e-12);
    for (int i = 0; i < 8; i++) s.add(true);
    CHECK_NEAR(s.mean(), 9.0 / 10, 1e-12);
    CHECK(s.n == 8);
    BetaStat wide(0.5, 2), narrow(0.5, 200);
    CHECK(wide.sd() > narrow.sd());
}

TEST(opponent_profile_leaks) {
    Profile p;
    CHECK_NEAR(p.divergence(), 0.0, 1e-12);
    CHECK(!p.maniac());

    // a nit that folds to every river bet
    for (int i = 0; i < 40; i++) p[FOLD_BET_RIVER].add(true);
    for (int i = 0; i < 40; i++) p[VPIP].add(i % 4 == 0);
    auto leaks = p.leaks(2);
    CHECK(leaks.size() == 2);
    CHECK(leaks[0].stat == FOLD_BET_RIVER);
    CHECK(leaks[0].observed > 0.8);
    CHECK(p.divergence() > 10);
}

TEST(opponent_maniac_detect) {
    Profile p;
    int shoves = 0;
    while (!p.maniac() && shoves < 50) p[SHOVE].add(true), shoves++;
    CHECK(shoves <= 6);  // shoving every hand gets flagged within a few hands
    Profile q;
    for (int i = 0; i < 100; i++) q[SHOVE].add(i % 20 == 0);  // 5%, normal
    CHECK(!q.maniac());
}

TEST(opponent_archetypes) {
    ArchetypeModel m;
    double h0 = m.entropy();
    for (int b = 0; b < NUM_SIZES; b++) CHECK(m.info_gain(b) >= 0);
    for (int i = 0; i < 6; i++) m.observe(2, CALL_R);
    CHECK(m.posterior()[STATION] > 0.6);
    CHECK(m.entropy() < h0);

    ArchetypeModel f;
    for (int i = 0; i < 8; i++) f.observe(3, FOLD_R);
    CHECK(f.posterior()[NIT] > 0.8);
    CHECK(f.predict(3)[FOLD_R] > 0.7);
}

TEST(opponent_bot_detector) {
    std::mt19937_64 rng(1);
    BotDetector bot, human;
    std::lognormal_distribution<double> think(std::log(4e6), 0.9);  // ~4s, wide
    for (int i = 0; i < 60; i++) {
        bot.add_latency(30 + rng() % 5);
        bot.add_bet_size(i % 2 ? 0.5 : 1.0);
        human.add_latency(think(rng));
        human.add_bet_size(0.3 + (rng() % 100) / 100.0);
    }
    CHECK(bot.score() > 0.8);
    CHECK(human.score() < 0.3);
}
