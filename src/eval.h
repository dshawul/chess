#pragma once
#include "board.h"

extern void init_eval();
extern int eval(const Board& B);

extern void init_kpk();
bool probe_kpk(int wk, int bk, int stm, int wp);