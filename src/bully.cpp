#include "bully.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

#include "combos.h"
#include "equity.h"
#include "eval.h"
#include "exploit.h"

namespace poker {
namespace {

double sigmoid(double x) { return 1 / (1 + std::exp(-x)); }

// ~1 for hands in the top `frac` of a range (strength 1 = best), ~0 below
double soft_top(double strength, double frac) { return sigmoid((strength - (1 - frac)) / 0.06); }
double soft_bottom(double strength, double frac) { return sigmoid((frac - strength) / 0.06); }

Stat on_street(Stat flop_stat, int street) { return Stat(flop_stat + street - FLOP); }

// P(this action | hand strength) for a player with profile p. Used to narrow ranges.
double likelihood(const GameState& g, const Profile& p, int s, const Action& a, double str) {
    if (g.street == PREFLOP) {
        switch (a.type) {
        case Act::RAISE: {
            bool allin = a.amount >= g.bet[s] + g.stack[s];
            double f = allin ? p.get(SHOVE) : g.raises == 0 ? p.get(PFR) : p.get(THREE_BET);
            if (g.raises >= 2 && !allin) f *= 0.5;
            return 0.02 + 0.98 * soft_top(str, std::max(f, 0.01));
        }
        case Act::CALL:
            return 0.05 + 0.95 * soft_top(str, p.get(VPIP)) * (1 - 0.6 * soft_top(str, 0.5 * p.get(PFR)));
        case Act::CHECK: return 1 - 0.7 * soft_top(str, p.get(PFR));
        case Act::FOLD: return 1;
        }
    }
    const double bet = p.get(on_street(BET_FLOP, g.street));
    const double fold = p.get(on_street(FOLD_BET_FLOP, g.street));
    switch (a.type) {
    case Act::RAISE:
        if (g.to_call(s) == 0) return 0.25 + 0.75 * soft_top(str, bet);  // value plus some bluffs/draws
        return 0.15 + 0.85 * soft_top(str, 0.35 * (1 - fold));          // raises are a lot narrower
    case Act::CALL: return 0.08 + 0.92 * soft_top(str, 1 - fold);
    case Act::CHECK: return 1 - 0.5 * soft_top(str, 0.5 * bet);  // they'd often bet the top
    case Act::FOLD: return 1;
    }
    return 1;
}

}  // namespace

BullyBot::BullyBot(BullyConfig cfg) : cfg_(cfg), rng_(cfg.seed) {}

double BullyBot::trust() const { return rnr_weight(tracker_.profile.hands, cfg_.p_max, cfg_.tau); }

bool BullyBot::coin(double p) { return std::uniform_real_distribution<double>(0, 1)(rng_) < p; }

void BullyBot::new_hand(int seat, Mask hole, const GameState&) {
    seat_ = seat;
    hole_ = hole;
    combo_ = combo_index(hole);
    range_[0] = range_[1] = Range::uniform();
    order_.clear();
    opp_capped_ = false;
    probe_size_ = -1;
    tracker_.new_hand(1 - seat);
}

void BullyBot::new_street(const GameState& g) {
    const auto& cs = combos();
    order_.clear();
    for (int k = 0; k < NUM_COMBOS; k++) {
        power_[k] = 0;
        if (cs[k].mask & g.board) continue;
        power_[k] = evaluate(cs[k].mask | g.board);
        order_.push_back(k);
    }
    std::sort(order_.begin(), order_.end(), [&](int a, int b) { return power_[a] < power_[b]; });
    for (auto& r : range_) {
        r.remove(g.board);
        if (r.total() <= 0) r = Range::uniform(), r.remove(g.board);  // read went wrong, start over
        r.normalize();
    }
    probe_size_ = -1;
    tracker_.on_street(g);
}

void BullyBot::hand_over(int, const HandResult& r) { tracker_.on_hand_end(r); }

// strength of every combo within range r on this board, 0 = weakest, 1 = strongest
void BullyBot::strengths(const Range& r, std::array<double, NUM_COMBOS>& out) const {
    out.fill(0);
    double total = 0;
    for (int k : order_) total += r.w[k];
    if (total <= 0) return;
    double below = 0;
    for (size_t i = 0; i < order_.size();) {
        size_t j = i;
        double tie = 0;
        while (j < order_.size() && power_[order_[j]] == power_[order_[i]]) tie += r.w[order_[j++]];
        double s = (below + tie / 2) / total;
        for (size_t x = i; x < j; x++) out[order_[x]] = s;
        below += tie;
        i = j;
    }
}

void BullyBot::observe(const GameState& g, int s, const Action& a, double latency_us) {
    const int opp = 1 - seat_;
    if (s == opp) {
        tracker_.on_action(g, s, a);
        detector_.add_latency(latency_us);
        if (a.type == Act::RAISE && g.street > PREFLOP)
            detector_.add_bet_size(double(a.amount - g.bet[s]) / std::max(1, g.total_pot()));
        if (probe_size_ >= 0 && g.to_call(s) > 0) {
            int resp = a.type == Act::FOLD ? FOLD_R : a.type == Act::CALL ? CALL_R : RAISE_R;
            arch_.observe(probe_size_, resp);
            probe_size_ = -1;
        }
        if (a.type == Act::CALL) opp_capped_ = true;  // flatted instead of raising the monsters
        if (a.type == Act::RAISE) opp_capped_ = false;
    } else if (a.type == Act::RAISE && g.street > PREFLOP) {
        probe_size_ = size_bucket(double(a.amount - g.bet[s]) / std::max(1, g.total_pot()));
    }
    if (a.type != Act::FOLD) narrow(g, s, a);
}

void BullyBot::narrow(const GameState& g, int s, const Action& a) {
    const Profile& p = s == seat_ ? self_ : tracker_.profile;
    Range& r = range_[s];
    std::array<double, NUM_COMBOS> str{};
    if (g.street == PREFLOP) {
        for (int k = 0; k < NUM_COMBOS; k++) str[k] = 1 - preflop_top_frac(combo_class(k));
    } else {
        strengths(r, str);
    }
    for (int k = 0; k < NUM_COMBOS; k++)
        if (r.w[k] > 0) r.w[k] *= likelihood(g, p, s, a, str[k]);
    r.normalize();
}

Action BullyBot::raise_to(const GameState& g, int amount) const {
    if (!g.can_raise(seat_)) return g.to_call(seat_) > 0 ? Action::call() : Action::check();
    return Action::raise(std::clamp(amount, g.min_raise_to(seat_), g.max_raise_to(seat_)));
}

Action BullyBot::act(const GameState& g, int, Mask) {
    if (g.street == PREFLOP) return preflop(g);
    if (g.street == RIVER && cfg_.use_solver) return river(g);
    return postflop(g);
}

Action BullyBot::preflop(const GameState& g) {
    const int me = seat_, opp = 1 - me;
    const int tc = g.to_call(me);
    const int bb = g.big_blind;
    const double pot = g.total_pot();
    const double top = preflop_top_frac(combo_class(combo_));  // 0 = AA
    const Profile& prof = tracker_.profile;
    const double p = trust();

    // facing a shove (or a raise that puts us in): pure pot odds vs their shoving range.
    // a maniac's range gets wide quickly through narrow(), so this calls lighter and lighter
    if (tc > 0 && (g.stack[opp] == 0 || tc >= g.stack[me])) {
        double eq = equity_vs_range(hole_, range_[opp], 0, cfg_.mc_samples * 3, rng_);
        return eq > required_equity(pot, std::min(tc, g.stack[me])) ? Action::call() : Action::fold();
    }

    if (g.raises == 0 && me == g.button) {
        // first in from the small blind. open wider vs big blinds that fold a lot
        double bb_folds = 1 - prof.get(VPIP);
        double open = 0.6 + 0.3 * p * std::clamp((bb_folds - 0.35) / 0.4, 0.0, 1.0);
        return top < open ? raise_to(g, int(2.5 * bb)) : Action::fold();
    }
    if (g.raises == 0) {  // big blind after a limp
        return top < 0.3 ? raise_to(g, 4 * bb) : Action::check();
    }

    // facing a raise
    const double eq = equity_vs_range(hole_, range_[opp], 0, cfg_.mc_samples, rng_);
    const int reraise = std::min(3 * g.bet[opp], g.max_raise_to(me));
    if (eq > 0.6 && g.raises < 4) return raise_to(g, reraise);
    if (g.raises == 1 && g.can_raise(me)) {
        // light 3bets vs people who give up to them too often
        double be = breakeven_fold(pot, reraise - g.bet[me]);
        if (prof.get(FOLD_TO_3BET) > be + 0.05 && top < 0.7 && coin(p)) return raise_to(g, reraise);
    }
    const double implied = g.stack[me] > 20 * tc ? 0.04 : 0.0;  // deep, we get paid when we hit
    return eq + implied >= required_equity(pot, tc) ? Action::call() : Action::fold();
}

// fold chance for a bet of `frac` pot: half from their stats, half from the archetype read
double BullyBot::fold_estimate(int street, double frac) const {
    double stat = tracker_.profile.get(on_street(FOLD_BET_FLOP, street));
    double arch = arch_.predict(size_bucket(frac))[FOLD_R];
    return 0.5 * stat + 0.5 * arch;
}

// share of their strong hands we block / share of their folding hands we block. > 1 = good bluff cards
double BullyBot::blocker_score() const {
    const Range& r = range_[1 - seat_];
    std::array<double, NUM_COMBOS> str;
    strengths(r, str);
    double nut = 0, nut_blocked = 0, weak = 0, weak_blocked = 0;
    for (int k : order_) {
        double w = r.w[k];
        if (w <= 0) continue;
        bool blocked = combos()[k].mask & hole_;
        if (str[k] > 0.8) nut += w, nut_blocked += blocked ? w : 0;
        else if (str[k] < 0.5) weak += w, weak_blocked += blocked ? w : 0;
    }
    if (nut <= 0 || weak <= 0) return 1;
    return (nut_blocked / nut + 0.01) / (weak_blocked / weak + 0.01);
}

double BullyBot::pick_size(const GameState& g, double eq, bool value) {
    const double pot = g.total_pot();
    const double eff = std::min(g.stack[0], g.stack[1]);
    const int streets_left = RIVER - g.street + 1;

    // their range is capped and we're strong: size so the stacks go in by the river
    if (value && opp_capped_) return std::clamp(geometric_fraction(pot, eff, streets_left), 0.33, 2.0);

    // otherwise pick the size with the best EV, plus a bonus for what it teaches us early on
    const double beta = cfg_.probe ? 0.4 * std::exp(-tracker_.profile.hands / 25.0) : 0.0;
    int best = value ? 1 : 0;
    double best_score = -1e9;
    for (int b = 0; b < NUM_SIZES; b++) {
        auto r = arch_.predict(b);
        double B = PROBE_SIZES[b];
        double called = eq * (1 + 2 * B) - B;  // in pot units
        double ev = r[FOLD_R] + r[CALL_R] * called + r[RAISE_R] * (eq > 0.7 ? called : -B);
        double score = ev + beta * arch_.info_gain(b);
        if (score > best_score) best_score = score, best = b;
    }
    return PROBE_SIZES[best];
}

Action BullyBot::postflop(const GameState& g) {
    const int me = seat_, opp = 1 - me;
    const int tc = g.to_call(me);
    const double pot = g.total_pot();
    const double p = trust();
    const double eq = equity_vs_range(hole_, range_[opp], g.board, cfg_.mc_samples, rng_);

    if (tc > 0) {
        if (cfg_.decoy && pot < 4 * g.big_blind && eq < 0.5) return Action::fold();
        if (eq > 0.8 && g.can_raise(me)) return raise_to(g, g.bet[opp] + int(0.8 * (pot + tc)));
        if (eq >= required_equity(pot, tc)) return Action::call();

        // nash floor: keep defending the top MDF of our range so blind aggression doesn't
        // print money. it fades out as the read on them gets trustworthy
        double floor = mdf(pot - tc, tc) * (1 - p);
        std::array<double, NUM_COMBOS> str;
        strengths(range_[me], str);
        return str[combo_] > 1 - floor ? Action::call() : Action::fold();
    }

    if (eq > 0.65) {
        // let maniacs hang themselves
        if (arch_.posterior()[MANIAC] > 0.6 && eq > 0.85 && coin(0.5)) return Action::check();
        return raise_to(g, g.bet[opp] + int(pick_size(g, eq, true) * pot));
    }
    if (eq < 0.4) {
        const double frac = pick_size(g, eq, false);
        const double B = frac * pot;
        const double be = breakeven_fold(pot, B);
        const double F = fold_estimate(g.street, frac);
        // baseline: enough bluffs to keep value bets honest, more with good blockers
        double bf = bluff_fraction(pot, B);
        double freq = 0.75 * bf / (1 - bf) * std::clamp(blocker_score(), 0.5, 2.0);
        if (F > be) freq += (1 - freq) * p * std::clamp((F - be) / 0.15, 0.0, 1.0);  // they over-fold
        else if (F < be - 0.1) freq *= 1 - p;                                        // stations: stop
        if (coin(freq)) return raise_to(g, g.bet[opp] + int(B));
    }
    return Action::check();
}

// opponent model for the rnr solve, built from their river stats
void BullyBot::build_model(RiverSolver& sv, int who) const {
    const Profile& prof = tracker_.profile;
    const double bet = prof.get(BET_RIVER), fold = prof.get(FOLD_BET_RIVER);
    std::array<double, NUM_COMBOS> str;
    strengths(sv.spot().range[who], str);

    for (Node& n : sv.tree()) {
        if (n.type != Node::ACTION || n.player != who) continue;
        const int na = n.num_actions();
        const int bets = int(std::count(n.moves.begin(), n.moves.end(), Move::BET));
        const bool facing = n.bet[who] < n.bet[1 - who];
        n.model.assign(na * NUM_COMBOS, 0.0);
        for (int k = 0; k < NUM_COMBOS; k++) {
            const double s = str[k];
            double pf = 0, pb = 0;
            if (facing) {
                pf = 1 - soft_top(s, 1 - fold);  // the weakest `fold` share gives up
                pb = bets ? 0.6 * soft_top(s, 0.08) : 0;
            } else {
                pb = bets ? std::max(soft_top(s, 0.7 * bet), 0.8 * soft_bottom(s, 0.3 * bet)) : 0;
            }
            const double rest = std::max(0.0, 1 - pf - pb);
            double sum = 0;
            for (int a = 0; a < na; a++) {
                double v = n.moves[a] == Move::FOLD ? pf : n.moves[a] == Move::BET ? pb / bets : rest;
                n.model[a * NUM_COMBOS + k] = v;
                sum += v;
            }
            for (int a = 0; a < na; a++) n.model[a * NUM_COMBOS + k] = sum > 0 ? n.model[a * NUM_COMBOS + k] / sum : 1.0 / na;
        }
    }
}

Action BullyBot::river(const GameState& g) {
    const int oop = 1 - g.button;
    const int seat_of[2] = {oop, g.button};
    const int me = seat_ == oop ? 0 : 1;

    RiverSpot spot;
    spot.board = g.board;
    spot.pot = g.pot;
    spot.stack = std::min(g.stack[0] + g.bet[0], g.stack[1] + g.bet[1]);
    for (int i = 0; i < 2; i++) spot.range[i] = range_[seat_of[i]];
    // our real hand has to be in our range or its strategy is garbage
    spot.range[me].w[combo_] = std::max(spot.range[me].w[combo_], 0.05);
    spot.to_act = me;
    spot.bet[0] = g.bet[oop];
    spot.bet[1] = g.bet[g.button];
    spot.raises = g.raises;
    spot.bet_sizes = {0.5, 1.0};
    spot.raise_sizes = {1.0};
    spot.max_raises = g.raises + 2;

    RiverSolver sv(spot, cfg_.algo);
    const double p = trust();
    if (p > 0.02) {
        build_model(sv, 1 - me);
        sv.set_exploit(1 - me, p);
    }
    sv.iterate(cfg_.solver_iters);
    solves_++;

    const auto probs = sv.strategy(0, combo_);
    double u = std::uniform_real_distribution<double>(0, 1)(rng_);
    int a = 0;
    for (; a + 1 < int(probs.size()); a++) {
        u -= probs[a];
        if (u <= 0) break;
    }
    const Node& root = sv.tree()[0];
    switch (root.moves[a]) {
    case Move::FOLD: return Action::fold();
    case Move::CHECK: return Action::check();
    case Move::CALL: return Action::call();
    case Move::BET: return raise_to(g, int(std::lround(sv.tree()[root.kids[a]].bet[me])));
    }
    return Action::check();
}

std::string BullyBot::report() const {
    const Profile& p = tracker_.profile;
    std::ostringstream o;
    char buf[256];
    std::snprintf(buf, sizeof buf, "  read after %d hands (trust p = %.2f, %d river solves)\n", p.hands, trust(), solves_);
    o << buf;
    std::snprintf(buf, sizeof buf,
                  "  vpip %.0f%%  pfr %.0f%%  3bet %.0f%%  shove %.0f%%  fold to river bet %.0f%%  wtsd %.0f%%\n",
                  100 * p.get(VPIP), 100 * p.get(PFR), 100 * p.get(THREE_BET), 100 * p.get(SHOVE),
                  100 * p.get(FOLD_BET_RIVER), 100 * p.get(WTSD));
    o << buf << "  biggest leaks:";
    for (const Leak& l : p.leaks(3)) {
        std::snprintf(buf, sizeof buf, "  %s %.0f%% (reg ~%.0f%%)", stat_name(l.stat), 100 * l.observed, 100 * l.expected);
        o << buf;
    }
    o << "\n  looks like:";
    for (int k = 0; k < NUM_ARCHETYPES; k++) {
        std::snprintf(buf, sizeof buf, "  %s %.0f%%", ArchetypeModel::name(k), 100 * arch_.posterior()[k]);
        o << buf;
    }
    std::snprintf(buf, sizeof buf, "\n  bot score %.2f%s\n", detector_.score(), p.maniac() ? "   [maniac flagged]" : "");
    o << buf;
    return o.str();
}

}  // namespace poker
