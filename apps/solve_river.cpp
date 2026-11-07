// river solver cli
//   ./solve_river --board Ks7d2c9hTs --oop top40 --ip top30 --iters 500
//   ./solve_river --bench          compares cfr+ / dcfr / pcfr+ / mmd on the same spot

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>
#include <string>

#include "river_solver.h"

using namespace poker;
using Clock = std::chrono::steady_clock;

static std::vector<double> parse_list(const std::string& s) {
    std::vector<double> v;
    std::stringstream ss(s);
    std::string x;
    while (std::getline(ss, x, ',')) v.push_back(std::stod(x));
    return v;
}

static void bench(const RiverSpot& spot, int max_iters) {
    const int checkpoints[] = {10, 25, 50, 100, 200, 500, 1000, 2000, 5000};
    const Algo algos[] = {Algo::CFR_PLUS, Algo::DCFR, Algo::PCFR_PLUS, Algo::MMD};

    std::printf("exploitability in %% of the pot\n\n%8s", "iters");
    for (Algo a : algos) std::printf(" %10s", algo_name(a));
    std::printf("\n");

    std::map<Algo, std::vector<double>> rows;
    std::map<Algo, double> secs;
    for (Algo a : algos) {
        RiverSolver sv(spot, a);
        double solve_time = 0;  // don't count the exploitability checks
        for (int c : checkpoints) {
            if (c > max_iters) break;
            auto s0 = Clock::now();
            sv.iterate(c - sv.iterations());
            solve_time += std::chrono::duration<double>(Clock::now() - s0).count();
            rows[a].push_back(sv.exploitability());
        }
        secs[a] = solve_time;
    }
    for (size_t i = 0; i < rows[algos[0]].size(); i++) {
        std::printf("%8d", checkpoints[i]);
        for (Algo a : algos) std::printf(" %10.4f", rows[a][i]);
        std::printf("\n");
    }
    std::printf("%8s", "secs");
    for (Algo a : algos) std::printf(" %10.2f", secs[a]);
    std::printf("\n");
}

int main(int argc, char** argv) {
    RiverSpot spot;
    std::string board = "Ks7d2c9hTs", oop = "top40", ip = "top40";
    int iters = 500;
    Algo algo = Algo::DCFR;  // fastest on --bench
    bool do_bench = false;

    try {
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];
            auto next = [&]() -> std::string {
                if (i + 1 >= argc) throw std::invalid_argument("missing value for " + a);
                return argv[++i];
            };
            if (a == "--board") board = next();
            else if (a == "--pot") spot.pot = std::stod(next());
            else if (a == "--stack") spot.stack = std::stod(next());
            else if (a == "--oop") oop = next();
            else if (a == "--ip") ip = next();
            else if (a == "--sizes") spot.bet_sizes = parse_list(next());
            else if (a == "--raises") spot.raise_sizes = parse_list(next());
            else if (a == "--iters") iters = std::stoi(next());
            else if (a == "--algo") algo = parse_algo(next());
            else if (a == "--bench") do_bench = true;
            else throw std::invalid_argument("unknown flag " + a);
        }
        spot.board = parse_mask(board);
        if (popcount(spot.board) != 5) throw std::invalid_argument("board needs 5 cards");
        spot.range[0] = Range::parse(oop);
        spot.range[1] = Range::parse(ip);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        std::fprintf(stderr,
                     "usage: %s [--board CARDS] [--pot P] [--stack S] [--oop RANGE] [--ip RANGE]\n"
                     "          [--sizes 0.33,0.75,1.5] [--raises 1.0] [--iters N] [--algo dcfr|cfr+|pcfr+|mmd] [--bench]\n",
                     argv[0]);
        return 1;
    }

    {
        RiverSolver probe(spot);
        Range r0 = spot.range[0], r1 = spot.range[1];
        r0.remove(spot.board), r1.remove(spot.board);
        std::printf("board %s  pot %.0f  stack %.0f  tree %zu nodes\n", mask_str(spot.board).c_str(),
                    spot.pot, spot.stack, probe.tree().size());
        std::printf("oop \"%s\" %d combos, ip \"%s\" %d combos\n\n", oop.c_str(), r0.live(), ip.c_str(), r1.live());
    }

    if (do_bench) {
        bench(spot, std::max(iters, 1000));
        return 0;
    }

    RiverSolver sv(spot, algo);
    auto t0 = Clock::now();
    sv.iterate(iters);
    double secs = std::chrono::duration<double>(Clock::now() - t0).count();
    std::printf("%s, %d iterations in %.2fs, exploitability %.3f%% of the pot\n\n", algo_name(algo), iters, secs,
                sv.exploitability());

    // root strategy, overall and per hand class (strongest on this board first)
    const Node& root = sv.tree()[0];
    const Range& r = sv.spot().range[root.player];
    const auto s = sv.strategy(0);
    const int na = root.num_actions();
    RiverShowdown sd(spot.board);

    std::map<int, std::pair<double, std::vector<double>>> by_class;  // cls -> (weight, action mass)
    std::map<int, double> cls_strength;
    std::vector<double> overall(na);
    double total = 0;
    for (int h = 0; h < NUM_COMBOS; h++) {
        if (r.w[h] <= 0) continue;
        auto& [w, mass] = by_class[combo_class(h)];
        mass.resize(na);
        w += r.w[h];
        cls_strength[combo_class(h)] += r.w[h] * sd.strength[h];
        total += r.w[h];
        for (int a = 0; a < na; a++) {
            mass[a] += r.w[h] * s[a * NUM_COMBOS + h];
            overall[a] += r.w[h] * s[a * NUM_COMBOS + h];
        }
    }

    std::printf("%s to act. overall:", root.player == 0 ? "oop" : "ip");
    for (int a = 0; a < na; a++) std::printf("  %s %.1f%%", sv.describe(0, a).c_str(), 100 * overall[a] / total);
    std::printf("\n\n%-6s", "hand");
    for (int a = 0; a < na; a++) std::printf(" %12s", sv.describe(0, a).c_str());
    std::printf("\n");

    std::vector<int> classes;
    for (auto& [cls, v] : by_class) classes.push_back(cls);
    std::sort(classes.begin(), classes.end(), [&](int x, int y) {
        return cls_strength[x] / by_class[x].first > cls_strength[y] / by_class[y].first;
    });
    for (int cls : classes) {
        auto& [w, mass] = by_class[cls];
        std::printf("%-6s", class_name(cls).c_str());
        for (int a = 0; a < na; a++) std::printf(" %11.0f%%", 100 * mass[a] / w);
        std::printf("\n");
    }
    return 0;
}
