#pragma once

#include <algorithm>
#include <array>
#include <random>
#include <string>

#include "card.h"

namespace poker {

enum Street { PREFLOP, FLOP, TURN, RIVER };

enum class Act { FOLD, CHECK, CALL, RAISE };  // RAISE also covers the first bet of a street

struct Action {
    Act type = Act::CHECK;
    int amount = 0;  // RAISE only: total chips in on this street after the raise ("raise to")

    static Action fold() { return {Act::FOLD, 0}; }
    static Action check() { return {Act::CHECK, 0}; }
    static Action call() { return {Act::CALL, 0}; }
    static Action raise(int to) { return {Act::RAISE, to}; }
};
std::string to_string(const Action& a);

struct GameState {
    int button = 0;  // posts the small blind, first to act preflop, last after
    int big_blind = 100;
    int street = PREFLOP;
    Mask board = 0;
    int pot = 0;          // chips from finished streets
    int stack[2] = {};    // behind
    int bet[2] = {};      // in on this street
    int to_act = 0;
    int last_raise = 0;   // size of the last raise, for the min raise rule
    int raises = 0;       // bets + raises this street (blinds don't count)
    int aggressor = -1;   // last bettor this street
    int prev_aggressor = -1;  // last bettor on the previous street (cbet stats)
    bool acted[2] = {};

    int to_call(int s) const { return bet[1 - s] - bet[s]; }
    int total_pot() const { return pot + bet[0] + bet[1]; }
    // can't raise past what the other guy can match
    int max_raise_to(int s) const { return std::min(bet[s] + stack[s], bet[1 - s] + stack[1 - s]); }
    int min_raise_to(int s) const {
        return std::min(bet[1 - s] + std::max(last_raise, big_blind), max_raise_to(s));
    }
    bool can_raise(int s) const { return stack[1 - s] > 0 && max_raise_to(s) > bet[1 - s]; }
};

struct HandResult {
    int net[2] = {};    // chips won / lost per seat
    bool showdown = false;
    Mask hole[2] = {};  // only filled in at showdown, folded hands stay hidden
    Mask board = 0;
};

class Bot {
public:
    virtual ~Bot() = default;
    virtual std::string name() const = 0;
    virtual void new_hand(int /*seat*/, Mask /*hole*/, const GameState&) {}
    virtual Action act(const GameState& g, int seat, Mask hole) = 0;
    // every action at the table (ours too), with how long the player took to decide
    virtual void observe(const GameState& /*before*/, int /*seat*/, const Action&, double /*latency_us*/) {}
    virtual void new_street(const GameState&) {}
    virtual void hand_over(int /*seat*/, const HandResult&) {}
};

// Heads up, stacks reset to `stack` every hand.
class Table {
public:
    explicit Table(int stack = 10000, int big_blind = 100) : stack_(stack), bb_(big_blind) {}

    // cards: [0,1] seat 0 hole, [2,3] seat 1 hole, [4..8] board
    HandResult play(Bot& seat0, Bot& seat1, int button, const std::array<int, 9>& cards);
    static std::array<int, 9> deal(std::mt19937_64& rng);

    int big_blind() const { return bb_; }

private:
    int stack_, bb_;
    Action legalize(const GameState& g, int s, Action a) const;
};

}  // namespace poker
