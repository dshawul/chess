/*
 * DiscoCheck, an UCI chess engine. Copyright (C) 2011-2013 Lucas Braesch.
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
#include <cstring>
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

/* History heuristic (quite move ordering):
 * - moves are sorted by descending h[color][piece][tsq] (after TT, killers, refutation).
 * - when a move fails high, its history score is incremented by depth*depth.
 * - for all other moves searched before, decrement their history score by depth*depth.
 * */
struct History
{
	static const int Max = 2000;
	
	void clear();
	void add(const Board& B, move_t m, int bonus);
	int get(const Board& B, move_t m) const;
	
private:
	int h[NB_COLOR][NB_PIECE][NB_SQUARE];
};

/* Double Move Refutation Hash Table:
 * - used for quiet move ordering, and as an LMR guard.
 * - move pair (m1, m2) -> move m3 that refuted the sequence (m1, m2) last time it was visited by the
 * search.
 * - dm_key is the zobrist key of the last 2 moves (see Board::get_dm_key(), in particular floor at
 * root sp0).
 * - always overwrite: fancy ageing schemes, or seveal slots per move pair did not work in testing.
 * */
struct Refutation
{
	struct Pack {
		uint64_t dm_key: 48;
		move_t move;
	};
	
	static const int count = 0x10000;
	Pack r[count];
	
	void clear() {
		memset(this, 0, sizeof(this));
	}
	
	move_t get_refutation(Key dm_key) const {
		const size_t idx = dm_key & (count-1);
		Pack tmp = {dm_key, 0};
		return r[idx].dm_key == tmp.dm_key ? r[idx].move : move_t(0);
	}
	
	void set_refutation(Key dm_key, move_t m) {
		const size_t idx = dm_key & (count-1);
		r[idx] = {dm_key, m};
	}
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

	MoveSort(const Board* _B, int _depth, const SearchInfo *_ss, 
		const History *_H, const Refutation *_R);

	move_t next(int *see);
	move_t previous();

	int get_count() const { return count; }

private:
	const Board *B;
	GenType type;
	const SearchInfo *ss;
	const History *H;
	const Refutation *R;
	move_t refutation;

	Token list[MAX_MOVES];
	int idx, count, depth;

	move_t *generate(GenType type, move_t *mlist);
	void annotate(const move_t *mlist);
	void score(MoveSort::Token *t);
};
