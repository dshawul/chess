#pragma once
#include "board.h"

extern std::uint64_t perft(Board& B, int depth, int ply);
extern bool test_perft();

extern void bench(int depth);

