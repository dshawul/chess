#include "chessclock.h"
#include <sstream>

std::string ChessClock::uci_str(Color color) const
{
	std::ostringstream s;
	s << "go";

	if (has_clock()) {
		s << (color == White ? " wtime " : " btime ") << time;
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

std::string ChessClock::pgn_str(Color color) const
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
