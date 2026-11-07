#pragma once

#include <memory>
#include <string>

#include "game.h"

namespace poker {

// sparring partners for the arena:
//   random   picks fold / call / raise at random
//   station  never folds, never raises
//   maniac   all in every single decision
//   nit      top ~12% preflop, only continues with strong made hands
//   tag      simple tight-aggressive equity bot, the closest thing here to a real player
std::unique_ptr<Bot> make_bot(const std::string& name, uint64_t seed = 1);
const char* const BOT_NAMES[] = {"random", "station", "maniac", "nit", "tag"};

}  // namespace poker
