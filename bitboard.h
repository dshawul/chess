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
#pragma once
#include "types.h"

const Bitboard FileA_bb = 0x0101010101010101ULL << FILE_A;
const Bitboard FileH_bb = 0x0101010101010101ULL << FILE_H;
const Bitboard Rank1_bb = 0x00000000000000FFULL;

/* PInitialRank[color], PPromotionRank[color] are the 2nd and 8-th ranks relative to color */
const Bitboard PInitialRank[NB_COLOR]   = { 0x000000000000FF00ULL, 0x00FF000000000000ULL };
const Bitboard PPromotionRank[NB_COLOR] = { 0xFF00000000000000ULL, 0x00000000000000FFULL };

extern bool BitboardInitialized;

/* Zobrist keys */
extern Key zob[NB_COLOR][NB_PIECE][NB_SQUARE], zob_turn, zob_ep[NB_SQUARE], zob_castle[16];

/* Between[s1][s2] is the segment ]s1,s2] when the angle (s1,s2) is a multiple of 45 degrees, 0
 * otherwise. Direction[s1][s2] is the the half-line from s1 to s2 going all the way to the edge of
 * the board*/
extern Bitboard Between[NB_SQUARE][NB_SQUARE];
extern Bitboard Direction[NB_SQUARE][NB_SQUARE];

/* Occupancy independant attacks */
extern Bitboard KAttacks[NB_SQUARE], NAttacks[NB_SQUARE];
extern Bitboard PAttacks[NB_COLOR][NB_SQUARE];
extern Bitboard BPseudoAttacks[NB_SQUARE], RPseudoAttacks[NB_SQUARE];

/* Initialize: bitboards, zobrist, magics */
extern void init_bitboard();

/* Squares attacked by a bishop/rook for a given board occupancy */
extern Bitboard bishop_attack(int sq, Bitboard occ);
extern Bitboard rook_attack(int sq, Bitboard occ);

/* squares attacked by piece on sq, for a given occupancy */
extern Bitboard piece_attack(int piece, int sq, Bitboard occ);

/* Displays a bitboard on stdout: 'X' when a square is occupied and '.' otherwise */
extern void print_bitboard(std::ostream& ostrm, Bitboard b);

inline void set_bit(Bitboard *b, unsigned sq)	{ assert(square_ok(sq)); *b |= 1ULL << sq; }
inline void clear_bit(Bitboard *b, unsigned sq)	{ assert(square_ok(sq)); *b &= ~(1ULL << sq); }
inline bool test_bit(Bitboard b, unsigned sq)	{ assert(square_ok(sq)); return b & (1ULL << sq); }

inline Bitboard shift_bit(Bitboard b, int i) { assert(abs(i) < 64); return i > 0 ? b << i : b >> -i; }
inline bool several_bits(Bitboard b) { return b & (b - 1); }

inline Bitboard rank_bb(int r) { assert(rank_file_ok(r,0)); return Rank1_bb << (8 * r); }
inline Bitboard file_bb(int f) { assert(rank_file_ok(0,f)); return FileA_bb << f; }

/* lsb: assembly for x86_64, GCC or ICC */

inline int lsb(Bitboard b)
{
	Bitboard index;
	__asm__("bsfq %1, %0": "=r"(index): "rm"(b) );
	return index;
}

inline int pop_lsb(Bitboard *b)
{
	const int s = lsb(*b);
	*b &= *b - 1;
	return s;
}

/* Counts the number of bits set in b, using a loop. Performs best on sparsly populated bitboards */
int count_bit(Bitboard b);

/* Counts the number of bits set in b, up to a maximum of 15. Faster than count_bit for not so
 * sparsly populated bitboards */
int count_bit_max15(Bitboard b);