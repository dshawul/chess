#include "book.h"
#include "search.h"
#include "eval.h"

namespace {

Bitboard keys[0x1000000];
board::Board B;

void process_fen(const std::string& fen, std::ostream& os)
{
	B.set_fen(fen);
	
	search::Limits sl;
	sl.depth = 13;
	sl.nodes = 1 << 20;	// in case of search explosion

	move::move_t mlist[MAX_MOVES];
	move::move_t *end = movegen::gen_moves(B, mlist);
	
	for (move::move_t *m = mlist; m != end; ++m) {

		const bool tactical = move::is_check(B, *m) || move::is_cop(B, *m);
		const int eval_before = B.is_check() ? -INF : eval::symmetric_eval(B);

		B.play(*m);

		if (tactical || -eval::symmetric_eval(B) >= eval_before) {
			std::string new_fen = B.get_fen();
			const Bitboard key = B.get_key();
			const size_t idx = key & 0xffffffULL;
		
			if (keys[idx] != key) {
				int score;
				search::bestmove(B, sl, &score);
			
				// position has been searched: always overwrite, to not search it again
				keys[idx] = key;
			
				if (std::abs(score) <= 45)
					os << new_fen << std::endl;
			}
		}

		B.undo();
	}
}

}	// namespace

namespace book {

void process_file(std::istream& is, std::ostream& os)
{
	memset(keys, 0, sizeof(keys));
	
	search::TT.alloc(128ULL << 20);
	search::clear_state();

	std::string fen;
	while (getline(is, fen))
		process_fen(fen, os);
}
	
}	// namespace book

