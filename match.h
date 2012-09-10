#pragma once
#include "board.h"
#include "engine.h"

struct MatchResult
{
	Color winner;
	Result result;
};

extern MatchResult match(const Engine E[NB_COLOR], const std::string& fen);