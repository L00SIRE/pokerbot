// Goes through every 7 card hand (52 choose 7 = 133,784,560) and checks the
// category counts against the known Hold'em numbers. Also counts how many of
// the 7462 hand classes can actually show up as a best-of-7 (should be 4824).

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "eval.h"

using namespace poker;

static const uint64_t EXPECTED[9] = {23294460, 58627800, 31433400, 6461620, 6180020,
                                     4047644,  3473184,  224848,   41584};

// low 26 bits of a score never go past 2^21, so this packs it into ~19M slots
static inline uint32_t key(uint32_t score) { return ((score >> 26) << 21) | (score & 0x1FFFFF); }

struct Worker {
    std::array<uint64_t, 9> counts{};
    std::vector<bool> seen = std::vector<bool>(9u << 21);
};

static void hands_starting_with(int a, Worker& w) {
    for (int b = a + 1; b < 52; b++) {
        Mask mb = bit(a) | bit(b);
        for (int c = b + 1; c < 52; c++) {
            Mask mc = mb | bit(c);
            for (int d = c + 1; d < 52; d++) {
                Mask md = mc | bit(d);
                for (int e = d + 1; e < 52; e++) {
                    Mask me = md | bit(e);
                    for (int f = e + 1; f < 52; f++) {
                        Mask mf = me | bit(f);
                        for (int g = f + 1; g < 52; g++) {
                            uint32_t s = evaluate(mf | bit(g));
                            w.counts[s >> 26]++;
                            w.seen[key(s)] = true;
                        }
                    }
                }
            }
        }
    }
}

int main() {
    auto t0 = std::chrono::steady_clock::now();

    unsigned n_threads = std::max(1u, std::thread::hardware_concurrency());
    std::vector<Worker> workers(n_threads);
    std::atomic<int> next{0};
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < n_threads; i++) {
        threads.emplace_back([&, i] {
            for (int a; (a = next++) < 46;) hands_starting_with(a, workers[i]);
        });
    }
    for (auto& t : threads) t.join();

    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::array<uint64_t, 9> total{};
    std::vector<bool> seen(9u << 21);
    for (auto& w : workers) {
        for (int i = 0; i < 9; i++) total[i] += w.counts[i];
        for (size_t k = 0; k < seen.size(); k++)
            if (w.seen[k]) seen[k] = true;
    }

    bool ok = true;
    uint64_t all = 0;
    std::printf("%-17s %12s %12s %9s\n", "category", "count", "expected", "prob");
    for (int i = 8; i >= 0; i--) {
        all += total[i];
        bool good = total[i] == EXPECTED[i];
        ok &= good;
        std::printf("%-17s %12llu %12llu %8.4f%% %s\n", category_name(Category(i)),
                    (unsigned long long)total[i], (unsigned long long)EXPECTED[i],
                    100.0 * total[i] / 133784560.0, good ? "" : "  <-- MISMATCH");
    }

    size_t distinct = 0;
    for (bool b : seen) distinct += b;

    std::printf("\ntotal %llu hands in %.2fs on %u threads (%.0fM hands/s)\n",
                (unsigned long long)all, secs, n_threads, all / secs / 1e6);
    std::printf("distinct best-of-7 classes: %zu (expected 4824)\n", distinct);
    ok &= all == 133784560 && distinct == 4824;
    std::printf("%s\n", ok ? "all good" : "SOMETHING IS OFF");
    return ok ? 0 : 1;
}
