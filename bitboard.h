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

/* Most used File bitboards */
const uint64_t FileA_bb = 0x0101010101010101ULL << FILE_A;
const uint64_t FileH_bb = 0x0101010101010101ULL << FILE_H;

/* PInitialRank[color], PPromotionRank[color] are the 2nd and 8-th ranks relative to color */
const uint64_t PInitialRank[NB_COLOR]   = { 0x000000000000FF00ULL, 0x00FF000000000000ULL };
const uint64_t PPromotionRank[NB_COLOR] = { 0xFF00000000000000ULL, 0x00000000000000FFULL };

extern bool BitboardInitialized;

/* Zobrist keys */
extern Key zob[NB_COLOR][NB_PIECE][NB_SQUARE], zob_turn, zob_ep[NB_SQUARE], zob_castle[16];

/* Between[s1][s2] is the segment ]s1,s2] when the angle (s1,s2) is a multiple of 45 degrees, 0
 * otherwise. Direction[s1][s2] is the the half-line from s1 to s2 going all the way to the edge of
 * the board*/
extern uint64_t Between[NB_SQUARE][NB_SQUARE];
extern uint64_t Direction[NB_SQUARE][NB_SQUARE];

/* Occupancy independant attacks */
extern uint64_t KAttacks[NB_SQUARE], NAttacks[NB_SQUARE];
extern uint64_t PAttacks[NB_COLOR][NB_SQUARE];
extern uint64_t BPseudoAttacks[NB_SQUARE], RPseudoAttacks[NB_SQUARE];

/* Initialize: bitboards, zobrist, magics */
extern void init_bitboard();

/* Squares attacked by a bishop/rook for a given board occupancy */
extern uint64_t bishop_attack(unsigned sq, uint64_t occ);
extern uint64_t rook_attack(unsigned sq, uint64_t occ);

/* squares attacked by piece on sq, for a given occupancy */
extern uint64_t piece_attack(int piece, unsigned sq, uint64_t occ);

/* Displays a bitboard on stdout: 'X' when a square is occupied and '.' otherwise */
extern void print_bitboard(std::ostream& ostrm, uint64_t b);

inline void set_bit(uint64_t *b, unsigned sq)
{
	assert(square_ok(sq));
	*b |= 1ULL << sq;
}

inline void clear_bit(uint64_t *b, unsigned sq)
{
	assert(square_ok(sq));
	*b &= ~(1ULL << sq);
}

inline bool test_bit(uint64_t b, unsigned sq)
{
	assert(square_ok(sq));
	return b & (1ULL << sq);
}

inline uint64_t shift_bit(uint64_t b, int i)
/* shift_bit() extends << in allowing negative shifts. This is useful to keep some pawn related code
 * generic and simple. */
{
	assert(abs(i) < 64);
	return i > 0 ? b << i : b >> -i;
}

inline bool several_bits(uint64_t b)
{
	return b & (b - 1);
}

/* lsb: assembly for x86_64, GCC or ICC */

inline unsigned lsb(Bitboard b) {
	Bitboard index;
	__asm__("bsfq %1, %0": "=r"(index): "rm"(b) );
	return index;
}

inline unsigned pop_lsb(Bitboard *b) {
	const unsigned s = lsb(*b);
	*b &= *b - 1;
	return s;
}
