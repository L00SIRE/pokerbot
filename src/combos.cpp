#include "combos.h"

#include <algorithm>
#include <cctype>
#include <random>

#include "eval.h"

namespace poker {

namespace {

struct ComboTables {
    std::array<Combo, NUM_COMBOS> list{};
    std::array<std::array<int16_t, NUM_CARDS>, NUM_CARDS> index{};
    std::array<int16_t, NUM_COMBOS> cls{};
};

const ComboTables& tables() {
    static const ComboTables t = [] {
        ComboTables t;
        int i = 0;
        for (int a = 0; a < NUM_CARDS; a++) {
            for (int b = a + 1; b < NUM_CARDS; b++) {
                t.list[i] = {uint8_t(a), uint8_t(b), bit(a) | bit(b)};
                t.index[a][b] = t.index[b][a] = int16_t(i);
                t.cls[i] = int16_t(preflop_class(a, b));
                i++;
            }
        }
        return t;
    }();
    return t;
}

struct Preflop {
    std::array<double, NUM_CLASSES> equity{};
    std::array<int, NUM_CLASSES> order{};
    std::array<double, NUM_CLASSES> top_frac{};
};

const Preflop& preflop() {
    static const Preflop p = [] {
        Preflop p;
        std::mt19937_64 rng(1234);
        const int samples = 20000;

        for (int cls = 0; cls < NUM_CLASSES; cls++) {
            int row = cls / 13, col = cls % 13;
            int a, b;
            if (row == col) {
                a = make_card(row, 0), b = make_card(row, 1);
            } else if (row > col) {
                a = make_card(row, 0), b = make_card(col, 0);
            } else {
                a = make_card(col, 0), b = make_card(row, 1);
            }
            Mask hero = bit(a) | bit(b);
            int deck[50], n = 0;
            for (int c = 0; c < NUM_CARDS; c++)
                if (!(hero & bit(c))) deck[n++] = c;

            double score = 0;
            for (int s = 0; s < samples; s++) {
                // partial shuffle: deck[0..1] is villain, deck[2..6] the board
                for (int i = 0; i < 7; i++) std::swap(deck[i], deck[i + rng() % (50 - i)]);
                Mask villain = bit(deck[0]) | bit(deck[1]);
                Mask board = bit(deck[2]) | bit(deck[3]) | bit(deck[4]) | bit(deck[5]) | bit(deck[6]);
                uint32_t h = evaluate(hero | board), v = evaluate(villain | board);
                score += h > v ? 1.0 : h == v ? 0.5 : 0.0;
            }
            p.equity[cls] = score / samples;
        }

        for (int i = 0; i < NUM_CLASSES; i++) p.order[i] = i;
        std::sort(p.order.begin(), p.order.end(),
                  [&](int x, int y) { return p.equity[x] > p.equity[y]; });

        double before = 0;
        for (int cls : p.order) {
            p.top_frac[cls] = (before + class_size(cls) / 2.0) / NUM_COMBOS;
            before += class_size(cls);
        }
        return p;
    }();
    return p;
}

}  // namespace

const std::array<Combo, NUM_COMBOS>& combos() { return tables().list; }

int combo_index(int a, int b) { return tables().index[a][b]; }

int combo_index(Mask two_cards) {
    int a = lowest_card(two_cards);
    int b = lowest_card(two_cards & (two_cards - 1));
    return combo_index(a, b);
}

int preflop_class(int a, int b) {
    int ra = rank_of(a), rb = rank_of(b);
    int hi = std::max(ra, rb), lo = std::min(ra, rb);
    if (suit_of(a) == suit_of(b)) return hi * 13 + lo;
    return lo * 13 + hi;  // offsuit, pairs end up on the diagonal
}

int combo_class(int combo) { return tables().cls[combo]; }

std::string class_name(int cls) {
    int row = cls / 13, col = cls % 13;
    if (row == col) return {rank_char(row), rank_char(row)};
    if (row > col) return {rank_char(row), rank_char(col), 's'};
    return {rank_char(col), rank_char(row), 'o'};
}

int class_from_name(const std::string& name) {
    if (name.size() < 2 || name.size() > 3) return -1;
    int r1 = parse_rank(name[0]), r2 = parse_rank(name[1]);
    if (r1 < 0 || r2 < 0) return -1;
    int hi = std::max(r1, r2), lo = std::min(r1, r2);
    if (hi == lo) return name.size() == 2 ? hi * 13 + hi : -1;
    if (name.size() != 3) return -1;
    char kind = char(std::tolower((unsigned char)name[2]));
    if (kind == 's') return hi * 13 + lo;
    if (kind == 'o') return lo * 13 + hi;
    return -1;
}

int class_size(int cls) {
    int row = cls / 13, col = cls % 13;
    return row == col ? 6 : row > col ? 4 : 12;
}

double preflop_equity(int cls) { return preflop().equity[cls]; }
const std::array<int, NUM_CLASSES>& preflop_order() { return preflop().order; }
double preflop_top_frac(int cls) { return preflop().top_frac[cls]; }

}  // namespace poker
