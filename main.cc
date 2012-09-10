#include <iostream>
#include <map>
#include "types.h"
#include "match.h"

int main(int argc, char **argv)
{
	init_bitboard();
	
	Engine E[NB_COLOR];
	E[White].create(argv[1]);
	E[Black].create(argv[2]);
	
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
	winner_desc[Black] = "Draw";
	
	MatchResult match_result = match(E, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	std::cout << winner_desc[match_result.winner] << " by "
		<< result_desc[match_result.result] << std::endl;
}