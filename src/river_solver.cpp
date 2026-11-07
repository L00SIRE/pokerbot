#include "river_solver.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace poker {

constexpr int N = NUM_COMBOS;

RiverSolver::RiverSolver(const RiverSpot& spot) : spot_(spot), sd_(spot.board) {
    for (auto& r : spot_.range) r.remove(spot_.board);
    build(spot_.to_act, spot_.bet[0], spot_.bet[1], spot_.raises);
}

int RiverSolver::add_node(Node::Type type, int player, double b0, double b1) {
    Node n;
    n.type = type;
    n.player = player;
    n.bet[0] = b0;
    n.bet[1] = b1;
    nodes_.push_back(std::move(n));
    return int(nodes_.size()) - 1;
}

int RiverSolver::build(int player, double b0, double b1, int raises) {
    const int id = add_node(Node::ACTION, player, b0, b1);
    const double bets[2] = {b0, b1};
    const double mine = bets[player], theirs = bets[1 - player];
    const double pot = spot_.pot + b0 + b1;
    const double cap = spot_.stack;

    std::vector<std::pair<Move, int>> kids;
    std::vector<double> sizes;  // candidate totals for `player` after betting

    if (theirs > mine) {
        kids.push_back({Move::FOLD, add_node(Node::FOLD, player, b0, b1)});
        kids.push_back({Move::CALL, add_node(Node::SHOWDOWN, player, theirs, theirs)});
        for (double f : spot_.raise_sizes) sizes.push_back(theirs + f * (pot + theirs - mine));
    } else {
        // oop checking passes the action, ip checking behind ends the street
        int next = player == 0 ? build(1, b0, b1, raises) : add_node(Node::SHOWDOWN, player, b0, b1);
        kids.push_back({Move::CHECK, next});
        for (double f : spot_.bet_sizes) sizes.push_back(mine + f * pot);
    }

    if (raises < spot_.max_raises && theirs < cap) {
        if (spot_.allin) sizes.push_back(cap);
        std::vector<double> amounts;
        for (double s : sizes) {
            s = std::min(std::round(s), cap);
            if (s <= theirs) continue;
            bool dup = false;
            for (double a : amounts) dup |= std::fabs(a - s) < 0.5;
            if (!dup) amounts.push_back(s);
        }
        std::sort(amounts.begin(), amounts.end());
        for (double s : amounts) {
            double nb[2] = {b0, b1};
            nb[player] = s;
            kids.push_back({Move::BET, build(1 - player, nb[0], nb[1], raises + 1)});
        }
    }

    Node& n = nodes_[id];  // build() may have moved the vector, grab it now
    for (auto& [m, k] : kids) {
        n.moves.push_back(m);
        n.kids.push_back(k);
    }
    const size_t sz = n.kids.size() * N;
    n.regret.assign(sz, 0.0);
    n.avg.assign(sz, 0.0);
    n.cur.assign(sz, 1.0 / n.kids.size());
    n.last.assign(sz, 0.0);
    return id;
}

// regret matching
void RiverSolver::current(const Node& n, std::vector<double>& s) const {
    const int na = n.num_actions();
    s.resize(na * N);
    for (int h = 0; h < N; h++) {
        double sum = 0;
        for (int a = 0; a < na; a++) {
            double r = std::max(n.regret[a * N + h], 0.0);
            s[a * N + h] = r;
            sum += r;
        }
        for (int a = 0; a < na; a++) s[a * N + h] = sum > 0 ? s[a * N + h] / sum : 1.0 / na;
    }
}

// zero sum payoffs, counting the starting pot as half each
void RiverSolver::terminal_values(const Node& n, int me, const double* opp, double* out) const {
    const double half = spot_.pot / 2;
    if (n.type == Node::FOLD) {
        const int folder = n.player;
        double u = me == folder ? -(half + n.bet[me]) : half + n.bet[folder];
        sd_.live_weight(opp, out);
        for (int h = 0; h < N; h++) out[h] *= u;
    } else {
        sd_.values(opp, half + n.bet[0], out);
    }
}

void RiverSolver::walk(int id, int me, const double* my, const double* opp, double* out) {
    Node& n = nodes_[id];
    if (n.type != Node::ACTION) {
        terminal_values(n, me, opp, out);
        return;
    }

    const int na = n.num_actions();
    std::vector<double> s, vals(na * N), reach(N);
    current(n, s);
    std::fill(out, out + N, 0.0);

    if (n.player != me) {
        for (int a = 0; a < na; a++) {
            for (int k = 0; k < N; k++) reach[k] = opp[k] * s[a * N + k];
            walk(n.kids[a], me, my, reach.data(), &vals[a * N]);
            for (int k = 0; k < N; k++) out[k] += vals[a * N + k];
        }
        return;
    }

    for (int a = 0; a < na; a++) {
        for (int h = 0; h < N; h++) reach[h] = my[h] * s[a * N + h];
        walk(n.kids[a], me, reach.data(), opp, &vals[a * N]);
        for (int h = 0; h < N; h++) out[h] += s[a * N + h] * vals[a * N + h];
    }

    // cfr+: floor regrets at zero, linear averaging
    const double t = t_;
    for (int a = 0; a < na; a++) {
        for (int h = 0; h < N; h++) {
            const int i = a * N + h;
            n.regret[i] = std::max(0.0, n.regret[i] + vals[i] - out[h]);
            n.avg[i] += t * my[h] * s[i];
        }
    }
}

void RiverSolver::iterate(int n) {
    std::vector<double> out(N);
    for (int i = 0; i < n; i++) {
        t_++;
        // alternating updates
        for (int p = 0; p < 2; p++) walk(0, p, spot_.range[p].w.data(), spot_.range[1 - p].w.data(), out.data());
    }
}

std::vector<double> RiverSolver::strategy(int id) const {
    const Node& n = nodes_[id];
    const int na = n.num_actions();
    std::vector<double> s(na * N);
    for (int h = 0; h < N; h++) {
        double sum = 0;
        for (int a = 0; a < na; a++) sum += n.avg[a * N + h];
        for (int a = 0; a < na; a++) s[a * N + h] = sum > 0 ? n.avg[a * N + h] / sum : 1.0 / na;
    }
    return s;
}

std::vector<double> RiverSolver::strategy(int id, int hand) const {
    auto s = strategy(id);
    std::vector<double> out(nodes_[id].num_actions());
    for (size_t a = 0; a < out.size(); a++) out[a] = s[a * N + hand];
    return out;
}

void RiverSolver::best_response(int id, int me, const double* opp, double* out) const {
    const Node& n = nodes_[id];
    if (n.type != Node::ACTION) {
        terminal_values(n, me, opp, out);
        return;
    }
    const int na = n.num_actions();
    std::vector<double> vals(na * N);
    if (n.player == me) {
        for (int a = 0; a < na; a++) best_response(n.kids[a], me, opp, &vals[a * N]);
        for (int h = 0; h < N; h++) {
            out[h] = vals[h];
            for (int a = 1; a < na; a++) out[h] = std::max(out[h], vals[a * N + h]);
        }
        return;
    }
    auto s = strategy(id);
    std::vector<double> reach(N);
    std::fill(out, out + N, 0.0);
    for (int a = 0; a < na; a++) {
        for (int k = 0; k < N; k++) reach[k] = opp[k] * s[a * N + k];
        best_response(n.kids[a], me, reach.data(), &vals[a * N]);
        for (int k = 0; k < N; k++) out[k] += vals[a * N + k];
    }
}

double RiverSolver::exploitability() const {
    std::vector<double> out(N), live(N);
    double total = 0;
    for (int p = 0; p < 2; p++) {
        best_response(0, p, spot_.range[1 - p].w.data(), out.data());
        for (int h = 0; h < N; h++) total += spot_.range[p].w[h] * out[h];
    }
    // normalise by the mass of hand pairs that can actually be dealt together
    sd_.live_weight(spot_.range[1].w.data(), live.data());
    double z = 0;
    for (int h = 0; h < N; h++) z += spot_.range[0].w[h] * live[h];
    return z > 0 ? total / z / 2 / spot_.pot * 100 : 0;
}

std::string RiverSolver::describe(int id, int a) const {
    const Node& n = nodes_[id];
    char buf[64];
    switch (n.moves[a]) {
    case Move::FOLD: return "fold";
    case Move::CHECK: return "check";
    case Move::CALL: return "call";
    case Move::BET: {
        double to = nodes_[n.kids[a]].bet[n.player];
        bool raise = n.bet[1 - n.player] > n.bet[n.player];
        if (to >= spot_.stack) std::snprintf(buf, sizeof buf, "allin %.0f", to);
        else std::snprintf(buf, sizeof buf, raise ? "raise to %.0f" : "bet %.0f", to);
        return buf;
    }
    }
    return "?";
}

}  // namespace poker
