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

GameResult game(const Engine E[NB_COLOR], Color color, const std::string& fen,
	const Engine::SearchParam& sp)
/*
 * Play a game between E[0] and E[1], with given FEN starting position, and SearchParam
 * E[0] plays color, and E[1] plays opp_color(color)
 * */
{
	GameResult game_result;
	Board B;
	B.set_fen(fen.c_str());
	std::string moves;

	for (;;)
	{
		Color stm = B.get_turn();
		int idx = stm ^ color;
		
		E[idx].set_position(fen, moves);
		std::string move_string = E[idx].search(sp);

		move_t m = string_to_move(B, move_string.c_str());
		if (!B.is_legal(m))
		{
			game_result.result = ResultIllegalMove;
			game_result.winner = opp_color(stm);
			return game_result;
		}

		B.play(m);
		
		if ((game_result.result = B.game_over()))
		{
			game_result.winner = game_result.result == ResultMate ? stm : NoColor;
			return game_result;
		}

		moves += move_string + " ";
	}
}

MatchResult::MatchResult()
	: win(0), draw(0), loss(0)
{}

MatchResult match(const Engine E[2], const EPD& epd, const Engine::SearchParam& sp, size_t nb_games)
{
	std::map<Result, std::string> result_desc;
	result_desc[ResultMate] = "check mate";
	result_desc[ResultThreefold] = "3-fold repetition";
	result_desc[Result50Move] = "50-move rule";
	result_desc[ResultMaterial] = "insufficient material";
	result_desc[ResultStalemate] = "stalemate";
	result_desc[ResultIllegalMove] = "illegal move";
	result_desc[ResultNone] = "ERROR";

	std::map<Color, std::string> winner_desc;
	winner_desc[White] = "White wins";
	winner_desc[Black] = "Black wins";
	winner_desc[NoColor] = "Draw";

	MatchResult match_result;
	
	for (size_t cnt = 0; cnt < nb_games; cnt++)
	{
		std::string fen = epd.next();
		Color color = Color(cnt & 1);
		GameResult game_result = game(E, color, fen, sp);
		
		if (game_result.winner == NoColor)
			match_result.draw++;
		else {
			if (game_result.winner == color)
				match_result.win++;
			else
				match_result.loss++;
		}

		std::cout << match_result.win << " - " << match_result.loss << " - " << match_result.draw << '\t';
		std::cout << winner_desc[game_result.winner] << " by ";
		std::cout << result_desc[game_result.result] << std::endl;
	}
	
	return match_result;
}