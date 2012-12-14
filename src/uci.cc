/*
 * DiscoCheck, an UCI chess interface. Copyright (C) 2012 Lucas Braesch.
 *
 * DiscoCheck is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * DiscoCheck is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not,
 * see <http://www.gnu.org/licenses/>.
*/
#include <string>
#include <sstream>
#include "uci.h"
#include "search.h"
#include "eval.h"

namespace
{
	const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

	void position(Board& B, std::istringstream& is);
	void go(Board& B, std::istringstream& is);
	void setoption(std::istringstream& is);
}

void loop()
{
	Board B;
	TT.alloc(32 << 20);

	std::string cmd, token;

	while (token != "quit") {
		if (!getline(std::cin, cmd) || cmd == "quit")
			break;

		std::istringstream is(cmd);
		is >> std::skipws >> token;

		if (token == "uci")
			std::cout << "id name DiscoCheck\n"
			          << "id author Lucas Braesch\n"
					  << "option name Hash type spin default 32 min 1 max 8192\n"
					  << "option name Clear Hash type button\n"
			          << "uciok" << std::endl;
		else if (token == "ucinewgame")
			TT.clear();	// FIXME
		else if (token == "position")
			position(B, is);
		else if (token == "go")
			go(B, is);
		else if (token == "isready")
			std::cout << "readyok" << std::endl;
		else if (token == "setoption")
			setoption(is);
		else if (token == "eval")
			std::cout << B << "eval = " << eval(B) << std::endl;
	}
}

namespace
{
	void position(Board& B, std::istringstream& is)
	{
		move_t m;
		std::string token, fen;
		is >> token;

		if (token == "startpos") {
			fen = StartFEN;
			is >> token;	// Consume "moves" token if any
		} else if (token == "fen")
			while (is >> token && token != "moves")
				fen += token + " ";
		else
			return;

		B.set_fen(fen);

		// Parse move list (if any)
		while (is >> token) {
			m = string_to_move(B, token);
			B.play(m);
		}
	}

	void go(Board& B, std::istringstream& is)
	{
		SearchLimits sl;
		std::string token;

		while (is >> token) {
			if (token == (B.get_turn() ? "btime" : "wtime"))
				is >> sl.time;
			else if (token == (B.get_turn() ? "binc" : "winc"))
				is >> sl.inc;
			else if (token == "movestogo")
				is >> sl.movestogo;
			else if (token == "movetime")
				is >> sl.movetime;
			else if (token == "depth")
				is >> sl.depth;
			else if (token == "nodes")
				is >> sl.nodes;
		}

		move_t m = bestmove(B, sl);
		std::cout << "bestmove " << move_to_string(m) << std::endl;
	}
	
	void setoption(std::istringstream& is)
	{
		std::string token, name;
		if (!(is >> token) || token != "name")
			return;
		
		while (is >> token && token != "value")
			name += token;
		
		if (name == "Hash") {
			uint64_t size_mb;
			is >> size_mb;
			TT.alloc(size_mb << 20);
		} else if (name == "ClearHash")
			TT.clear();
	}
}
