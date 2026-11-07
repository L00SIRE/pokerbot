#pragma once

#include <array>
#include <random>
#include <string>
#include <vector>

#include "game.h"
#include "opponent.h"
#include "range.h"
#include "river_solver.h"
#include "tracker.h"

namespace poker {

struct BullyConfig {
    bool use_solver = true;  // solve rivers live, otherwise equity heuristics everywhere
    int solver_iters = 60;
    Algo algo = Algo::DCFR;
    double p_max = 0.85;  // the most we ever trust the opponent model (rnr p)
    double tau = 35;      // hands until we're ~63% of the way to p_max
    bool probe = true;    // early on, pick bet sizes that teach us the most about them
    bool decoy = false;   // cheap folds in tiny pots, poisons sliding window / ewma trackers
    int mc_samples = 400;
    uint64_t seed = 7;
};

// The main bot. Tracks both ranges through the hand, keeps a profile of the
// opponent, and leans on them harder the more hands of evidence it has.
//   preflop      equity vs their (estimated) range + pot odds. vs shovers that range
//                gets wide fast, so we snap them off light
//   flop/turn    equity vs range, MDF floor that fades as we trust the read,
//                blocker-picked bluffs that ramp up vs people who over-fold,
//                geometric sizing when their range is capped
//   river        live DCFR solve with restricted nash response vs their model
class BullyBot : public Bot {
public:
    explicit BullyBot(BullyConfig cfg = {});
    std::string name() const override { return "bully"; }
    void new_hand(int seat, Mask hole, const GameState& g) override;
    Action act(const GameState& g, int seat, Mask hole) override;
    void observe(const GameState& before, int seat, const Action& a, double latency_us) override;
    void new_street(const GameState& g) override;
    void hand_over(int seat, const HandResult& r) override;

    const Profile& profile() const { return tracker_.profile; }
    const ArchetypeModel& archetypes() const { return arch_; }
    const BotDetector& detector() const { return detector_; }
    double trust() const;  // current rnr p
    int river_solves() const { return solves_; }
    std::string report() const;

private:
    BullyConfig cfg_;
    std::mt19937_64 rng_;
    StatTracker tracker_;
    Profile self_;  // how we assume we look to them, a baseline reg
    ArchetypeModel arch_;
    BotDetector detector_;

    int seat_ = 0;
    Mask hole_ = 0;
    int combo_ = 0;
    Range range_[2];          // per seat, public info only (our hole cards aren't removed from theirs)
    std::vector<int> order_;  // live combos, weakest first on this board
    std::array<uint32_t, NUM_COMBOS> power_{};
    bool opp_capped_ = false;
    int probe_size_ = -1;  // size bucket of our last bet, waiting for their answer
    int solves_ = 0;

    Action preflop(const GameState& g);
    Action postflop(const GameState& g);
    Action river(const GameState& g);

    void narrow(const GameState& before, int seat, const Action& a);
    void strengths(const Range& r, std::array<double, NUM_COMBOS>& out) const;
    void build_model(RiverSolver& sv, int who) const;
    double pick_size(const GameState& g, double eq, bool value);
    double fold_estimate(int street, double frac) const;
    double blocker_score() const;
    Action raise_to(const GameState& g, int amount) const;
    bool coin(double p);
};

}  // namespace poker
