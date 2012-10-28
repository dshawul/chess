/*
 * Zinc, an UCI chess interface. Copyright (C) 2012 Lucas Braesch.
 *
 * Zinc is free software: you can redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zinc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not,
 * see <http://www.gnu.org/licenses/>.
*/
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
