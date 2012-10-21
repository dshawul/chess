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
#include <ostream>
#include "match.h"
#include "pgn.h"

GameResult game(const Engine E[NB_COLOR], const std::string& fen, const Engine::SearchParam& sp,
                std::ostream *ostrm)
{
	GameResult game_result;
	Board B;
	B.set_fen(fen.c_str());
	std::string moves;

	Engine::SearchParam current_sp(sp);
	const bool update_clock[NB_COLOR] = { sp.has_clock(White), sp.has_clock(Black) };

	PGN::Header header;
	header.white = E[White].get_name();
	header.black = E[Black].get_name();
	header.sp = sp;
	header.fen = fen;
	header.color = B.get_turn();
	header.move_count = B.get_move_count();
	PGN pgn(header);

	for (;;)
	{
		const Color stm = B.get_turn();
		int elapsed;

		E[stm].set_position(fen, moves);
		std::string move_string = E[stm].search(current_sp, elapsed);

		if (update_clock[stm])
		{
			if (stm == White)
				current_sp.wtime += sp.winc - elapsed;
			else	// stm == Black
				current_sp.btime += sp.binc - elapsed;
		}

		move_t m = string_to_move(B, move_string.c_str());
		if (!B.is_legal(m))
		{
			game_result.result = ResultIllegalMove;
			game_result.winner = opp_color(stm);
			return game_result;
		}

		pgn << move_to_san(B, m);

		B.play(m);

		if ((game_result.result = B.game_over()))
		{
			game_result.winner = game_result.result == ResultMate ? stm : NoColor;

			pgn.set_result(game_result.winner == White ? "1-0"
			               : (game_result.winner == Black ? "0-1" : "1/2-1/2"));
			pgn >> *ostrm;

			return game_result;
		}

		moves += move_string + " ";
	}
}
