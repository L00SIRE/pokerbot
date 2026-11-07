#pragma once

#include <string>
#include <vector>

#include "range.h"
#include "showdown.h"

namespace poker {

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

    int num_actions() const { return int(kids.size()); }
};

// Vectorised CFR: every iteration pushes the whole 1326 range for both players
// through the tree at once, showdowns use the O(n) trick.
class RiverSolver {
public:
    explicit RiverSolver(const RiverSpot& spot);

    void iterate(int n);
    int iterations() const { return t_; }

    std::vector<double> strategy(int node) const;            // average strategy
    std::vector<double> strategy(int node, int hand) const;  // action probs for one hand

    // average of both players' best response gains, as % of the starting pot. 0 = nash
    double exploitability() const;

    std::string describe(int node, int action) const;
    const std::vector<Node>& tree() const { return nodes_; }
    const RiverSpot& spot() const { return spot_; }

private:
    RiverSpot spot_;
    RiverShowdown sd_;
    std::vector<Node> nodes_;
    int t_ = 0;

    int add_node(Node::Type type, int player, double b0, double b1);
    int build(int player, double b0, double b1, int raises);
    void current(const Node& n, std::vector<double>& s) const;
    void terminal_values(const Node& n, int me, const double* opp, double* out) const;
    void walk(int id, int me, const double* my, const double* opp, double* out);
    void best_response(int id, int me, const double* opp, double* out) const;
};

}  // namespace poker
