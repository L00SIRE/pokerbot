#include "opponent.h"

#include <algorithm>
#include <map>

namespace poker {

const char* stat_name(Stat s) {
    static const char* names[] = {
        "vpip",        "pfr",           "3bet",          "fold to 3bet",
        "cbet flop",   "cbet turn",     "cbet river",    "fold cbet flop",
        "fold cbet turn", "fold cbet river", "check raise", "wtsd", "wwsf",
        "shove",       "bet flop",      "bet turn",      "bet river",
        "fold flop",   "fold turn",     "fold river",
    };
    return names[s];
}

double baseline(Stat s) {
    switch (s) {
    case VPIP: return 0.70;
    case PFR: return 0.45;
    case THREE_BET: return 0.15;
    case FOLD_TO_3BET: return 0.45;
    case CBET_FLOP: return 0.60;
    case CBET_TURN: return 0.50;
    case CBET_RIVER: return 0.45;
    case FOLD_CBET_FLOP: return 0.40;
    case FOLD_CBET_TURN: return 0.40;
    case FOLD_CBET_RIVER: return 0.45;
    case CHECK_RAISE: return 0.10;
    case WTSD: return 0.35;
    case WWSF: return 0.48;
    case SHOVE: return 0.05;
    case BET_FLOP: return 0.45;
    case BET_TURN: return 0.40;
    case BET_RIVER: return 0.40;
    case FOLD_BET_FLOP: return 0.38;
    case FOLD_BET_TURN: return 0.42;
    case FOLD_BET_RIVER: return 0.47;
    case NUM_STATS: break;
    }
    return 0.5;
}

Profile::Profile() {
    for (int s = 0; s < NUM_STATS; s++) stats[s] = BetaStat(baseline(Stat(s)), 8);
}

static double kl_bernoulli(double p, double q) {
    p = std::clamp(p, 1e-6, 1 - 1e-6);
    q = std::clamp(q, 1e-6, 1 - 1e-6);
    return p * std::log(p / q) + (1 - p) * std::log((1 - p) / (1 - q));
}

double Profile::divergence() const {
    double d = 0;
    for (int s = 0; s < NUM_STATS; s++) d += stats[s].n * kl_bernoulli(get(Stat(s)), baseline(Stat(s)));
    return d;
}

std::vector<Leak> Profile::leaks(int top, int min_obs) const {
    std::vector<Leak> out;
    for (int s = 0; s < NUM_STATS; s++) {
        if (stats[s].n < min_obs) continue;
        double o = get(Stat(s)), e = baseline(Stat(s));
        out.push_back({Stat(s), o, e, stats[s].n * kl_bernoulli(o, e)});
    }
    std::sort(out.begin(), out.end(), [](const Leak& a, const Leak& b) { return a.score > b.score; });
    if (int(out.size()) > top) out.resize(top);
    return out;
}

bool Profile::maniac() const {
    const BetaStat& s = stats[SHOVE];
    return s.mean() > baseline(SHOVE) + 2 * s.sd();
}

// --- archetypes ---

int size_bucket(double f) {
    if (f < 0.5) return 0;
    if (f < 0.875) return 1;
    if (f < 1.12) return 2;
    return 3;
}

// P(fold, call, raise) facing 1/3, 3/4, pot, overbet. the reg folds about breakeven_fold
static const double LIKELY[NUM_ARCHETYPES][NUM_SIZES][3] = {
    {{0.55, 0.37, 0.08}, {0.65, 0.29, 0.06}, {0.72, 0.23, 0.05}, {0.82, 0.15, 0.03}},  // nit
    {{0.12, 0.83, 0.05}, {0.16, 0.80, 0.04}, {0.20, 0.76, 0.04}, {0.28, 0.69, 0.03}},  // station
    {{0.10, 0.35, 0.55}, {0.12, 0.38, 0.50}, {0.14, 0.40, 0.46}, {0.18, 0.42, 0.40}},  // maniac
    {{0.25, 0.63, 0.12}, {0.43, 0.47, 0.10}, {0.50, 0.41, 0.09}, {0.55, 0.38, 0.07}},  // reg
};

ArchetypeModel::ArchetypeModel() { p_.fill(1.0 / NUM_ARCHETYPES); }

const char* ArchetypeModel::name(int a) {
    static const char* names[] = {"nit", "station", "maniac", "reg"};
    return names[a];
}

void ArchetypeModel::observe(int size, int response) {
    double t = 0;
    for (int k = 0; k < NUM_ARCHETYPES; k++) t += p_[k] *= LIKELY[k][size][response];
    // keep a little mass everywhere so a player who changes gears can still be re-read
    for (auto& x : p_) x = std::max(x / t, 1e-4);
    t = 0;
    for (double x : p_) t += x;
    for (auto& x : p_) x /= t;
}

std::array<double, 3> ArchetypeModel::predict(int size) const {
    std::array<double, 3> r{};
    for (int k = 0; k < NUM_ARCHETYPES; k++)
        for (int o = 0; o < 3; o++) r[o] += p_[k] * LIKELY[k][size][o];
    return r;
}

double ArchetypeModel::info_gain(int size) const {
    auto marginal = predict(size);
    double ig = 0;
    for (int k = 0; k < NUM_ARCHETYPES; k++)
        for (int o = 0; o < 3; o++) {
            double l = LIKELY[k][size][o];
            ig += p_[k] * l * std::log2(l / marginal[o]);
        }
    return ig;
}

double ArchetypeModel::entropy() const {
    double h = 0;
    for (double x : p_)
        if (x > 0) h -= x * std::log2(x);
    return h;
}

// --- bot detection ---

void BotDetector::add_latency(double us) {
    double x = std::log(std::max(us, 1.0));
    n_++;
    double d = x - mean_;
    mean_ += d / n_;
    m2_ += d * (x - mean_);
}

void BotDetector::add_bet_size(double f) { sizes_.push_back(int(std::lround(f * 20))); }

double BotDetector::log_latency_sd() const { return n_ > 1 ? std::sqrt(m2_ / (n_ - 1)) : 0; }

double BotDetector::size_entropy() const {
    std::map<int, int> count;
    for (int s : sizes_) count[s]++;
    double h = 0;
    for (auto& [s, c] : count) {
        double p = double(c) / sizes_.size();
        h -= p * std::log2(p);
    }
    return h;
}

double BotDetector::score() const {
    if (n_ < 10) return 0.5;
    auto sig = [](double x) { return 1 / (1 + std::exp(-x)); };
    double fast = sig((std::log(200000.0) - mean_) / 1.0);  // typical decision under ~200ms
    double steady = sig((0.5 - log_latency_sd()) / 0.15);   // people are lognormal with sd ~0.8+
    double rigid = sizes_.size() < 5 ? 0.5 : sig((1.5 - size_entropy()) / 0.4);
    return (fast + steady + rigid) / 3;
}

}  // namespace poker
