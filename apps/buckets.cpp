// cluster every hand on a flop into k buckets by its equity distribution
//   ./buckets --flop Ah7d2c --k 8

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "buckets.h"

using namespace poker;

int main(int argc, char** argv) {
    std::string flop_s = "Jh8h3c";
    int k = 8;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--flop")) flop_s = argv[i + 1];
        else if (!std::strcmp(argv[i], "--k")) k = std::stoi(argv[i + 1]);
    }
    Mask flop;
    try {
        flop = parse_mask(flop_s);
        if (popcount(flop) != 3) throw std::invalid_argument("need 3 cards");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\nusage: %s --flop Jh8h3c --k 8\n", e.what(), argv[0]);
        return 1;
    }

    auto t0 = std::chrono::steady_clock::now();
    auto hist = flop_histograms(flop);
    std::vector<int> live;
    std::vector<Histogram> hs;
    for (int c = 0; c < NUM_COMBOS; c++) {
        if (combos()[c].mask & flop) continue;
        live.push_back(c);
        hs.push_back(hist[c]);
    }
    std::mt19937_64 rng(5);
    auto label = cluster(hs, k, 50, rng);
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::printf("flop %s: %zu hands -> %d buckets in %.2fs\n\n", mask_str(flop).c_str(), live.size(), k, secs);

    struct Info {
        double eq = 0, spread = 0;
        int n = 0;
        std::map<std::string, int> examples;
    };
    std::vector<Info> info(k);
    for (size_t i = 0; i < live.size(); i++) {
        Info& b = info[label[i]];
        double m = mean_equity(hs[i]);
        double var = 0;
        for (int j = 0; j < HIST_BINS; j++) var += hs[i][j] * std::pow((j + 0.5) / HIST_BINS - m, 2);
        b.eq += m, b.spread += std::sqrt(var), b.n++;
        b.examples[mask_str(combos()[live[i]].mask)]++;
    }
    std::vector<int> order(k);
    for (int i = 0; i < k; i++) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return info[a].eq / std::max(1, info[a].n) > info[b].eq / std::max(1, info[b].n);
    });

    // "spread" = std dev of the river equity. same mean, different spread = made hand vs draw
    std::printf("%6s %6s %8s %8s   examples\n", "bucket", "hands", "mean eq", "spread");
    for (int c : order) {
        const Info& b = info[c];
        if (!b.n) continue;
        std::printf("%6d %6d %7.1f%% %7.1f%%  ", c, b.n, 100 * b.eq / b.n, 100 * b.spread / b.n);
        int shown = 0;
        for (auto& [name, cnt] : b.examples) {
            if (shown++ == 8) break;
            std::printf(" %s", name.c_str());
        }
        std::printf("\n");
    }
    return 0;
}
