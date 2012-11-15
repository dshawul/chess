/*
 * DiscoCheck, an UCI chess interface. Copyright (C) 2012 Lucas Braesch.
 *
 * DiscoCheck is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * DiscoCheck is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not,
 * see <http://www.gnu.org/licenses/>.
*/
#include "search.h"
#include "eval.h"
#include "movesort.h"
#include "prng.h"

namespace
{
	struct SearchInfo {
		move_t best_move;
		int ply;
	};
	const int MAX_PLY = 0x80;
	const int MATE = 32000;

	bool abort_search;
	uint64_t node_count, node_limit;
	bool node_poll();

	int search(Board& B, int alpha, int beta, int depth, SearchInfo *ss);
	int qsearch(Board& B, int alpha, int beta, int depth, SearchInfo *ss);
}

move_t bestmove(Board& B, const SearchLimits& sl)
{
	SearchInfo ss[MAX_PLY];
	for (int ply = 0; ply < MAX_PLY; ++ply)
		ss[ply].ply = ply;

	node_count = 0;
	node_limit = sl.nodes;
	abort_search = false;
	move_t best_move;
	
	for (int depth = 1; !abort_search && (!sl.depth || depth <= sl.depth); depth++) {
		int score = search(B, -INF, +INF, depth, ss);

		if (!abort_search) {
			best_move = ss->best_move;
			
			std::cout << "info score cp " << score
			          << " depth " << depth
			          << " nodes " << node_count
			          << " pv " << move_to_string(ss->best_move)
			          << std::endl;
		}
	}

	return ss->best_move;
}

namespace
{
	const int QS_LIMIT = -6;

	int mated_in(int ply)	{ return ply-MATE; }
	int mate_in(int ply)	{ return MATE-ply; }

	int search(Board& B, int alpha, int beta, int depth, SearchInfo *ss)
	{
		assert(alpha < beta);

		if (depth <= 0)
			return qsearch(B, alpha, beta, depth, ss);

		assert(depth > 0);

		if ( (abort_search = node_poll()) )
			return 0;

		int best_score = -INF;

		// mate distance pruning
		alpha = std::max(alpha, mated_in(ss->ply));
		beta = std::min(beta, mate_in(ss->ply+1));
		if (alpha >= beta) {
			assert(ss->ply > 0);
			return alpha;
		}

		MoveSort MS(&B, MoveSort::ALL);
		move_t *m;

		while ( alpha < beta && (m = MS.next()) && !abort_search ) {
			bool check = move_is_check(B, *m);

			// check extension
			int new_depth;
			if (check && (check == DISCO_CHECK || see(B, *m) >= 0) )
				new_depth = depth;
			else
				new_depth = depth-1;

			// recursion
			B.play(*m);
			int score = -search(B, -beta, -alpha, new_depth, ss+1);
			B.undo();

			if (score > best_score) {
				best_score = score;
				alpha = std::max(alpha, score);
				ss->best_move = *m;
			}
		}

		// mated or stalemated
		if (!MS.get_count()) {
			assert(ss->ply > 0);
			return B.is_check() ? mated_in(ss->ply) : 0;
		}

		return best_score;
	}

	int qsearch(Board& B, int alpha, int beta, int depth, SearchInfo *ss)
	{
		assert(depth <= 0);
		assert(alpha < beta);

		if ( (abort_search = node_poll()) )
			return 0;

		bool in_check = B.is_check();
		int best_score = -INF, static_eval = -INF;

		// stand pat
		if (!in_check) {
			best_score = static_eval = eval(B);
			ss->best_move = NO_MOVE;
			alpha = std::max(alpha, best_score);
			if (alpha >= beta) {
				return alpha;
			}
		}

		MoveSort MS(&B, depth < 0 ? MoveSort::CAPTURES : MoveSort::CAPTURES_CHECKS);
		move_t *m;

		while ( alpha < beta && (m = MS.next()) && !abort_search ) {
			int check = move_is_check(B, *m);

			// SEE pruning
			if (!in_check && check != DISCO_CHECK && see(B, *m) < 0)
				continue;

			// recursion
			int score;
			if (depth <= QS_LIMIT && !in_check)
				score = static_eval + see(B,*m);	// prevent qsearch explosion
			else {
				B.play(*m);
				score = -qsearch(B, -beta, -alpha, depth-1, ss+1);
				B.undo();
			}

			if (score > best_score) {
				best_score = score;
				alpha = std::max(alpha, score);
				ss->best_move = *m;
			}
		}

		if (B.is_check() && !MS.get_count())
			return mated_in(ss->ply);

		return best_score;
	}

	bool node_poll()
	{
		++node_count;
		return node_limit && node_count >= node_limit;
	}
}
