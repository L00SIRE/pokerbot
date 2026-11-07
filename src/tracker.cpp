#include "tracker.h"

namespace poker {

static Stat on(Stat flop_stat, int street) { return Stat(flop_stat + street - FLOP); }

void StatTracker::new_hand(int opp) {
    opp_ = opp;
    first_pre_ = true;
    raised_pre_ = faced_3bet_ = saw_flop_ = false;
    for (bool& c : checked_) c = false;
}

void StatTracker::on_action(const GameState& g, int seat, const Action& a) {
    if (seat != opp_) return;
    Profile& p = profile;
    const bool raise = a.type == Act::RAISE, fold = a.type == Act::FOLD;
    const int tc = g.to_call(seat);

    if (g.street == PREFLOP) {
        if (first_pre_) {
            p[VPIP].add(raise || a.type == Act::CALL);  // bb checking his option isn't voluntary
            p[PFR].add(raise);
            p[SHOVE].add(raise && a.amount >= g.bet[seat] + g.stack[seat]);
            first_pre_ = false;
        }
        if (g.raises == 1 && g.aggressor != seat) p[THREE_BET].add(raise);
        if (raised_pre_ && g.raises >= 2 && g.aggressor != seat && !faced_3bet_) {
            p[FOLD_TO_3BET].add(fold);
            faced_3bet_ = true;
        }
        if (raise) raised_pre_ = true;
        return;
    }

    const int st = g.street;
    if (tc == 0 && g.raises == 0) {
        p[on(BET_FLOP, st)].add(raise);
        if (g.prev_aggressor == seat) p[on(CBET_FLOP, st)].add(raise);
    } else if (tc > 0) {
        p[on(FOLD_BET_FLOP, st)].add(fold);
        if (g.raises == 1 && g.aggressor == 1 - seat && g.prev_aggressor == 1 - seat)
            p[on(FOLD_CBET_FLOP, st)].add(fold);
        if (checked_[st] && g.raises == 1) p[CHECK_RAISE].add(raise);
    }
    if (a.type == Act::CHECK) checked_[st] = true;
}

void StatTracker::on_street(const GameState& g) {
    if (g.street == FLOP) saw_flop_ = true;
}

void StatTracker::on_hand_end(const HandResult& r) {
    if (saw_flop_) {
        profile[WTSD].add(r.showdown);
        profile[WWSF].add(r.net[opp_] > 0);
    }
    profile.hands++;
}

}  // namespace poker
