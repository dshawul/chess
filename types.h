/*
 * Zinc, an UCI chess interface. Copyright (C) 2012 Lucas Braesch.
 *
 * Zinc is free software: you can redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zinc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not,
 * see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include <cinttypes>
#include <cassert>
#include <iostream>
#include "operator.h"

/* Square, Rank, File */

#define NB_SQUARE 64
enum Square {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NoSquare
};
ENABLE_OPERATORS_ON(Square)

#define NB_RANK_FILE 8
enum Rank { Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8 };
enum File { FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH };
ENABLE_OPERATORS_ON(Rank)
ENABLE_OPERATORS_ON(File)

inline bool rank_file_ok(Rank r, File f)
{
	return 0 <= r && r < NB_RANK_FILE
	       && 0 <= f && f < NB_RANK_FILE;
}

inline Square square(Rank r, File f)
{
	assert(rank_file_ok(r, f));
	return Square(8 * int(r) + int(f));
}

inline bool square_ok(Square sq)
{
	return A1 <= sq && sq <= H8;
}

inline Rank rank(Square sq)
{
	assert(square_ok(sq));
	return Rank(int(sq) / 8);
}

inline File file(Square sq)
{
	assert(square_ok(sq));
	return File(int(sq) % 8);
}

/* Piece */

#define NB_PIECE 6
enum Piece { Pawn, Knight, Bishop, Rook, Queen, King, NoPiece };
ENABLE_OPERATORS_ON(Piece)

inline bool piece_ok(Piece piece)
{
	return Pawn <= piece && piece < NoPiece;
}

inline bool is_slider(Piece piece)
{
	assert(piece_ok(piece));
	return Bishop <= piece && piece <= Queen;
}

/* Color */

#define NB_COLOR 2
enum Color { White, Black, NoColor };
ENABLE_OPERATORS_ON(Color)

inline bool color_ok(Color color)
{
	return color == White || color == Black;
}

inline Color opp_color(Color color)
{
	assert(color_ok(color));
	return color ^ Black;
}

inline Square pawn_push(Color color, Square sq)
{
	assert(color_ok(color) && rank(sq) >= Rank2 && rank(sq) <= Rank7);
	return color ? sq - 8 : sq + 8;
}
