#pragma once
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

/* Square, Rank, File */

#define NB_SQUARE 64
enum {
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
#define NB_RANK_FILE 8
enum { Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8 };
enum { FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH };

inline bool rank_file_ok(unsigned r, unsigned f)
{
	return r < NB_RANK_FILE && f < NB_RANK_FILE;
}

inline unsigned square(unsigned r, unsigned f)
{
	assert(rank_file_ok(r, f));
	return 8 * r + f;
}

inline bool square_ok(unsigned sq)
{
	return sq <= H8;
}

inline unsigned rank(unsigned sq)
{
	assert(square_ok(sq));
	return sq / 8;
}

inline unsigned file(unsigned sq)
{
	assert(square_ok(sq));
	return sq % 8;
}

/* Piece */

#define NB_PIECE 6
enum { Pawn, Knight, Bishop, Rook, Queen, King, NoPiece };

inline bool piece_ok(unsigned piece)
{
	return piece < NoPiece;
}

inline bool is_slider(unsigned piece)
{
	assert(piece_ok(piece));
	return Bishop <= piece && piece <= Queen;
}

/* Color */

#define NB_COLOR 2
enum { White, Black, NoColor };

inline bool color_ok(unsigned color)
{
	return color <= Black;
}

inline unsigned opp_color(unsigned color)
{
	assert(color_ok(color));
	return color ^ 1;
}
