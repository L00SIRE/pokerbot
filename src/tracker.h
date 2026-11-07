#pragma once

#include "game.h"
#include "opponent.h"

namespace poker {

// turns table events into Profile stats for one opponent
class StatTracker {
public:
    Profile profile;

    void new_hand(int opp_seat);
    void on_action(const GameState& before, int seat, const Action& a);  // ignores our own actions
    void on_street(const GameState& g);
    void on_hand_end(const HandResult& r);

private:
    int opp_ = 1;
    bool first_pre_ = true, raised_pre_ = false, faced_3bet_ = false, saw_flop_ = false;
    bool checked_[4] = {};
};

}  // namespace poker
