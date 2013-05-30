/*
 * DiscoCheck, an UCI chess engine. Copyright (C) 2011-2013 Lucas Braesch.
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

#if defined(_WIN32) || defined(_WIN64)
   #include <windows.h>
   #include <conio.h>
#else   // assume POSIX
   #include <cerrno>
   #include <unistd.h>
   #include <sys/time.h>
#endif

/* Default values for UCI options */
int Hash = 16;
int Contempt = 25;
bool UCI_LimitStrength = false;
static const int ELO_MIN = 1400, ELO_MAX = 2600;
int UCI_Elo = ELO_MIN;

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
	std::string cmd, token;
	std::cout << std::boolalpha;
	
	while (token != "quit") {
		if (!getline(std::cin, cmd) || cmd == "quit")
			break;

		std::istringstream is(cmd);
		is >> std::boolalpha;
		is >> std::skipws >> token;
		if (token == "uci")
			std::cout << "id name DiscoCheck 4.2.1\n"
				<< "id author Lucas Braesch\n"
				/* Declare UCI options here */
				<< "option name Hash type spin default " << Hash << " min 1 max 8192\n"
				<< "option name Clear Hash type button\n"
				<< "option name Contempt type spin default " << Contempt << " min 0 max 100\n"
				<< "option name UCI_LimitStrength type check default " << UCI_LimitStrength << '\n'
				<< "option name UCI_Elo type spin default " << UCI_Elo
					<< " min " << ELO_MIN << " max " << ELO_MAX <<  "\n"
				/* end of UCI options */
				<< "uciok" << std::endl;
		else if (token == "ucinewgame")
			TT.clear();
		else if (token == "position")
			position(B, is);
		else if (token == "go")
			go(B, is);
		else if (token == "isready") {
			TT.alloc(Hash << 20);
			std::cout << "readyok" << std::endl;
		} else if (token == "setoption")
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
		
		if (UCI_LimitStrength)
			// discard parameters of the go command
			sl.nodes = pow(2.0, 8.0 + pow((UCI_Elo-ELO_MIN)/128.0, 1.0/0.9));
		else {
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

		/* UCI option 'name' has been modified. Handle here. */
		if (name == "Hash")
			is >> Hash;
		else if (name == "ClearHash")
			TT.clear();
		else if (name == "Contempt")
			is >> Contempt;
		else if (name == "UCI_LimitStrength")
			is >> UCI_LimitStrength;
		else if (name == "UCI_Elo")
			is >> UCI_Elo;
	}
}

bool input_available()
{
#if defined(_WIN32) || defined(_WIN64)

	static bool init = false, is_pipe;
	static HANDLE stdin_h;
	DWORD val;

	if (!init) {
		init = true;
		stdin_h = GetStdHandle(STD_INPUT_HANDLE);
		is_pipe = !GetConsoleMode(stdin_h, &val);
	}

	if (stdin->_cnt > 0)
		return true;

	if (is_pipe)
		return !PeekNamedPipe(stdin_h, NULL, 0, NULL, &val, NULL) ? true : val>0;
	else
		return _kbhit();

#else   // assume POSIX

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);

	struct timeval timeout;
	timeout.tv_sec = timeout.tv_usec = 0;

	select(STDIN_FILENO+1, &readfds, 0, 0, &timeout);
	return FD_ISSET(STDIN_FILENO, &readfds);

#endif
}

bool stop_received()
{
	std::string token;
	
	if (input_available())
		getline(std::cin, token);
	
	return token == "stop";
}
