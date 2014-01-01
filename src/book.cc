#include "book.h"
#include "movegen.h"
#include "eval.h"

namespace {

board::Board B;

void process_fen(const std::string& fen, std::ostream& os)
{
	B.set_fen(fen);

	move::move_t mlist[MAX_MOVES];
	move::move_t *end = movegen::gen_moves(B, mlist);
	
	for (move::move_t *m = mlist; m != end; ++m) {
		const bool tactical = move::is_check(B, *m) || move::is_cop(B, *m);
		const int eval_before = B.is_check() ? -INF : eval::symmetric_eval(B);

		if (tactical || -eval::symmetric_eval(B) >= eval_before) {
			std::string new_fen = B.get_fen();
			os << new_fen << std::endl;
		}
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
