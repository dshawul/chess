#include "eval.h"

int calc_phase(const Board& B)
{
	unsigned total = 4*(vN + vB + vR) + 2*vQ;
	return (B.st().piece_psq[WHITE] + B.st().piece_psq[BLACK]) * 1024 / total;
}

int eval(const Board& B)
{
	assert(!B.is_check());
	int us = B.get_turn(), them = opp_color(us);
	int phase = calc_phase(B);
	
	return (phase * (B.st().psq[us].op - B.st().psq[them].op)
		+ (1024 - phase) * (B.st().psq[us].eg - B.st().psq[them].eg)) / 1024;
}