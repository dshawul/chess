#include "match.h"

Result match(const Engine E[NB_COLOR], const std::string& fen)
{
	Result result;
	Board B;
	set_fen(&B, fen.c_str());
	
	// hardcode 100 ms/move for the moment
	Engine::SearchParam sp;
	sp.movetime = 100;
	
	std::string moves;
	
	for (;;) {
		E[B.turn].set_position(fen, moves);
		std::string move_string = E[B.turn].search(sp);
		
		move_t m = string_to_move(&B, move_string.c_str());
		if (!move_is_legal(&B, m)) {
			// TODO: declare E[B->turn] as forfeit, and exit gracefully
		}
		
		play(&B, m);
		if ((result = game_over(&B)))
			return result;		
		
		moves += move_string + " ";
	}
}