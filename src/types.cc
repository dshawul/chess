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
#include "types.h"

/* Square, Rank, File */

bool rank_file_ok(int r, int f)
{
	return 0 <= r && r < NB_RANK && 0 <= f && f < NB_FILE;
}

bool square_ok(int sq)
{
	return A1 <= sq && sq <= H8;
}

int rank(int sq)
{
	assert(square_ok(sq));
	return sq / NB_FILE;
}

int file(int sq)
{
	assert(square_ok(sq));
	return sq % NB_FILE;
}

int square(int r, int f)
{
	assert(rank_file_ok(r, f));
	return NB_FILE * r + f;
}

int rank_mirror(int sq)
{
	assert(square_ok(sq));
	return sq ^ 070;
}

int file_mirror(int sq)
{
	assert(square_ok(sq));
	return sq ^ 7;
}

/* Piece */

bool piece_ok(int piece)
{
	return PAWN <= piece && piece < NO_PIECE;
}

bool is_slider(int piece)
{
	assert(piece_ok(piece));
	return BISHOP <= piece && piece <= QUEEN;
}

/* Color */

bool color_ok(int color)
{
	return color == WHITE || color == BLACK;
}

int opp_color(int color)
{
	assert(color_ok(color));
	return color ^ BLACK;
}

int color_of(int sq)
{
	assert(square_ok(sq));
	return (sq & 1) ^ BLACK;
}

