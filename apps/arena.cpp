// bully vs the sparring bots, duplicate format (every deal played twice with
// seats swapped, so card luck mostly cancels out)
//   ./arena                      everyone, 1000 deals each
//   ./arena --opp maniac --deals 3000
//   ./arena --opp bully          mirror match
//   ./arena --no-solver          heuristics on the river too (much faster)
//   ./arena --pmax 0             never trust the read, nash-ish everywhere

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "bots.h"
#include "bully.h"

using namespace poker;

struct Result {
    double bb100, stderr_bb100, secs;
};

static Result run(Bot& hero, Bot& villain, int deals, uint64_t seed) {
    Table t;
    std::mt19937_64 rng(seed);
    std::vector<double> pair;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < deals; i++) {
        auto c = Table::deal(rng);
        HandResult a = t.play(hero, villain, i % 2, c);
        HandResult b = t.play(villain, hero, i % 2, c);
        pair.push_back((a.net[0] + b.net[1]) / 2.0);  // per hand
    }
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    double mean = 0, var = 0;
    for (double x : pair) mean += x;
    mean /= pair.size();
    for (double x : pair) var += (x - mean) * (x - mean);
    var /= std::max<size_t>(1, pair.size() - 1);
    const double bb = t.big_blind();
    return {100 * mean / bb, 100 * std::sqrt(var / pair.size()) / bb, secs};
}

int main(int argc, char** argv) {
    std::vector<std::string> opps(std::begin(BOT_NAMES), std::end(BOT_NAMES));
    int deals = 1000;
    uint64_t seed = 1;
    BullyConfig cfg;
    bool quiet = false;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--opp" && i + 1 < argc) opps = {argv[++i]};
        else if (a == "--deals" && i + 1 < argc) deals = std::stoi(argv[++i]);
        else if (a == "--seed" && i + 1 < argc) seed = std::stoull(argv[++i]);
        else if (a == "--iters" && i + 1 < argc) cfg.solver_iters = std::stoi(argv[++i]);
        else if (a == "--pmax" && i + 1 < argc) cfg.p_max = std::stod(argv[++i]);  // 0 = never exploit
        else if (a == "--no-solver") cfg.use_solver = false;
        else if (a == "--no-probe") cfg.probe = false;
        else if (a == "--decoy") cfg.decoy = true;
        else if (a == "--quiet") quiet = true;
        else {
            std::fprintf(stderr,
                         "usage: %s [--opp NAME|bully] [--deals N] [--seed S] [--iters N] [--pmax P] "
                         "[--no-solver] [--no-probe] [--decoy] [--quiet]\n",
                         argv[0]);
            return 1;
        }
    }

    std::printf("%d deals per match (x2 hands, duplicate), river solver %s\n\n", deals,
                cfg.use_solver ? "on" : "off");
    std::printf("%-10s %10s %8s %8s\n", "opponent", "bb/100", "+-", "secs");
    for (const auto& name : opps) {
        BullyBot bully(cfg);
        std::unique_ptr<Bot> villain;
        try {
            if (name == "bully") {
                BullyConfig other = cfg;
                other.seed = cfg.seed + 1000;
                villain = std::make_unique<BullyBot>(other);
            } else {
                villain = make_bot(name, seed + 99);
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "error: %s\n", e.what());
            return 1;
        }
        Result r = run(bully, *villain, deals, seed);
        std::printf("%-10s %+10.1f %8.1f %8.1f\n", name.c_str(), r.bb100, 1.96 * r.stderr_bb100, r.secs);
        if (!quiet) std::printf("%s\n", bully.report().c_str());
        std::fflush(stdout);
    }
    return 0;
}
