#pragma once

#include <array>
#include <random>
#include <vector>

#include "combos.h"

namespace poker {

// Potential-aware card abstraction. Instead of bucketing flop hands by their equity
// (which lumps a fragile pair in with a big draw that has the same number), describe
// each hand by the histogram of its river equity over every turn+river, and cluster
// those histograms with earth mover's distance.

constexpr int HIST_BINS = 50;
using Histogram = std::array<double, HIST_BINS>;

// one histogram per combo (equity vs a random hand), hands touching the flop are all zero
std::vector<Histogram> flop_histograms(Mask flop);

// 1d wasserstein: sum of |cdf_a - cdf_b|, in equity units (0..1)
double emd(const Histogram& a, const Histogram& b);
double mean_equity(const Histogram& h);

// k-means with emd as the distance and k-means++ seeding. returns a bucket per input
std::vector<int> cluster(const std::vector<Histogram>& hs, int k, int iters, std::mt19937_64& rng);

}  // namespace poker
