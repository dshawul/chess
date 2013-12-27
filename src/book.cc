#include "book.h"
#include "search.h"

namespace {

void process_fen(const std::string& fen, std::ostream& os)
{
	board::Board B;
	B.set_fen(fen);
	
	search::Limits sl;
	sl.depth = 12;
	sl.nodes = 1 << 20;	// in case of search explosion

	search::TT.alloc(16ULL << 20);
	search::clear_state();

	move::move_t mlist[MAX_MOVES];
	move::move_t *end = movegen::gen_moves(B, mlist);
	
	for (move::move_t *m = mlist; m != end; ++m) {
		int score;
		
		B.play(*m);
		std::string new_fen = B.get_fen();
		
		search::bestmove(B, sl, &score);
		
		if (std::abs(score) < 70)
			os << new_fen << std::endl;
		
		B.undo();
	}
}

}	// namespace

namespace book {

void process_file(std::istream& is, std::ostream& os)
{
	std::string fen;
	while (getline(is, fen))
		process_fen(fen, os);
}
	
}	// namespace book
