#include "buckets.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "showdown.h"

namespace poker {

std::vector<Histogram> flop_histograms(Mask flop) {
    std::vector<Histogram> hist(NUM_COMBOS, Histogram{});
    std::vector<double> ones(NUM_COMBOS), diff(NUM_COMBOS), live(NUM_COMBOS);

    // every runout: one showdown sort gives the river equity of all hands at once.
    // equity = (win + tie/2) / live = (live + win - lose) / (2 live)
    for_each_subset(FULL_DECK & ~flop, 2, [&](Mask runout) {
        RiverShowdown sd(flop | runout);
        std::fill(ones.begin(), ones.end(), 0.0);
        for (int k : sd.order) ones[k] = 1.0;
        sd.values(ones.data(), 1.0, diff.data());
        sd.live_weight(ones.data(), live.data());
        for (int k : sd.order) {
            double eq = (live[k] + diff[k]) / (2 * live[k]);
            hist[k][std::min(HIST_BINS - 1, int(eq * HIST_BINS))] += 1;
        }
    });

    for (auto& h : hist) {
        double t = 0;
        for (double x : h) t += x;
        if (t > 0)
            for (double& x : h) x /= t;
    }
    return hist;
}

double emd(const Histogram& a, const Histogram& b) {
    double ca = 0, cb = 0, d = 0;
    for (int i = 0; i < HIST_BINS; i++) {
        ca += a[i], cb += b[i];
        d += std::fabs(ca - cb);
    }
    return d / HIST_BINS;
}

double mean_equity(const Histogram& h) {
    double m = 0;
    for (int i = 0; i < HIST_BINS; i++) m += h[i] * (i + 0.5) / HIST_BINS;
    return m;
}

std::vector<int> cluster(const std::vector<Histogram>& hs, int k, int iters, std::mt19937_64& rng) {
    const int n = int(hs.size());
    k = std::min(k, n);
    std::vector<Histogram> centers;
    std::vector<double> best(n, std::numeric_limits<double>::max());

    // k-means++: next center picked proportional to distance^2 from the closest one so far
    centers.push_back(hs[rng() % n]);
    while (int(centers.size()) < k) {
        double total = 0;
        for (int i = 0; i < n; i++) {
            best[i] = std::min(best[i], emd(hs[i], centers.back()));
            total += best[i] * best[i];
        }
        double u = std::uniform_real_distribution<double>(0, total)(rng);
        int pick = n - 1;
        for (int i = 0; i < n; i++) {
            u -= best[i] * best[i];
            if (u <= 0) {
                pick = i;
                break;
            }
        }
        centers.push_back(hs[pick]);
    }

    std::vector<int> label(n, 0);
    for (int it = 0; it < iters; it++) {
        bool moved = false;
        for (int i = 0; i < n; i++) {
            int arg = 0;
            double d = emd(hs[i], centers[0]);
            for (int c = 1; c < k; c++) {
                double dc = emd(hs[i], centers[c]);
                if (dc < d) d = dc, arg = c;
            }
            moved |= arg != label[i];
            label[i] = arg;
        }
        if (!moved && it > 0) break;

        // mean histogram as the center (same as the potential-aware abstraction papers)
        std::vector<Histogram> sum(k, Histogram{});
        std::vector<int> count(k, 0);
        for (int i = 0; i < n; i++) {
            for (int b = 0; b < HIST_BINS; b++) sum[label[i]][b] += hs[i][b];
            count[label[i]]++;
        }
        for (int c = 0; c < k; c++) {
            if (!count[c]) continue;  // empty cluster, keep the old center
            for (int b = 0; b < HIST_BINS; b++) centers[c][b] = sum[c][b] / count[c];
        }
    }
    return label;
}

}  // namespace poker
