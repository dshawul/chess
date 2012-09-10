#include "match.h"

MatchResult match(const Engine E[NB_COLOR], const std::string& fen)
{
	MatchResult match_result;
	Board B;
	set_fen(&B, fen.c_str());

	// hardcode 100 ms/move for the moment
	Engine::SearchParam sp;
	sp.movetime = 100;

	std::string moves;

	for (;;)
	{
		E[B.turn].set_position(fen, moves);
		std::string move_string = E[B.turn].search(sp);

		move_t m = string_to_move(&B, move_string.c_str());
		if (!move_is_legal(&B, m))
		{
			match_result.result = ResultIllegalMove;
			match_result.winner = opp_color(B.turn);
			return match_result;
		}

		play(&B, m);
		if ((match_result.result = game_over(&B)))
		{
			match_result.winner = match_result.result == ResultMate
				? opp_color(B.turn)
				: NoColor;
			return match_result;
		}

		moves += move_string + " ";
	}
}
