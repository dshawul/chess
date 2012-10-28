#pragma once
#include "types.h"

class Clock
{
public:
	struct Err {};
	struct TimeOut: Err {};

	Clock();

	std::string uci_str(Color color) const;
	std::string pgn_str(Color color) const;
	void consume(int elapsed) const throw (TimeOut);

	void set_time(int msec)		{ time = time_left = msec; }
	void set_inc(int msec)		{ inc = msec; }
	void set_movetime(int msec)	{ movetime = msec; }
	void set_depth(int _depth)	{ depth = _depth; }
	void set_nodes(int _nodes)	{ nodes = _nodes; }

private:
	int time, inc, movetime;
	mutable int time_left;
	unsigned depth, nodes;

	bool has_clock() const;
};

inline Clock::Clock()
	: time(0), inc(0), movetime(0), depth(0), nodes(0)
{}

inline bool Clock::has_clock() const
{
	return time || inc;
}
