#include "check.h"
#include "river_solver.h"

using namespace poker;

// Toy spot with a known answer. Board can't make straights or flushes.
// oop: 3 combos of AA (top set) + 16 combos of 43 (air). ip: KQ only, a pure bluff catcher.
// One pot sized bet, no raises. Theory says:
//   ip calls MDF = P / (P + B) = 50%
//   oop bets every set and bluffs so that bluffs / bets = B / (P + 2B) = 1/3, i.e. 1.5 of the 16 air combos
static RiverSpot toy_spot() {
    RiverSpot s;
    s.board = parse_mask("2s7d9cKhAh");
    s.pot = 100;
    s.stack = 1000;
    s.range[0] = Range::parse("AA,43");
    s.range[1] = Range::parse("KQ");
    s.bet_sizes = {1.0};
    s.raise_sizes = {};
    s.allin = false;
    s.max_raises = 1;
    return s;
}

static int child(const RiverSolver& sv, int node, Move m) {
    const Node& n = sv.tree()[node];
    for (int a = 0; a < n.num_actions(); a++)
        if (n.moves[a] == m) return a;
    return -1;
}

// average prob of action a at node over a range
static double freq(const RiverSolver& sv, int node, int a, const Range& r) {
    auto s = sv.strategy(node);
    double num = 0, den = 0;
    for (int h = 0; h < NUM_COMBOS; h++) {
        if (r.w[h] <= 0) continue;
        num += r.w[h] * s[a * NUM_COMBOS + h];
        den += r.w[h];
    }
    return num / den;
}

TEST(solver_tree_shape) {
    RiverSolver sv(toy_spot());
    const Node& root = sv.tree()[0];
    CHECK(root.num_actions() == 2);  // check, bet 100
    CHECK(sv.describe(0, 1) == "bet 100");
    const Node& facing = sv.tree()[root.kids[1]];
    CHECK(facing.player == 1);
    CHECK(facing.num_actions() == 2);  // fold, call (no raises left)
}

TEST(solver_toy_polar_spot) {
    RiverSpot spot = toy_spot();
    RiverSolver sv(spot);
    sv.iterate(2000);
    CHECK(sv.exploitability() < 0.2);

    const int bet = child(sv, 0, Move::BET);
    const int facing = sv.tree()[0].kids[bet];
    Range ip = spot.range[1], sets = Range::parse("AA"), air = Range::parse("43");
    ip.remove(spot.board), sets.remove(spot.board);

    CHECK_NEAR(freq(sv, facing, child(sv, facing, Move::CALL), ip), 0.5, 0.02);
    CHECK(freq(sv, 0, bet, sets) > 0.97);
    CHECK_NEAR(freq(sv, 0, bet, air), 1.5 / 16, 0.015);
}

TEST(solver_all_algos_toy) {
    for (Algo a : {Algo::CFR_PLUS, Algo::DCFR, Algo::PCFR_PLUS, Algo::MMD}) {
        RiverSolver sv(toy_spot(), a);
        sv.iterate(1000);
        double e = sv.exploitability();
        if (e > (a == Algo::MMD ? 1.0 : 0.1)) std::printf("    %s: %.4f\n", algo_name(a), e);
        CHECK(e < (a == Algo::MMD ? 1.0 : 0.1));
    }
}

// model of a nit: folds to any bet, checks back when checked to
static void model_always_folds(RiverSolver& sv, int who) {
    for (Node& n : sv.tree()) {
        if (n.type != Node::ACTION || n.player != who) continue;
        n.model.assign(n.kids.size() * NUM_COMBOS, 0.0);
        for (int a = 0; a < n.num_actions(); a++) {
            if (n.moves[a] != Move::FOLD && n.moves[a] != Move::CHECK) continue;
            for (int h = 0; h < NUM_COMBOS; h++) n.model[a * NUM_COMBOS + h] = 1.0;
        }
    }
}

// In the toy spot an extra bluff wins +100 vs the folder and loses -100 vs a caller
// (relative to checking), so it's only worth it once p > 0.5. Below that the nemesis
// just calls more, (1-p) * call = 50%, and the right answer is still nash.
TEST(solver_rnr) {
    RiverSpot spot = toy_spot();
    Range air = Range::parse("43");
    const double ps[4] = {0.0, 0.3, 0.7, 1.0};
    double bluff[4], given_up[4];
    for (int i = 0; i < 4; i++) {
        RiverSolver sv(spot);
        model_always_folds(sv, 1);
        sv.set_exploit(1, ps[i]);
        sv.iterate(1500);
        bluff[i] = freq(sv, 0, child(sv, 0, Move::BET), air);
        given_up[i] = sv.best_response_value(1);  // what a real counter takes back from us
    }
    CHECK_NEAR(bluff[0], 1.5 / 16, 0.015);  // p = 0 is plain nash
    CHECK_NEAR(bluff[1], 1.5 / 16, 0.015);  // not confident enough to deviate
    CHECK_NEAR(given_up[1], given_up[0], 0.5);
    CHECK(bluff[2] > 0.95);  // now it's worth it, fire everything
    CHECK(bluff[3] > 0.95);
    CHECK(given_up[2] > given_up[0]);
}

TEST(solver_exploitability_drops) {
    RiverSpot s;
    s.board = parse_mask("Ks7d2c9hTs");
    s.range[0] = Range::parse("top40");
    s.range[1] = Range::parse("top40");
    s.bet_sizes = {0.5, 1.0};
    RiverSolver sv(s);
    sv.iterate(10);
    double early = sv.exploitability();
    sv.iterate(190);
    double later = sv.exploitability();
    CHECK(later < early);
    CHECK(later < 1.0);  // under 1% of the pot after 200 iterations
}
