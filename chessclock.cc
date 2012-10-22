#include "chessclock.h"
#include <sstream>

std::string ChessClock::uci_str(Color color) const
{
	std::ostringstream s;
	s << "go";

	if (has_clock())
	{
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