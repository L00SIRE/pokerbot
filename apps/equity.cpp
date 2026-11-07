// quick equity calculator
//   ./equity AhKh QsJs --board Qh7h2c
//   ./equity AhKh "QQ+,AK,top5" --board Qh7h2c

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#include "equity.h"
#include "eval.h"

using namespace poker;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s HERO VILLAIN|RANGE [--board CARDS] [--samples N]\n", argv[0]);
        return 1;
    }
    std::string hero_s = argv[1], villain_s = argv[2], board_s;
    int samples = 200000;
    for (int i = 3; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--board")) board_s = argv[i + 1];
        else if (!std::strcmp(argv[i], "--samples")) samples = std::stoi(argv[i + 1]);
    }

    try {
        Mask hero = parse_mask(hero_s);
        Mask board = board_s.empty() ? 0 : parse_mask(board_s);
        if (popcount(hero) != 2 || popcount(board) > 5 || (hero & board)) throw std::invalid_argument("bad hand/board");

        // villain is either two cards or a range
        Mask villain = 0;
        try {
            villain = parse_mask(villain_s);
            if (popcount(villain) != 2) villain = 0;
        } catch (const std::invalid_argument&) {
        }

        std::printf("hero %s  board %s\n", mask_str(hero).c_str(), board ? mask_str(board).c_str() : "-");
        if (villain) {
            if (villain & (hero | board)) throw std::invalid_argument("cards overlap");
            std::printf("vs %s\n", mask_str(villain).c_str());
            if (popcount(board) >= 3) {
                std::printf("hero has:    %s\n", category_name(category(evaluate(hero | board))));
                std::printf("villain has: %s\n", category_name(category(evaluate(villain | board))));
            }
            if (popcount(board) == 3 || popcount(board) == 4) {
                int outs = count_outs(hero, villain, board);
                int unseen = 52 - 4 - popcount(board);
                std::printf("outs: %d  -> next card %.1f%%", outs, 100 * hit_next(outs, unseen));
                if (popcount(board) == 3) std::printf(", by the river %.1f%%", 100 * hit_by_river(outs, unseen));
                std::printf("  (rule of 4/2 says %d%%)\n", outs * (popcount(board) == 3 ? 4 : 2));
            }
            std::printf("equity (exact): %.2f%%\n", 100 * equity_exact(hero, villain, board));
        } else {
            Range r = Range::parse(villain_s);
            std::mt19937_64 rng(42);
            r.remove(hero | board);
            std::printf("vs range \"%s\" (%d live combos)\n", villain_s.c_str(), r.live());
            std::printf("equity: %.2f%%%s\n", 100 * equity_vs_range(hero, r, board, samples, rng),
                        popcount(board) >= 4 ? " (exact)" : " (monte carlo)");
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
