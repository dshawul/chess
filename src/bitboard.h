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
#include "types.h"

namespace bb {

const Bitboard FileA_bb = 0x0101010101010101ULL;
const Bitboard FileH_bb	= FileA_bb << FILE_H;
const Bitboard Rank1_bb	= 0x00000000000000FFULL;

// PInitialRank[color], PPromotionRank[color] are the 2nd and 8-th ranks relative to color
const Bitboard PInitialRank[NB_COLOR]   = { 0x000000000000FF00ULL, 0x00FF000000000000ULL };
const Bitboard PPromotionRank[NB_COLOR] = { 0xFF00000000000000ULL, 0x00000000000000FFULL };

const Bitboard HalfBoard[NB_COLOR] = { 0x00000000FFFFFFFFULL, 0xFFFFFFFF00000000ULL };

const Bitboard WhiteSquares = 0x55AA55AA55AA55AAULL;
const Bitboard BlackSquares = 0xAA55AA55AA55AA55ULL;

// Initialize: bitboards, magics
extern void init();
extern bool BitboardInitialized;

// Zobrist keys
extern Key zob[NB_COLOR][NB_PIECE][NB_SQUARE], zob_turn, zob_ep[NB_SQUARE], zob_castle[16];

// Between[s1][s2] is the segment ]s1,s2] when the angle (s1,s2) is a multiple of 45 degrees, 0
// otherwise. Direction[s1][s2] is the the half-line from s1 to s2 going all the way to the edge of
// the board
extern Bitboard Between[NB_SQUARE][NB_SQUARE];
extern Bitboard Direction[NB_SQUARE][NB_SQUARE];

// Bitboards to detect passed pawns
extern Bitboard InFront[NB_COLOR][NB_RANK];
extern Bitboard AdjacentFiles[NB_FILE];
extern Bitboard SquaresInFront[NB_COLOR][NB_SQUARE];
extern Bitboard PawnSpan[NB_COLOR][NB_SQUARE];
extern Bitboard Shield[NB_COLOR][NB_SQUARE];

// Occupancy independant attacks
extern Bitboard KAttacks[NB_SQUARE], NAttacks[NB_SQUARE];
extern Bitboard PAttacks[NB_COLOR][NB_SQUARE];
extern Bitboard BPseudoAttacks[NB_SQUARE], RPseudoAttacks[NB_SQUARE];

// Squares attacked by a bishop/rook for a given board occupancy
extern Bitboard bishop_attack(int sq, Bitboard occ);
extern Bitboard rook_attack(int sq, Bitboard occ);

// squares attacked by piece on sq, for a given occupancy
extern Bitboard piece_attack(int piece, int sq, Bitboard occ);

// print bitboard (ASCII style)
extern void print(std::ostream& ostrm, Bitboard b);

extern int kdist(int s1, int s2);

extern void set_bit(Bitboard *b, unsigned sq);
extern void clear_bit(Bitboard *b, unsigned sq);
extern bool test_bit(Bitboard b, unsigned sq);
extern Bitboard shift_bit(Bitboard b, int i);
extern bool several_bits(Bitboard b);

extern int pawn_push(int color, int sq);
extern Bitboard rank_bb(int r);
extern Bitboard file_bb(int f);

extern int count_bit(Bitboard b);
extern int lsb(Bitboard b);
extern int msb(Bitboard b);
extern int pop_lsb(Bitboard *b);

}	// namespace bb
