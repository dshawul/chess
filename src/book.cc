#include "book.h"
#include "search.h"

namespace {

Bitboard keys[0x100000];

void process_fen(const std::string& fen, std::ostream& os)
{
	board::Board B;
	B.set_fen(fen);
	
	search::Limits sl;
	sl.depth = 12;
	sl.nodes = 1 << 20;	// in case of search explosion

	move::move_t mlist[MAX_MOVES];
	move::move_t *end = movegen::gen_moves(B, mlist);
	
	for (move::move_t *m = mlist; m != end; ++m) {
		B.play(*m);
		
		std::string new_fen = B.get_fen();
		const Bitboard key = B.get_key();
		const size_t idx = key & 0x100000;
		
		if (keys[idx] != key) {
			int score;
			search::bestmove(B, sl, &score);
			
			// position has been searched: always overwrite, to not search it again
			keys[idx] = key;
			
			if (std::abs(score) <= 50)
				os << new_fen << std::endl;
		}
		
		B.undo();
	}
}

}	// namespace

namespace book {

void process_file(std::istream& is, std::ostream& os)
{
	memset(keys, 0, sizeof(keys));
	
	search::TT.alloc(32ULL << 20);
	search::clear_state();

	std::string fen;
	while (getline(is, fen))
		process_fen(fen, os);
}
	
}	// namespace book
