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
#include "chessclock.h"
#include <sstream>

std::string Clock::uci_str(Color color) const
{
	std::ostringstream s;
	s << "go";

	if (has_clock()) {
		s << (color == White ? " wtime " : " btime ") << time_left;
		s << (color == White ? " winc " : " binc ") << inc;
	}
	if (movetime)
		s << " movetime " << movetime;
	if (depth)
		s << " depth " << depth;
	if (nodes)
		s << " nodes " << nodes;

	s << '\n';
	return s.str();
}

std::string Clock::pgn_str(Color color) const
{
	std::ostringstream s;

	if (has_clock())
		s << float(time)/1000 << '+' << float(inc)/1000;
	if (movetime)
		s << std::string(",", !s.str().empty()) << "movetime=" << movetime;
	if (nodes)
		s << std::string(",", !s.str().empty()) << "nodes=" << nodes;
	if (depth)
		s << std::string(",", !s.str().empty()) << "depth=" << depth;

	return s.str();
}

void Clock::consume(int elapsed) const throw (TimeOut)
{
	if (has_clock()) {
		time_left -= elapsed;
		if (time_left < 0)
			throw TimeOut();
		time_left += inc;
	}
}
