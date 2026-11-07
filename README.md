# pokerbot

Heads up no limit hold'em bot in C++20, written from scratch. No dependencies besides the standard library and CMake.

This started as a pile of research notes on how the big poker AIs work (Cepheus, Libratus, Pluribus, DeepStack and friends) plus a bunch of math for exploiting bad players. I wanted to actually build the pieces instead of just reading about them, and check whether the math in my notes holds up when you run it. Most of it did. A few formulas didn't, those are listed near the bottom.

The goal of the bot is to play close to Nash by default and then lean on opponents harder the more evidence it has that they're leaking. Basically a bully that can't easily be bullied back.

## Layout

```
src/
  card, eval      64 bit card masks, 7 card evaluator (bit tricks + a straight table, no sorting)
  combos, range   the 1326 two card combos, 169 preflop classes, weighted ranges with card removal
  equity          outs math, exact enumeration, monte carlo vs a range
  showdown        O(n) range vs range river showdown (prefix sums + inclusion-exclusion)
  river_solver    vectorized CFR over full ranges: CFR+, DCFR, PCFR+, MMD, restricted nash response
  buckets         potential-aware (EMD) bucketing of flop hands
  exploit.h       MDF, bluff ratios, geometric sizing, action translation
  opponent        beta-binomial stats, KL leak finder, archetype model + info gain, bot detector
  tracker         turns table events into those stats
  game, bots      heads up engine and some sparring bots
  bully           the actual bot
apps/             verify_hands, equity, solve_river, buckets, arena
tests/            ~40 tests with a tiny homemade harness
```

## Building

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/tests
```

I've only built it on macOS with Apple clang 17. It uses `__builtin_popcountll` and friends, so it wants gcc or clang.

## Cards and hand evaluation

A card set is a `uint64_t`. Card ids are suit-major (`suit * 13 + rank`) so each suit is its own 13 bit block. The evaluator splits out the four suit masks, then gets pairs, trips and quads with ANDs/ORs between them, and uses an 8192 entry table for straights. It returns one `uint32_t` where bigger is better and equal is a chop.

`verify_hands` walks all 133,784,560 seven card hands and checks the counts:

```
category                 count     expected      prob
Straight flush           41584        41584   0.0311%
Four of a kind          224848       224848   0.1681%
Full house             3473184      3473184   2.5961%
Flush                  4047644      4047644   3.0255%
Straight               6180020      6180020   4.6194%
Three of a kind        6461620      6461620   4.8299%
Two pair              31433400     31433400  23.4955%
One pair              58627800     58627800  43.8225%
High card             23294460     23294460  17.4119%

total 133784560 hands in 0.40s on 8 threads (333M hands/s)
distinct best-of-7 classes: 4824 (expected 4824)
```

Every number matches. The tests also check the 5 card distribution and that there are exactly 7462 distinct 5 card hands. Fun fact I didn't know going in: only 4824 of those 7462 can ever be someone's best hand with 7 cards.

The equity tool does hand vs hand (exact) or hand vs range:

```
$ ./build/equity AhKh QsJs --board Qh7h2c
hero AhKh  board Qh7h2c
vs QsJs
hero has:    High card
villain has: One pair
outs: 15  -> next card 33.3%, by the river 56.1%  (rule of 4/2 says 60%)
equity (exact): 53.43%

$ ./build/equity AhKh "QQ+,AK,top5" --board Qh7h2c
vs range "QQ+,AK,top5" (48 live combos)
equity: 50.84% (monte carlo)
```

Ranges take `QQ+`, `ATs+`, `KQ`, `AKo`, `AsKs`, `top20`, `random`, and weights like `AA:0.5`.

## O(n) showdown

A river solver needs "how much of the opponent's range do I beat" for all 1326 hands at once, at every terminal node, every iteration. Doing it pairwise is 1326 x 1326 per node. Instead, sort the hands by strength and walk up keeping a running total S of opponent weight plus C[c], the part of S that touches card c. For a hand {a, b} the weight it beats is S - C[a] - C[b]. The term for both cards is the hand itself, which is never strictly weaker. Do the same walking down for losses. Ties get handled by finishing a whole strength group before adding it to the sums. The test compares it against the n^2 loop on random boards and ranges and they agree to 1e-8.

## River solver

`RiverSolver` takes a board, pot, effective stack, both ranges and a bet tree (pot fraction bet sizes, raise sizes, all in, max raises). It can start mid street too, like facing a bet, which is what the bot needs. Every iteration pushes the full range for both players through the tree (vectorized CFR, alternating updates). Exploitability comes from a proper best response.

I implemented four update rules and benched them on the same spot (`./build/solve_river --bench`, board KsTs9h7d2c, top 40% vs top 40%, bets of 33/75/150% pot plus all in, 63 nodes):

```
exploitability in % of the pot

   iters       cfr+       dcfr      pcfr+        mmd
      10    18.0803    23.9320    14.2984    16.3215
      25     7.5570     7.3456     8.1696     9.9604
      50     3.5969     2.2820     4.3199     6.0725
     100     1.6595     0.5716     1.7598     1.8220
     200     0.6473     0.1967     0.6339     0.7787
     500     0.1922     0.0408     0.2036     0.4090
    1000     0.0667     0.0115     0.0640     0.2535
    2000     0.0197     0.0041     0.0205     0.1530
    secs       1.91       1.69       1.61       3.19
```

DCFR (Brown & Sandholm 2019, alpha 1.5, beta 0, gamma 2) wins pretty clearly. It's about 5x less exploitable than CFR+ at the same iteration count, so it's the default.

PCFR+ (predictive regret matching+, Farina et al. 2021) basically tied CFR+ here. That matches their own paper, where DCFR usually comes out ahead on poker-like games, so I wouldn't call it an upgrade for poker specifically.

MMD (Sokota et al. 2023) converges but slower. I had to sweep eta/tau for it (anything above eta ~10 and the last iterate starts cycling). Speed isn't the point of it. What it gets you is a usable last iterate and a "magnet" policy you can point somewhere.

For a sanity check with a known answer, the tests have a toy spot: a polarized range (sets + air) vs a pure bluff catcher with one pot sized bet. Theory says the caller defends MDF = 50%, and bluffs should be B/(P+2B) = 1/3 of the betting range, which is 1.5 of the 16 air combos. After 1000 iterations the solver has the call at 0.5002 and the bluff rate at 0.0936 (theory 0.09375).

Inside the bot, a river solve on a smaller tree (half pot / pot bets, one raise size) is 45 nodes. 60 DCFR iterations take about 35ms and land within ~2.4% of the pot of Nash.

### Restricted nash response

`set_exploit(who, p)` makes the opponent a mix. With probability p they play a fixed model of how we think they play, otherwise they're a "nemesis" that learns the best counter to us. We can't tell which one we're facing. p = 0 is Nash, p = 1 is a pure best response to the model.

On the toy spot, with the model being "always folds to a bet", the solver stays exactly at the Nash bluff rate until p > 0.5 and then bluffs everything. That looked like a bug at first, but it's right: an extra bluff wins +100 against the folder and loses -100 against a caller, so it's only worth it once p is over half. Below that the nemesis just calls more to cover. That's the safety property you want.

## Buckets (EMD)

For every hand on a flop, `flop_histograms` builds a histogram of its river equity over all 1176 turn+river runouts. Since one showdown sort gives every hand's equity on a runout at once, the whole flop takes about 60ms. `cluster` is k-means with earth mover's distance (k-means++ seeding, mean histogram centers like the potential-aware abstraction papers).

```
$ ./build/buckets --flop Jh8h3c --k 10
bucket  hands  mean eq   spread   examples
     3     51    87.4%     9.6%   3h3d 3s3d 3s3h 8c3d 8c3h ...
     5    123    78.7%    10.6%   AcJc AcJd AcJs AdJc ...
     ...
     0    155    56.1%    19.1%   3d2c 3d2d 3d2h 3d2s ...
     2     85    53.9%    33.4%   4h2h 5h2h 5h4h 6h2h ...
```

Buckets 0 and 2 have about the same mean equity, but one is weak made hands and the other is flush draws (look at the spread). Bucketing on plain equity would merge them. That's the whole reason to use EMD. Nothing consumes these buckets yet. They're the first step toward a blueprint (see todo).

## Opponent modelling

`Profile` keeps a beta-binomial counter per stat (VPIP, PFR, 3bet, fold to 3bet, cbet and fold-to-cbet per street, check raise, WTSD, WWSF, shove %, bet % and fold-to-bet % per street). The priors are a rough heads up reg, worth 8 observations, so a few hands don't swing anything to 100%.

On top of that:

- `leaks()` ranks stats by n * KL(observed || baseline).
- `maniac()` flags them when the posterior shove rate is more than 2 sd above baseline. Shoving every hand gets flagged after 4 hands.
- `ArchetypeModel` keeps a posterior over nit / station / maniac / reg from how they answer our bets, and scores each bet size by expected information gain in bits. Early in a match the bot picks sizes by EV + beta * info gain, with beta decaying over the first ~25 hands.
- `BotDetector` scores how script-like they are from decision latency (fast, low variance in log time) and bet size entropy.

## The bot

`BullyBot` keeps a 1326 weight range for both seats and narrows them after every action with a likelihood based on that player's stats. Then:

- Preflop it's equity vs their estimated range against pot odds. Against a shover their range gets wide fast, so it starts snapping off with hands like A7o and K9s, which is the "maniac trap" from my notes. It opens wider vs big blinds that fold a lot and 3bets light vs players who fold to 3bets more than breakeven.
- Flop and turn are equity vs range. When facing a bet it defends the top MDF of its own range, but that floor shrinks by (1 - p) as the read gets trustworthy. Bluffs come from weak hands, with frequency scaled by a blocker score and pushed up when their fold rate beats breakeven (or cut when they're a station). Against a capped range it bets geometric sizes so the stacks go in by the river.
- On the river it builds a `RiverSpot` from the current state and both ranges, fills in a model of the opponent from their river stats, solves with DCFR + restricted nash response, and samples its hand's action.

How much it trusts the read is p(n) = 0.85 * (1 - e^(-n/35)) after n hands.

## Arena results

`./build/arena` plays duplicate matches: every deal is played twice with the seats swapped so card luck mostly cancels. 2000 deals (4000 hands) per opponent, river solver on, +- is a 95% interval:

```
opponent       bb/100       +-     secs
random         +420.3     76.6     23.6
station       +2084.1    116.5     82.7
maniac         +616.3    153.4      0.7
nit             +46.4     23.1      3.6
tag            +214.5     53.9     21.9
```

To see how much the exploitation is worth, I ran it with `--pmax 0` (never trust the read) over 400 deals. It wins +598 vs the station instead of +1937, +312 vs random instead of +423, and +51 vs the tag instead of +155. A bully vs bully mirror match comes out at -29 +- 80, so zero, which is what it should be.

To be honest about it: these sparring bots are bad, so beating them proves the plumbing works more than anything. The real test is against something like Slumbot or other people's bots.

The reads aren't perfect either. The archetype model calls the nit a "reg" because it only learns from how people answer our postflop bets, and a nit only sees flops with strong hands. That's selection bias I haven't fixed yet. It also reads the tag as a station.

## Things from my notes that didn't hold up

- Bluff to value ratio. The notes had bluffs/value = B/P. If the betting range has bluff fraction B/(P+2B), the ratio is B/(P+B). Fixed in `exploit.h` and tested.
- The pseudo-harmonic action translation formula was upside down. The Ganzfried & Sandholm version is f(x) = (B - x)(1 + A) / ((B - A)(1 + x)). The size that confuses it most (maps 50/50) is (A + B + 2AB) / (A + B + 2), which is close to sqrt(AB) but not equal.
- "PDCFR+ is 3-10x faster than CFR+." On my bench DCFR is about 5x better than CFR+, but the predictive part (PCFR+) gave nothing on this spot. I didn't implement the combined PDCFR+.
- "MMD converges to the projection of the bully policy onto the Nash polytope." What I actually see is it converging to a regularized equilibrium near the magnet, and it gets closer to Nash as tau decays. It's not a free exploit with zero exploitability.
- The Safe Exploitation Search code in my notes doesn't really do SES (it just nudges reach probabilities by a constant). I used a proper restricted nash response instead and left SES as a todo.
- The "12 dimensional" stat tensor is actually 13 stats if you count them. I track 20.
- "400M evals/s single threaded" is for a Two Plus Two style lookup table. My bitmask evaluator does about 42M/s per thread (333M/s on 8), which has been plenty so far.

## Todo

- A blueprint for preflop, flop and turn (MCCFR over an abstraction, which is what the EMD buckets are for). Right now those streets are equity heuristics on top of range tracking.
- Safe subgame solving. The river re-solve trusts our estimated ranges and has no gadget, so a strong opponent could exploit bad range estimates.
- AIVAT for variance reduction. Duplicate dealing helps, but not as much.
- Growing-tree CFR / value nets for deeper search.
- Fix the archetype selection bias (learn from preflop folds too).

Don't point this at real money sites, it's against their terms and that's not what it's for. It's for bot competitions and messing around.
