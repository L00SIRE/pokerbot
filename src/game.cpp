#include "game.h"

#include <algorithm>
#include <chrono>

#include "eval.h"

namespace poker {

std::string to_string(const Action& a) {
    switch (a.type) {
    case Act::FOLD: return "fold";
    case Act::CHECK: return "check";
    case Act::CALL: return "call";
    case Act::RAISE: return "raise to " + std::to_string(a.amount);
    }
    return "?";
}

std::array<int, 9> Table::deal(std::mt19937_64& rng) {
    std::array<int, 52> deck;
    for (int i = 0; i < 52; i++) deck[i] = i;
    for (int i = 0; i < 9; i++) std::swap(deck[i], deck[i + rng() % (52 - i)]);
    std::array<int, 9> out;
    std::copy(deck.begin(), deck.begin() + 9, out.begin());
    return out;
}

// bots can ask for nonsense, turn it into the closest legal thing
Action Table::legalize(const GameState& g, int s, Action a) const {
    const int tc = g.to_call(s);
    switch (a.type) {
    case Act::FOLD: return tc > 0 ? a : Action::check();
    case Act::CHECK: return tc > 0 ? Action::fold() : a;
    case Act::CALL: return tc > 0 ? a : Action::check();
    case Act::RAISE:
        if (!g.can_raise(s)) return tc > 0 ? Action::call() : Action::check();
        return Action::raise(std::clamp(a.amount, g.min_raise_to(s), g.max_raise_to(s)));
    }
    return Action::fold();
}

static void put_in(GameState& g, int s, int chips) {
    chips = std::min(chips, g.stack[s]);
    g.stack[s] -= chips;
    g.bet[s] += chips;
}

static void apply(GameState& g, int s, const Action& a) {
    g.acted[s] = true;
    switch (a.type) {
    case Act::FOLD:
    case Act::CHECK: break;
    case Act::CALL: put_in(g, s, g.to_call(s)); break;
    case Act::RAISE: {
        int increment = a.amount - g.bet[1 - s];
        if (increment >= g.last_raise) g.last_raise = increment;
        put_in(g, s, a.amount - g.bet[s]);
        g.raises++;
        g.aggressor = s;
        g.acted[1 - s] = false;  // they have to answer
        break;
    }
    }
    g.to_act = 1 - s;
}

static bool round_over(const GameState& g) {
    if (g.bet[0] != g.bet[1]) return false;
    if (g.stack[0] == 0 || g.stack[1] == 0) return true;
    return g.acted[0] && g.acted[1];
}

HandResult Table::play(Bot& b0, Bot& b1, int button, const std::array<int, 9>& cards) {
    using Clock = std::chrono::steady_clock;
    Bot* bots[2] = {&b0, &b1};
    const Mask hole[2] = {bit(cards[0]) | bit(cards[1]), bit(cards[2]) | bit(cards[3])};

    GameState g;
    g.button = button;
    g.big_blind = bb_;
    g.stack[0] = g.stack[1] = stack_;
    put_in(g, button, bb_ / 2);
    put_in(g, 1 - button, bb_);
    g.last_raise = bb_;
    g.to_act = button;
    for (int s = 0; s < 2; s++) bots[s]->new_hand(s, hole[s], g);

    int folded = -1;
    for (;;) {
        while (!round_over(g)) {
            const int s = g.to_act;
            auto t0 = Clock::now();
            Action a = bots[s]->act(g, s, hole[s]);
            double us = std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
            a = legalize(g, s, a);
            for (Bot* b : bots) b->observe(g, s, a, us);
            apply(g, s, a);
            if (a.type == Act::FOLD) {
                folded = s;
                break;
            }
        }
        if (folded >= 0 || g.street == RIVER) break;

        g.pot += g.bet[0] + g.bet[1];
        g.bet[0] = g.bet[1] = 0;
        g.street++;
        if (g.street == FLOP) g.board = bit(cards[4]) | bit(cards[5]) | bit(cards[6]);
        else g.board |= bit(cards[g.street + 5]);  // turn = cards[7], river = cards[8]
        g.prev_aggressor = g.aggressor;
        g.aggressor = -1;
        g.raises = 0;
        g.acted[0] = g.acted[1] = false;
        g.last_raise = bb_;
        g.to_act = 1 - button;
        for (Bot* b : bots) b->new_street(g);
    }

    HandResult r;
    r.board = g.board;
    const int in[2] = {stack_ - g.stack[0], stack_ - g.stack[1]};
    if (folded >= 0) {
        // the winner only ever wins what the folder put in, any uncalled bet comes back
        r.net[folded] = -in[folded];
        r.net[1 - folded] = in[folded];
    } else {
        r.showdown = true;
        r.hole[0] = hole[0], r.hole[1] = hole[1];
        const int won = std::min(in[0], in[1]);
        uint32_t s0 = evaluate(hole[0] | g.board), s1 = evaluate(hole[1] | g.board);
        if (s0 != s1) {
            int w = s0 > s1 ? 0 : 1;
            r.net[w] = won;
            r.net[1 - w] = -won;
        }
    }
    for (int s = 0; s < 2; s++) bots[s]->hand_over(s, r);
    return r;
}

}  // namespace poker
