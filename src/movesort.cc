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
#include <algorithm>
#include "movesort.h"
#include "search.h"

void History::clear()
{
	memset(h, 0, sizeof(h));
}

int History::get(const Board& B, move_t m) const
{
	const int piece = B.get_piece_on(m.fsq()), tsq = m.tsq();
	assert(!move_is_cop(B, m) && piece_ok(piece));

	return h[B.get_turn()][piece][tsq];
}

void History::add(const Board& B, move_t m, int bonus)
{
	const int piece = B.get_piece_on(m.fsq()), tsq = m.tsq();
	assert(!move_is_cop(B, m) && piece_ok(piece));

	int &v = h[B.get_turn()][piece][tsq];
	v += bonus;

	if (std::abs(v) >= History::Max)
		for (int c = WHITE; c <= BLACK; ++c)
			for (int p = PAWN; p <= KING; ++p)
				for (int s = A1; s <= H8; h[c][p][s++] /= 2);
}

MoveSort::MoveSort(const Board* _B, int _depth, const SearchInfo *_ss, int _node_type,
	const History *_H, const Refutation *_R)
	: B(_B), ss(_ss), node_type(_node_type), H(_H), R(_R), idx(0), depth(_depth)
{
	type = depth > 0 ? ALL : (depth == 0 ? CAPTURES_CHECKS : CAPTURES);
	/* If we're in check set type = ALL. This affects the sort() and uses SEE instead of MVV/LVA for
	 * example. It improves the quality of sorting for check evasions in the qsearch. */
	if (B->is_check())
		type = ALL;

	refutation = R ? R->get_refutation(B->get_dm_key()) : move_t(0);
	
	move_t mlist[MAX_MOVES];
	count = generate(type, mlist) - mlist;
	annotate(mlist);
}

move_t *MoveSort::generate(GenType type, move_t *mlist)
{
	if (type == ALL)
		return gen_moves(*B, mlist);
	else {
		// If we are in check, then type must be ALL (see constructor)
		assert(!B->is_check());

		move_t *end = mlist;
		Bitboard enemies = B->get_pieces(opp_color(B->get_turn()));

		end = gen_piece_moves(*B, enemies, end, true);
		end = gen_pawn_moves(*B, enemies | B->st().epsq_bb() | PPromotionRank[B->get_turn()], end, false);

		if (type == CAPTURES_CHECKS)
			end = gen_quiet_checks(*B, end);

		return end;
	}
}

void MoveSort::annotate(const move_t *mlist)
{
	for (int i = idx; i < count; ++i) {
		list[i].m = mlist[i];
		score(&list[i]);
	}
}

void MoveSort::score(MoveSort::Token *t)
{
	t->see = -INF;	// not computed

	if (t->m == ss->best)
		t->score = INF;
	else if (move_is_cop(*B, t->m))
		if (type == ALL) {
			if (node_type == All)
				t->score = mvv_lva(*B, t->m) + History::Max;
			else {
				// equal and winning captures, by SEE, in front of quiet moves
				// losing captures, after all quiet moves
				t->see = calc_see(*B, t->m);
				t->score = t->see >= 0 ? t->see + History::Max : t->see - History::Max;
			}

		} else
			t->score = mvv_lva(*B, t->m);
	else {
		// killers first, then the rest by history
		if (depth > 0 && t->m == ss->killer[0])
			t->score = History::Max-1;
		else if (depth > 0 && t->m == ss->killer[1])
			t->score = History::Max-2;
		else if (t->m == refutation)
			t->score = History::Max-3;
		else
			t->score = H->get(*B, t->m);
	}
}

move_t MoveSort::next(int *see)
{
	if (idx < count) {
		std::swap(list[idx], *std::max_element(&list[idx], &list[count]));
		const Token& t = list[idx++];
		*see = t.see == -INF
		       ? calc_see(*B, t.m)	// compute SEE
		       : t.see;			// use SEE cache
		return t.m;
	} else
		return 0;
}

move_t MoveSort::previous()
{
	if (idx > 0)
		return list[--idx].m;
	else
		return move_t(0);
}
