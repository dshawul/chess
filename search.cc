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

move_t bestmove(Board& B, const SearchLimits& sl)
{
	// FIXME: play pseudo-random moves for now
	move_t mlist[MAX_MOVES];
	move_t *end = gen_moves(B, mlist);
	int count = end - mlist;
	int idx = PRNG().rand<int>() % count;
	return mlist[idx];
}

uint64_t node_count = 0;

int qsearch(Board& B, int alpha, int beta, int depth, int ply)
{
	node_count++;		
	int best_score = -INF;

	// stand pat
	if (!B.is_check()) {
		best_score = eval(B);
		if (best_score > alpha) {
			alpha = best_score;
			if (alpha >= beta)
				return alpha;
		}
	}
	
	MoveSort MS(&B, depth < 0 ? MoveSort::CAPTURES : MoveSort::CAPTURES_CHECKS);
	move_t *m;
	int cnt = 0;
	
	while ( (m = MS.next()) && alpha < beta) {
		cnt++;
		
		if (!B.is_check() && !move_is_check(B, *m) && see(B, *m) < 0) {
			assert(move_is_cop(B, *m));
			continue;
			
		}
		
		B.play(*m);
		int score = -qsearch(B, -beta, -alpha, depth-1, ply+1);
		B.undo();
		
		if (score > best_score) {
			best_score = score;
			if (score > alpha)
				alpha = score;
		}
	}
	
	if (B.is_check() && !cnt)
		return ply-MATE;
	
	return best_score;
}
