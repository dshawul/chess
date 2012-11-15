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

namespace {
	int mated_in(int ply)	{ return ply-MATE; }
	int mate_in(int ply)	{ return MATE-ply; }
}

move_t bestmove(Board& B, const SearchLimits& sl)
{
	// FIXME: play pseudo-random moves for now
	move_t mlist[MAX_MOVES];
	move_t *end = gen_moves(B, mlist);
	int count = end - mlist;
	int idx = PRNG().rand<int>() % count;
	return mlist[idx];
}

uint64_t node_count;

int search(Board& B, int alpha, int beta, int depth, int ply)
{
	assert(alpha < beta);
	
	if (depth <= 0)
		return qsearch(B, alpha, beta, depth, ply);

	assert(depth > 0);
	node_count++;
	int best_score = -INF;
	
	// mate distance pruning
	alpha = std::max(alpha, mated_in(ply));	// at worst we're mated on this node
	beta = std::min(beta, mate_in(ply+1));	// at best, we mate at the next ply
	if (alpha >= beta)
		return alpha;
	
	MoveSort MS(&B, MoveSort::ALL);
	move_t *m;
	int cnt = 0;
	
	while ( alpha < beta && (m = MS.next()) ) {
		cnt++;
		bool check = move_is_check(B, *m);
		
		int new_depth;
		if (check && (check == DISCO_CHECK || see(B, *m) >= 0) )
			new_depth = depth;
		else
			new_depth = depth-1;
		
		B.play(*m);
		int score = -search(B, -beta, -alpha, new_depth, ply+1);
		B.undo();
		
		best_score = std::max(best_score, score);
		alpha = std::max(alpha, score);
	}
	
	// mated or stalemated
	if (!cnt)
		return B.is_check() ? ply-MATE : 0;
	
	return best_score;
}

#define QS_LIMIT	-6

int qsearch(Board& B, int alpha, int beta, int depth, int ply)
{
	assert(depth <= 0);
	assert(alpha < beta);
	
	node_count++;
	bool in_check = B.is_check();
	int best_score = -INF, static_eval = -INF;

	// stand pat
	if (!in_check) {
		best_score = static_eval = eval(B);
		alpha = std::max(alpha, best_score);
		if (alpha >= beta)
			return alpha;
	}
	
	MoveSort MS(&B, depth < 0 ? MoveSort::CAPTURES : MoveSort::CAPTURES_CHECKS);
	move_t *m;
	int cnt = 0;
	
	while ( alpha < beta && (m = MS.next()) ) {
		cnt++;
		int check = move_is_check(B, *m);
		
		if (!in_check && check != DISCO_CHECK && see(B, *m) < 0)
			continue;
		
		int score;
		
		if (depth <= QS_LIMIT && !in_check)
			score = static_eval + see(B,*m);	// prevent qsearch explosion
		else {
			B.play(*m);
			score = -qsearch(B, -beta, -alpha, depth-1, ply+1);
			B.undo();
		}
		
		best_score = std::max(best_score, score);
		alpha = std::max(alpha, score);
	}
	
	if (B.is_check() && !cnt)
		return ply-MATE;
	
	return best_score;
}
