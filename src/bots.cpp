#include "bots.h"

#include <random>
#include <stdexcept>

#include "combos.h"
#include "equity.h"

namespace poker {
namespace {

int pot_bet(const GameState& g, int s, double frac) {
    return g.bet[1 - s] + int(frac * (g.total_pot() + g.to_call(s)));
}

double top_frac(Mask hole) { return preflop_top_frac(combo_class(combo_index(hole))); }

class RandomBot : public Bot {
public:
    explicit RandomBot(uint64_t seed) : rng_(seed) {}
    std::string name() const override { return "random"; }
    Action act(const GameState& g, int s, Mask) override {
        int r = int(rng_() % 3);
        if (r == 0) return g.to_call(s) > 0 ? Action::fold() : Action::check();
        if (r == 1) return Action::call();
        double frac = 0.3 + (rng_() % 100) / 50.0;  // 0.3 .. 2.3 pot
        return Action::raise(pot_bet(g, s, frac));
    }

private:
    std::mt19937_64 rng_;
};

class Station : public Bot {
public:
    std::string name() const override { return "station"; }
    Action act(const GameState& g, int s, Mask) override {
        return g.to_call(s) > 0 ? Action::call() : Action::check();
    }
};

class Maniac : public Bot {
public:
    std::string name() const override { return "maniac"; }
    Action act(const GameState& g, int s, Mask) override { return Action::raise(g.max_raise_to(s)); }
};

class Nit : public Bot {
public:
    explicit Nit(uint64_t seed) : rng_(seed) {}
    std::string name() const override { return "nit"; }
    Action act(const GameState& g, int s, Mask hole) override {
        const int tc = g.to_call(s);
        if (g.street == PREFLOP) {
            double top = top_frac(hole);
            if (top < 0.05) return Action::raise(3 * std::max(g.bet[1 - s], g.big_blind));
            if (top < 0.12 && g.raises <= 1) return tc > 0 ? Action::call() : Action::check();
            return tc > 0 ? Action::fold() : Action::check();
        }
        double eq = equity_vs_range(hole, any_, g.board, 300, rng_);
        if (eq > 0.8) return Action::raise(pot_bet(g, s, 0.75));
        if (tc > 0) return eq > 0.7 ? Action::call() : Action::fold();
        return Action::check();
    }

private:
    std::mt19937_64 rng_;
    Range any_ = Range::uniform();
};

class Tag : public Bot {
public:
    explicit Tag(uint64_t seed) : rng_(seed) {}
    std::string name() const override { return "tag"; }
    Action act(const GameState& g, int s, Mask hole) override {
        const int tc = g.to_call(s);
        const double pot = g.total_pot();
        if (g.street == PREFLOP) {
            double top = top_frac(hole);
            if (g.raises == 0) {
                if (top < 0.45) return Action::raise(std::max(int(2.5 * g.big_blind), g.bet[1 - s] * 3));
                return tc > 0 ? Action::fold() : Action::check();
            }
            if (top < 0.06) return Action::raise(3 * g.bet[1 - s]);
            if (top < 0.30 && tc < g.stack[s] / 4) return Action::call();
            return Action::fold();
        }
        double eq = equity_vs_range(hole, any_, g.board, 400, rng_);
        if (tc > 0) {
            if (eq > 0.85) return Action::raise(pot_bet(g, s, 0.8));
            return eq > required(pot, tc) + 0.05 ? Action::call() : Action::fold();
        }
        if (eq > 0.62 || (eq < 0.3 && rng_() % 100 < 15)) return Action::raise(pot_bet(g, s, 0.66));
        return Action::check();
    }

private:
    static double required(double pot, double tc) { return tc / (pot + tc); }
    std::mt19937_64 rng_;
    Range any_ = Range::uniform();
};

}  // namespace

std::unique_ptr<Bot> make_bot(const std::string& name, uint64_t seed) {
    if (name == "random") return std::make_unique<RandomBot>(seed);
    if (name == "station") return std::make_unique<Station>();
    if (name == "maniac") return std::make_unique<Maniac>();
    if (name == "nit") return std::make_unique<Nit>(seed);
    if (name == "tag") return std::make_unique<Tag>(seed);
    throw std::invalid_argument("no bot called " + name);
}

}  // namespace poker
