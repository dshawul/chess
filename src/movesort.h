/*
 * DiscoCheck, an UCI chess interface. Copyright (C) 2011-2013 Lucas Braesch.
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
#pragma once
#include "movegen.h"

struct SearchInfo {
	move_t m, best, killer[2];
	int ply, reduction, eval;
	bool skip_null, null_child;
	
	void clear(int _ply) {
		ply = _ply;
		m = best = killer[0] = killer[1] = 0;
		eval = reduction = 0;
		skip_null = null_child = false;
	}
};

struct History
{
	static const int Max = 2000;
	
	void clear();
	
	void add(const Board& B, move_t m, int bonus);
	int get(const Board& B, move_t m) const;
	
	void set_refutation(const Board& B, move_t m);
	move_t get_refutation(const Board& B) const;
	
private:
	/* History heuristic:
	 * - when a move causes a beta cutoff, increment by +depth*depth
	 * - all other previously searched moves are marked as bad, decrement by depth*depth
	 * - on overflow, divide the whole table by 2 */
	int history[NB_COLOR][NB_PIECE][NB_SQUARE];
	
	/* Counter move heuristic:
	 * remember the last refutation to each move. For example if 1.. Nf6 refutes 1. Nc3
	 * refutation[BLACK][KNIGHT][C3] == Nf6 move */
	move_t refutation[NB_COLOR][NB_PIECE+1][NB_SQUARE];
};

struct MoveSort
{
	enum GenType {
	    ALL,				// all legal moves
	    CAPTURES_CHECKS,	// captures and quiet checks
	    CAPTURES			// captures only
	};

	struct Token {
		move_t m;
		int score, see;
		bool operator< (const Token& t) const {return score < t.score; }
	};

	MoveSort(const Board* _B, int _depth, const SearchInfo *_ss, int _node_type, const History *_H);

	move_t next(int *see);
	move_t previous();

	int get_count() const { return count; }

private:
	const Board *B;
	GenType type;
	const SearchInfo *ss;
	int node_type;
	const History *H;
	move_t refutation;

	Token list[MAX_MOVES];
	int idx, count, depth;

	move_t *generate(GenType type, move_t *mlist);
	void annotate(const move_t *mlist);
	void score(MoveSort::Token *t);
};
