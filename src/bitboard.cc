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
#include <cstring>
#include "bitboard.h"
#include "prng.h"

bool BitboardInitialized = false;

Key zob[NB_COLOR][NB_PIECE][NB_SQUARE], zob_turn, zob_ep[NB_SQUARE], zob_castle[16];

Bitboard Between[NB_SQUARE][NB_SQUARE];
Bitboard Direction[NB_SQUARE][NB_SQUARE];

Bitboard InFront[NB_COLOR][8];
Bitboard AdjacentFiles[8];
Bitboard SquaresInFront[NB_COLOR][NB_SQUARE];
Bitboard PawnSpan[NB_COLOR][NB_SQUARE];
Bitboard Shield[NB_COLOR][NB_SQUARE];

Bitboard KAttacks[NB_SQUARE], NAttacks[NB_SQUARE];
Bitboard PAttacks[NB_COLOR][NB_SQUARE];
Bitboard BPseudoAttacks[NB_SQUARE], RPseudoAttacks[NB_SQUARE];

int KingDistance[NB_SQUARE][NB_SQUARE];

int kdist(int s1, int s2)
{
	return KingDistance[s1][s2];
}

namespace {

void safe_add_bit(Bitboard *b, int r, int f)
{
	if (rank_file_ok(r, f))
		set_bit(b, square(r, f));
}

}	// namespace

void init_bitboard()
{
	if (BitboardInitialized) return;

	/* Zobrist: zob[c][p][s], zob_turn, zob_castle[cr], zob_ep[s] */

	PRNG prng;
	for (int c = WHITE; c <= BLACK; c++)
		for (int p = PAWN; p <= KING; p++)
			for (int sq = A1; sq <= H8; zob[c][p][sq++] = prng.rand());

	zob_turn = prng.rand();
	for (int crights = 0; crights < 16; zob_castle[crights++] = prng.rand());
	for (int sq = A1; sq <= H8; zob_ep[sq++] = prng.rand());

	/* NAttacks[s], KAttacks[s], Pattacks[c][s] */

	const int Kdir[8][2] = { { -1, -1}, { -1, 0}, { -1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1} };
	const int Ndir[8][2] = { { -2, -1}, { -2, 1}, { -1, -2}, { -1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1} };
	const int Pdir[2][2] = { {1, -1}, {1, 1} };

	for (int sq = A1; sq <= H8; ++sq) {
		const int r = rank(sq);
		const int f = file(sq);

		for (int d = 0; d < 8; d++) {
			safe_add_bit(&NAttacks[sq], r + Ndir[d][0], f + Ndir[d][1]);
			safe_add_bit(&KAttacks[sq], r + Kdir[d][0], f + Kdir[d][1]);
		}

		for (int d = 0; d < 2; d++) {
			safe_add_bit(&PAttacks[WHITE][sq], r + Pdir[d][0], f + Pdir[d][1]);
			safe_add_bit(&PAttacks[BLACK][sq], r - Pdir[d][0], f - Pdir[d][1]);
		}

		BPseudoAttacks[sq] = bishop_attack(sq, 0);
		RPseudoAttacks[sq] = rook_attack(sq, 0);
	}

	/* Between[s1][s2] and Direction[s1][s2] */

	for (int sq = A1; sq <= H8; ++sq) {
		int r = rank(sq);
		int f = file(sq);

		for (int i = 0; i < 8; i++) {
			Bitboard mask = 0;
			const int dr = Kdir[i][0], df = Kdir[i][1];
			int _r, _f, _sq;

			for (_r = r + dr, _f = f + df; rank_file_ok(_r, _f); _r += dr, _f += df) {
				_sq = square(_r, _f);
				mask |= 1ULL << _sq;
				Between[sq][_sq] = mask;
			}

			Bitboard direction = mask;

			while (mask) {
				_sq = pop_lsb(&mask);
				Direction[sq][_sq] = direction;
			}
		}
	}

	/* AdjacentFile[f] and InFront[c][r] */

	for (int f = FILE_A; f <= FILE_H; f++) {
		if (f > FILE_A) AdjacentFiles[f] |= file_bb(f - 1);
		if (f < FILE_H) AdjacentFiles[f] |= file_bb(f + 1);
	}

	for (int rw = RANK_7, rb = RANK_2; rw >= RANK_1; rw--, rb++) {
		InFront[WHITE][rw] = InFront[WHITE][rw + 1] | rank_bb(rw + 1);
		InFront[BLACK][rb] = InFront[BLACK][rb - 1] | rank_bb(rb - 1);
	}

	/* SquaresInFront[c][sq], PawnSpan[c][sq], Shield[c][sq] */

	for (int us = WHITE; us <= BLACK; ++us) {
		for (int sq = A1; sq <= H8; ++sq) {
			const int r = rank(sq), f = file(sq);
			SquaresInFront[us][sq] = file_bb(f) & InFront[us][r];
			PawnSpan[us][sq] = AdjacentFiles[f] & InFront[us][r];
			Shield[us][sq] = KAttacks[sq] & InFront[us][r];
		}
	}

	/* KingDistance[s1][s2] */

	for (int s1 = A1; s1 <= H8; ++s1)
		for (int s2 = A1; s2 <= H8; ++s2)
			KingDistance[s1][s2] = std::max(std::abs(file(s1) - file(s2)), std::abs(rank(s1) - rank(s2)));

	BitboardInitialized = true;
}

void print_bitboard(std::ostream& ostrm, Bitboard b)
{
	for (int r = RANK_8; r >= RANK_1; --r) {
		for (int f = FILE_A; f <= FILE_H; ++f) {
			int sq = square(r, f);
			char c = test_bit(b, sq) ? 'X' : '.';
			ostrm << ' ' << c;
		}
		ostrm << std::endl;
	}
}

Bitboard piece_attack(int piece, int sq, Bitboard occ)
/* Generic attack function for pieces (not pawns). Typically, this is used in a block that loops on
 * piece, so inling this allows some optimizations in the calling code, thanks to loop unrolling */
{
	assert(BitboardInitialized);
	assert(KNIGHT <= piece && piece <= KING && square_ok(sq));

	if (piece == KNIGHT)
		return NAttacks[sq];
	else if (piece == BISHOP)
		return bishop_attack(sq, occ);
	else if (piece == ROOK)
		return rook_attack(sq, occ);
	else if (piece == QUEEN)
		return bishop_attack(sq, occ) | rook_attack(sq, occ);
	else
		return KAttacks[sq];
}

