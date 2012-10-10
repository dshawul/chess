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

MatchResult match(const Engine E[NB_COLOR], const std::string& fen, const Engine::SearchParam& sp)
{
	MatchResult match_result;
	Board B;
	B.set_fen(fen.c_str());

	std::string moves;
	Color stm;

	for (;;)
	{
		stm = B.get_turn();
		
		E[stm].set_position(fen, moves);
		std::string move_string = E[stm].search(sp);

		move_t m = string_to_move(B, move_string.c_str());
		if (!B.is_legal(m))
		{
			match_result.result = ResultIllegalMove;
			match_result.winner = opp_color(stm);
			return match_result;
		}

		std::cout << move_to_san(B, m) << '\t';
		if (stm == Black)
			std::cout << std::endl;
		
		B.play(m);
		
		if ((match_result.result = B.game_over()))
		{
			match_result.winner = match_result.result == ResultMate ? stm : NoColor;
			return match_result;
		}

		moves += move_string + " ";
	}
}
