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
#include "movesort.h"

MoveSort::MoveSort(const Board& B, GenType type)
{
	move_t mlist[MAX_MOVES];
	move_t *end = generate(B, type, mlist);
}

move_t *MoveSort::generate(const Board& B, GenType type, move_t *mlist)
{
	if (type == ALL)
		return gen_moves(B, mlist);
	else {
		if (B.st().checkers)
			return gen_evasion(B, mlist);
		else {
			move_t *end = mlist;
			Bitboard enemies = B.get_pieces(opp_color(B.get_turn()));
			
			end = gen_piece_moves(B, enemies, end, true);
			end = gen_pawn_moves(B, enemies | B.st().epsq_bb(), end, false);
			
			if (type == CAPTURES_CHECKS)
				end = gen_quiet_checks(B, end);
			
			return end;
		}
	}
}

const MoveSort::Token& MoveSort::next()
{}
