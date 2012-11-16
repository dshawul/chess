#include "eval.h"

int eval(const Board& B)
{
	assert(!B.is_check());
	int us = B.get_turn(), them = opp_color(us);
	return B.st().psq[us].op - B.st().psq[them].op;
}