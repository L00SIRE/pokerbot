#include <cstring>

#include "check.h"

// ./tests            run everything
// ./tests solver     only tests with "solver" in the name
int main(int argc, char** argv) {
    int ran = 0;
    for (auto& t : all_tests()) {
        if (argc > 1 && !std::strstr(t.name, argv[1])) continue;
        int before = failures();
        t.fn();
        std::printf("%s %s\n", failures() == before ? "ok  " : "FAIL", t.name);
        ran++;
    }
    std::printf("\n%d tests, %d failed checks\n", ran, failures());
    return failures() ? 1 : 0;
}
