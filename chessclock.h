#pragma once
#include "types.h"

struct ChessClock
{
	ChessClock();
	mutable int time;
	int inc, movetime;
	unsigned depth, nodes;
	
	bool has_clock() const;
	std::string uci_str(Color color) const;
	std::string pgn_str(Color color) const;
};

inline ChessClock::ChessClock()
	: time(0), inc(0), movetime(0), depth(0), nodes(0)
{}

inline bool ChessClock::has_clock() const
{
	return time || inc;
}