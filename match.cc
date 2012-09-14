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
#include "match.h"

MatchResult match(const Engine E[NB_COLOR], const std::string& fen)
{
	MatchResult match_result;
	Board B;
	B.set_fen(fen.c_str());

	// hardcode 100 ms/move for the moment
	Engine::SearchParam sp;
	sp.movetime = 100;

	std::string moves;

	for (;;)
	{
		E[B.get_turn()].set_position(fen, moves);
		std::string move_string = E[B.get_turn()].search(sp);

		move_t m = string_to_move(B, move_string.c_str());
		if (!B.is_legal(m))
		{
			match_result.result = ResultIllegalMove;
			match_result.winner = opp_color(B.get_turn());
			return match_result;
		}

		std::cout << move_string << std::endl;
		B.play(m);
		std::cout << B << std::endl;
		
		if ((match_result.result = B.game_over()))
		{
			match_result.winner = match_result.result == ResultMate
			                      ? opp_color(B.get_turn())
			                      : NoColor;
			return match_result;
		}

		moves += move_string + " ";
	}
}
