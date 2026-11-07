#pragma once

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace poker {

// beta-binomial counter for a yes/no habit ("folds to cbet"). the prior acts like
// `strength` fake observations at `mean`, so 3 hands of data don't swing it to 100%
struct BetaStat {
    double a = 1, b = 1;
    int n = 0;

    BetaStat() = default;
    BetaStat(double mean, double strength) : a(mean * strength), b((1 - mean) * strength) {}

    void add(bool yes) {
        (yes ? a : b) += 1;
        n++;
    }
    double mean() const { return a / (a + b); }
    double sd() const {
        double s = a + b;
        return std::sqrt(a * b / (s * s * (s + 1)));
    }
};

enum Stat {
    VPIP, PFR, THREE_BET, FOLD_TO_3BET,
    CBET_FLOP, CBET_TURN, CBET_RIVER,
    FOLD_CBET_FLOP, FOLD_CBET_TURN, FOLD_CBET_RIVER,
    CHECK_RAISE, WTSD, WWSF,
    SHOVE,                                        // preflop first action is all in
    BET_FLOP, BET_TURN, BET_RIVER,                // bets when nobody has bet yet
    FOLD_BET_FLOP, FOLD_BET_TURN, FOLD_BET_RIVER, // folds when facing any bet
    NUM_STATS
};

const char* stat_name(Stat s);
// rough numbers for a solid heads up reg. not solver output, just a sane reference
double baseline(Stat s);

struct Leak {
    Stat stat;
    double observed, expected, score;  // score = n * KL(observed || expected)
};

struct Profile {
    std::array<BetaStat, NUM_STATS> stats;
    int hands = 0;

    Profile();
    double get(Stat s) const { return stats[s].mean(); }
    const BetaStat& operator[](Stat s) const { return stats[s]; }
    BetaStat& operator[](Stat s) { return stats[s]; }

    // sum over stats of n * KL(bernoulli(observed) || bernoulli(baseline)).
    // big = they play nothing like a reg
    double divergence() const;
    std::vector<Leak> leaks(int top = 3, int min_obs = 5) const;
    // shoving way more than a balanced range would (posterior mean > baseline + 2 sd)
    bool maniac() const;
};

// --- archetypes + information gain -----------------------------------------

enum Archetype { NIT, STATION, MANIAC, REG, NUM_ARCHETYPES };
enum Response { FOLD_R, CALL_R, RAISE_R };

constexpr int NUM_SIZES = 4;
constexpr double PROBE_SIZES[NUM_SIZES] = {0.33, 0.75, 1.0, 1.25};  // pot fractions
int size_bucket(double pot_frac);

// Bayesian guess of what kind of player this is, from how they answer our bets.
// Also says which bet size would teach us the most (expected information gain).
class ArchetypeModel {
public:
    ArchetypeModel();
    void observe(int size, int response);
    std::array<double, 3> predict(int size) const;  // P(fold, call, raise)
    // I(M; response) = sum_k sum_o P(o|k,a) P(k) log2(P(o|k,a) / P(o|a)), in bits
    double info_gain(int size) const;
    double entropy() const;
    const std::array<double, NUM_ARCHETYPES>& posterior() const { return p_; }
    static const char* name(int a);

private:
    std::array<double, NUM_ARCHETYPES> p_;
};

// --- bot detection ---------------------------------------------------------

// Humans think in roughly log-normal time and vary their sizes. Scripts answer
// in microseconds, with steady timing, from a handful of sizes.
class BotDetector {
public:
    void add_latency(double us);
    void add_bet_size(double pot_frac);
    int decisions() const { return n_; }
    double size_entropy() const;  // bits, over sizes rounded to 5% of pot
    double log_latency_sd() const;
    double score() const;  // 0 = looks human, 1 = looks like a script, 0.5 = no idea yet

private:
    int n_ = 0;
    double mean_ = 0, m2_ = 0;  // welford on log(latency)
    std::vector<int> sizes_;
};

}  // namespace poker
