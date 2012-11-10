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
