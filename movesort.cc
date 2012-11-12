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
#include <algorithm>
#include "movesort.h"

MoveSort::MoveSort(const Board *_B, GenType _type)
	: B(_B), type(_type), idx(0)
{
	move_t mlist[MAX_MOVES];
	count = generate(type, mlist) - mlist;
	annotate(mlist);
}

move_t *MoveSort::generate(GenType type, move_t *mlist)
{
	if (type == ALL)
		return gen_moves(*B, mlist);
	else {
		if (B->st().checkers)
			return gen_evasion(*B, mlist);
		else {
			move_t *end = mlist;
			Bitboard enemies = B->get_pieces(opp_color(B->get_turn()));
			
			end = gen_piece_moves(*B, enemies, end, true);
			end = gen_pawn_moves(*B, enemies | B->st().epsq_bb(), end, false);
			
			if (type == CAPTURES_CHECKS)
				end = gen_quiet_checks(*B, end);
			
			return end;
		}
	}
}

void MoveSort::annotate(const move_t *mlist)
{
	for (int i = idx; i < count; ++i) {
		list[i].m = mlist[i];
		list[i].score = score(list[i].m);
	}
}

int MoveSort::score(move_t m)
{
	switch (type) {
		case ALL:	// search
			return (move_is_cop(*B, m) || test_bit(B->st().attacked, m.tsq)) ? see(*B, m) : 0;
		default:	// qsearch
			return move_is_cop(*B, m) ? mvv_lva(*B, m) : 0;
	}
}

move_t *MoveSort::next()
{
	if (idx < count) {
		std::swap(list[idx], *std::max_element(&list[idx], &list[count]));
		return &list[idx++].m;
	} else
		return NULL;
}