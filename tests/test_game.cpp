#include <algorithm>
#include <cstdlib>
#include <vector>

#include "bots.h"
#include "check.h"
#include "eval.h"

using namespace poker;

// plays a fixed script of actions, then checks/calls
struct Script : Bot {
    std::vector<Action> moves;
    size_t i = 0;
    std::vector<int> seen_streets;
    std::vector<Action> log;  // every legalised action at the table
    std::string name() const override { return "script"; }
    Action act(const GameState& g, int s, Mask) override {
        if (i < moves.size()) return moves[i++];
        return g.to_call(s) > 0 ? Action::call() : Action::check();
    }
    void observe(const GameState&, int, const Action& a, double) override { log.push_back(a); }
    void new_street(const GameState& g) override { seen_streets.push_back(g.street); }
};

static std::array<int, 9> cards(const char* s) {
    auto v = parse_cards(s);
    std::array<int, 9> a;
    std::copy(v.begin(), v.end(), a.begin());
    return a;
}

TEST(game_fold_preflop) {
    Table t;
    Script a, b;
    a.moves = {Action::fold()};
    // seat 0 is the button / small blind and folds straight away
    HandResult r = t.play(a, b, 0, cards("AsAh KdKc 2c7d9hJsQs"));
    CHECK(r.net[0] == -50 && r.net[1] == 50);
    CHECK(!r.showdown);
    CHECK(r.hole[1] == 0);  // folded hands stay hidden
}

TEST(game_check_down_showdown) {
    Table t;
    Script a, b;
    HandResult r = t.play(a, b, 1, cards("AsAh KdKc 2c7d9hJsQs"));
    CHECK(r.showdown);
    CHECK(r.net[0] == 100 && r.net[1] == -100);  // limp + check down, aces hold
    CHECK(a.seen_streets == std::vector<int>({FLOP, TURN, RIVER}));
}

TEST(game_raise_rules) {
    Table t;
    Script a, b;
    // both try raises that are too small, the table bumps them up to the min raise
    a.moves = {Action::raise(150), Action::fold()};
    b.moves = {Action::raise(210)};
    HandResult r = t.play(a, b, 0, cards("AsAh KdKc 2c7d9hJsQs"));
    CHECK(a.log.size() == 3);
    CHECK(a.log[0].amount == 200);  // min raise over the 100 blind
    CHECK(a.log[1].amount == 300);  // min re-raise is another 100
    CHECK(r.net[0] == -200 && r.net[1] == 200);
}

TEST(game_raise_capped_by_stacks) {
    Table t(1000, 100);
    Script a, b;
    a.moves = {Action::raise(999999)};
    t.play(a, b, 0, cards("AsAh KdKc 2c7d9hJsQs"));
    CHECK(a.log[0].amount == 1000);
}

TEST(game_allin_runout_and_conservation) {
    std::mt19937_64 rng(9);
    Table t;
    auto maniac = make_bot("maniac"), station = make_bot("station"), random = make_bot("random", 3);
    auto tag = make_bot("tag", 4), nit = make_bot("nit", 5);
    for (int i = 0; i < 300; i++) {
        auto c = Table::deal(rng);
        HandResult r = t.play(*maniac, *station, i % 2, c);
        CHECK(r.net[0] + r.net[1] == 0);
        CHECK(r.showdown);
        CHECK(std::abs(r.net[0]) == 10000 || r.net[0] == 0);  // always stacks off
        HandResult q = t.play(*random, *tag, i % 2, c);
        CHECK(q.net[0] + q.net[1] == 0);
        CHECK(std::abs(q.net[0]) <= 10000);
        HandResult w = t.play(*nit, *maniac, i % 2, c);
        CHECK(w.net[0] + w.net[1] == 0);
    }
}
