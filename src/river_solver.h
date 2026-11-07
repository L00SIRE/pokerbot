#pragma once

#include <string>
#include <vector>

#include "range.h"
#include "showdown.h"

namespace poker {

// CFR_PLUS   Tammelin 2014 (what Cepheus used): regrets floored at 0, linear averaging
// DCFR       Brown & Sandholm 2019, alpha=1.5 beta=0 gamma=2
// PCFR_PLUS  Farina, Kroer & Sandholm 2021: predictive regret matching+ (guess next regret = last one)
// MMD        Sokota et al. 2023, magnetic mirror descent. no averaging, plays the last iterate
enum class Algo { CFR_PLUS, DCFR, PCFR_PLUS, MMD };
const char* algo_name(Algo a);
Algo parse_algo(const std::string& s);  // "cfr+", "dcfr", "pcfr+", "mmd"

// A river spot. Player 0 is out of position (acts first), player 1 in position.
struct RiverSpot {
    Mask board = 0;
    double pot = 100;     // in the middle before any river betting
    double stack = 1000;  // effective stack at the start of the river
    Range range[2];

    // the spot can start mid street, e.g. ip facing a bet
    int to_act = 0;
    double bet[2] = {0, 0};
    int raises = 0;

    std::vector<double> bet_sizes = {0.33, 0.75, 1.5};  // fractions of the pot
    std::vector<double> raise_sizes = {1.0};
    bool allin = true;
    int max_raises = 2;
};

enum class Move { FOLD, CHECK, CALL, BET };

struct Node {
    enum Type { ACTION, FOLD, SHOWDOWN } type = ACTION;
    int player = 0;          // who acts here, or who folded
    double bet[2] = {0, 0};  // river chips in per player
    std::vector<int> kids;
    std::vector<Move> moves;
    // per hand tables, indexed [action * NUM_COMBOS + hand]
    std::vector<double> regret, avg, cur, last, model;
    std::vector<double> magnet;  // mmd reference policy, empty = uniform

    int num_actions() const { return int(kids.size()); }
};

// Vectorised CFR: every iteration pushes the whole 1326 range for both players
// through the tree at once, showdowns use the O(n) trick.
class RiverSolver {
public:
    explicit RiverSolver(const RiverSpot& spot, Algo algo = Algo::DCFR);

    void iterate(int n);
    int iterations() const { return t_; }
    Algo algo() const { return algo_; }

    // the strategy we'd actually play: average for the cfr family, last iterate for mmd
    std::vector<double> strategy(int node) const;
    std::vector<double> strategy(int node, int hand) const;  // action probs for one hand

    // average of both players' best response gains, as % of the starting pot. 0 = nash
    double exploitability() const;

    std::string describe(int node, int action) const;
    const std::vector<Node>& tree() const { return nodes_; }
    std::vector<Node>& tree() { return nodes_; }
    const RiverSpot& spot() const { return spot_; }

    // mmd knobs: step size and how hard the magnet pulls (decays like tau / sqrt(t)).
    // swept these on a top40 vs top40 spot, eta > ~10 starts cycling
    double eta = 5.0, tau = 0.03;

private:
    RiverSpot spot_;
    Algo algo_;
    RiverShowdown sd_;
    std::vector<Node> nodes_;
    int t_ = 0;

    int add_node(Node::Type type, int player, double b0, double b1);
    int build(int player, double b0, double b1, int raises);
    void current(const Node& n, std::vector<double>& s) const;
    void terminal_values(const Node& n, int me, const double* opp, double* out) const;
    void walk(int id, int me, const double* my, const double* opp, double* out);
    void update(Node& n, const std::vector<double>& s, const std::vector<double>& vals,
                const double* out, const double* my, const double* opp);
    void best_response(int id, int me, const double* opp, double* out) const;
};

}  // namespace poker
