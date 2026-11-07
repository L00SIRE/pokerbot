#include "range.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace poker {

Range Range::uniform() {
    Range r;
    r.w.fill(1.0);
    return r;
}

Range Range::top(double frac) {
    Range r;
    bool want[NUM_CLASSES] = {};
    double taken = 0;
    for (int cls : preflop_order()) {
        if (taken >= frac * NUM_COMBOS) break;
        want[cls] = true;
        taken += class_size(cls);
    }
    for (int k = 0; k < NUM_COMBOS; k++)
        if (want[combo_class(k)]) r.w[k] = 1.0;
    return r;
}

static void set_class(Range& r, int cls, double weight) {
    for (int k = 0; k < NUM_COMBOS; k++)
        if (combo_class(k) == cls) r.w[k] = weight;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

Range Range::parse(const std::string& text) {
    Range r;
    std::stringstream ss(text);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        tok = trim(tok);
        if (tok.empty()) continue;

        double weight = 1.0;
        if (auto colon = tok.find(':'); colon != std::string::npos) {
            weight = std::stod(tok.substr(colon + 1));
            tok = tok.substr(0, colon);
        }

        if (tok == "random" || tok == "any" || tok == "*") {
            r.w.fill(weight);
            continue;
        }
        if (tok.rfind("top", 0) == 0) {
            Range t = top(std::stod(tok.substr(3)) / 100.0);
            for (int k = 0; k < NUM_COMBOS; k++)
                if (t.w[k] > 0) r.w[k] = weight;
            continue;
        }
        if (tok.size() == 4 && parse_card(tok.substr(0, 2)) >= 0 && parse_card(tok.substr(2, 2)) >= 0) {
            int a = parse_card(tok.substr(0, 2)), b = parse_card(tok.substr(2, 2));
            if (a == b) throw std::invalid_argument("same card twice: " + tok);
            r.w[combo_index(a, b)] = weight;
            continue;
        }

        bool plus = tok.back() == '+';
        if (plus) tok.pop_back();
        if (tok.size() < 2 || tok.size() > 3) throw std::invalid_argument("bad range token: " + tok);
        int hi = parse_rank(tok[0]), lo = parse_rank(tok[1]);
        if (hi < 0 || lo < 0) throw std::invalid_argument("bad range token: " + tok);
        if (hi < lo) std::swap(hi, lo);
        char kind = tok.size() == 3 ? char(std::tolower((unsigned char)tok[2])) : 'x';
        if (kind != 's' && kind != 'o' && kind != 'x') throw std::invalid_argument("bad range token: " + tok);

        auto add = [&](int h, int l) {
            if (h == l) {
                set_class(r, h * 13 + h, weight);
                return;
            }
            if (kind != 'o') set_class(r, h * 13 + l, weight);
            if (kind != 's') set_class(r, l * 13 + h, weight);
        };
        if (hi == lo) {
            for (int x = hi; x <= (plus ? 12 : hi); x++) add(x, x);  // TT+ = TT..AA
        } else {
            for (int x = lo; x <= (plus ? hi - 1 : lo); x++) add(hi, x);  // ATs+ = ATs..AKs
        }
    }
    return r;
}

void Range::remove(Mask dead) {
    for (int k = 0; k < NUM_COMBOS; k++)
        if (combos()[k].mask & dead) w[k] = 0;
}

double Range::total() const {
    double t = 0;
    for (double x : w) t += x;
    return t;
}

int Range::live() const {
    return int(std::count_if(w.begin(), w.end(), [](double x) { return x > 0; }));
}

void Range::normalize() {
    double mx = *std::max_element(w.begin(), w.end());
    if (mx <= 0) return;
    for (double& x : w) x /= mx;
}

int Range::sample(std::mt19937_64& rng) const {
    double t = total();
    if (t <= 0) return -1;
    double u = std::uniform_real_distribution<double>(0, t)(rng);
    for (int k = 0; k < NUM_COMBOS; k++) {
        u -= w[k];
        if (u <= 0 && w[k] > 0) return k;
    }
    for (int k = NUM_COMBOS - 1; k >= 0; k--)  // rounding, just take the last live one
        if (w[k] > 0) return k;
    return -1;
}

}  // namespace poker
