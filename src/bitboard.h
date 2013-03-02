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

#define FileA_bb	0x0101010101010101ULL
#define FileH_bb	(0x0101010101010101ULL << FILE_H)
#define Rank1_bb	0x00000000000000FFULL

// PInitialRank[color], PPromotionRank[color] are the 2nd and 8-th ranks relative to color
const Bitboard PInitialRank[NB_COLOR]   = { 0x000000000000FF00ULL, 0x00FF000000000000ULL };
const Bitboard PPromotionRank[NB_COLOR] = { 0xFF00000000000000ULL, 0x00000000000000FFULL };

const Bitboard HalfBoard[NB_COLOR] = { 0x00000000FFFFFFFFULL, 0xFFFFFFFF00000000ULL };

extern bool BitboardInitialized;

// Zobrist keys
extern Key zob[NB_COLOR][NB_PIECE][NB_SQUARE], zob_turn, zob_ep[NB_SQUARE], zob_castle[16];

// Between[s1][s2] is the segment ]s1,s2] when the angle (s1,s2) is a multiple of 45 degrees, 0
// otherwise. Direction[s1][s2] is the the half-line from s1 to s2 going all the way to the edge of
// the board
extern Bitboard Between[NB_SQUARE][NB_SQUARE];
extern Bitboard Direction[NB_SQUARE][NB_SQUARE];

// Bitboards to detect passed pawns
extern Bitboard InFront[NB_COLOR][NB_RANK_FILE];
extern Bitboard AdjacentFiles[NB_RANK_FILE];
extern Bitboard SquaresInFront[NB_COLOR][NB_SQUARE];
extern Bitboard PawnSpan[NB_COLOR][NB_SQUARE], PassedPawnMask[NB_COLOR][NB_SQUARE];
extern Bitboard Shield[NB_COLOR][NB_SQUARE];

// Occupancy independant attacks
extern Bitboard KAttacks[NB_SQUARE], NAttacks[NB_SQUARE];
extern Bitboard PAttacks[NB_COLOR][NB_SQUARE];
extern Bitboard BPseudoAttacks[NB_SQUARE], RPseudoAttacks[NB_SQUARE];

// Initialize: bitboards, zobrist, magics
extern void init_bitboard();

// Squares attacked by a bishop/rook for a given board occupancy
extern Bitboard bishop_attack(int sq, Bitboard occ);
extern Bitboard rook_attack(int sq, Bitboard occ);

// squares attacked by piece on sq, for a given occupancy
extern Bitboard piece_attack(int piece, int sq, Bitboard occ);

// Displays a bitboard on stdout: 'X' when a square is occupied and '.' otherwise
extern void print_bitboard(std::ostream& ostrm, Bitboard b);

inline void set_bit(Bitboard *b, unsigned sq)	{ assert(square_ok(sq)); *b |= 1ULL << sq; }
inline void clear_bit(Bitboard *b, unsigned sq)	{ assert(square_ok(sq)); *b &= ~(1ULL << sq); }
inline bool test_bit(Bitboard b, unsigned sq)	{ assert(square_ok(sq)); return b & (1ULL << sq); }

inline Bitboard shift_bit(Bitboard b, int i) { assert(abs(i) < 64); return i > 0 ? b << i : b >> -i; }
inline bool several_bits(Bitboard b) { return b & (b - 1); }

inline Bitboard rank_bb(int r) { assert(rank_file_ok(r,0)); return Rank1_bb << (8 * r); }
inline Bitboard file_bb(int f) { assert(rank_file_ok(0,f)); return FileA_bb << f; }

// King distance between squares
int kdist(int s1, int s2);

// GCC intrinscs for bitscan, reverse bitscan and popcount
inline int lsb(Bitboard b) { assert(b); return __builtin_ffsll(b) - 1; }
inline int msb(Bitboard b) { assert(b); return 63 - __builtin_clzll(b); }
inline int count_bit(Bitboard b) { return __builtin_popcountll(b); }

inline int pop_lsb(Bitboard *b)
{
	const int s = lsb(*b);
	*b &= *b - 1;
	return s;
}