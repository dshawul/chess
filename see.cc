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
#include "see.h"

namespace
{
	Bitboard calc_attackers(const Board& B, int sq, Bitboard occ)
	{
		assert(square_ok(sq));
		
		return (B.get_RQ() & RPseudoAttacks[sq] & rook_attack(sq, occ))
		       | (B.get_BQ() & BPseudoAttacks[sq] & bishop_attack(sq, occ))
		       | (NAttacks[sq] & B.get_N())
		       | (KAttacks[sq] & B.get_K())
		       | (PAttacks[WHITE][sq] & B.get_pieces(BLACK, PAWN))
		       | (PAttacks[BLACK][sq] & B.get_pieces(WHITE, PAWN));
	}
}

const int see_val[NB_PIECE+1] = {100, 300, 300, 500, 1000, 20000, 0};

int see_sign(const Board& B, move_t m)
{
	int from_value = see_val[B.get_piece_on(m.fsq)];
	int to_value = see_val[B.get_piece_on(m.tsq)];

	/* Note that we do not adjust to_value for en-passant and promotions. The only case where we can
	 * directly conclude on the (strict) signum of the SEE, is when the promotion is also a capture,
	 * as the capture victim is necessarly a piece, worth more than a pawn.
	 * For en-passant, to_value = 0, and for a promotion to_value is generally zero, except when the
	 * promotion is a capture, in which case to_value is the value of the victim. */

	return (to_value > from_value) ? 1 : see(B, m);
}

int see(const Board& B, move_t m)
/* SEE largely inspired by Glaurung. Adapted and improved to handle promotions and en-passant. */
{
	int fsq = m.fsq, tsq = m.tsq;
	int stm = B.get_color_on(fsq);	// side to move
	uint64_t attackers, stm_attackers;
	int swap_list[32], sl_idx = 1;
	uint64_t occ = B.st().occ;
	int piece = B.get_piece_on(fsq), capture;

	// Determine captured piece
	if (m.flag == EN_PASSANT) {
		clear_bit(&occ, pawn_push(opp_color(stm), tsq));
		capture = PAWN;
	} else
		capture = B.get_piece_on(tsq);
	assert(capture != KING);

	swap_list[0] = see_val[capture];
	clear_bit(&occ, fsq);

	// Handle promotion
	if (m.flag == PROMOTION) {
		swap_list[0] += see_val[m.get_prom()] - see_val[PAWN];
		capture = QUEEN;
	} else
		capture = B.get_piece_on(fsq);

	// If the opponent has no attackers we are finished
	attackers = test_bit(B.st().attacked, tsq) ? calc_attackers(B, tsq, occ) : 0;
	stm = opp_color(stm);
	stm_attackers = attackers & B.get_pieces(stm);
	if (!stm_attackers)
		return swap_list[0];

	/* The destination square is defended, which makes things more complicated. We proceed by
	 * building a "swap list" containing the material gain or loss at each stop in a sequence of
	 * captures to the destination square, where the sides alternately capture, and always capture
	 * with the least valuable piece. After each capture, we look for new X-ray attacks from behind
	 * the capturing piece. */
	do {
		/* Locate the least valuable attacker for the side to move. The loop below looks like it is
		 * potentially infinite, but it isn't. We know that the side to move still has at least one
		 * attacker left. */
		for (piece = PAWN; !(stm_attackers & B.get_pieces(stm, piece)); ++piece)
			assert(piece < KING);

		// remove the piece (from wherever it came)
		clear_bit(&occ, lsb(stm_attackers & B.get_pieces(stm, piece)));
		// scan for new X-ray attacks through the vacated square
		attackers |= (B.get_RQ() & RPseudoAttacks[tsq] & rook_attack(tsq, occ))
			| (B.get_BQ() & BPseudoAttacks[tsq] & bishop_attack(tsq, occ));
		// cut out pieces we've already done
		attackers &= occ;

		// add the new entry to the swap list (beware of promoting pawn captures)
		assert(sl_idx < 32);
		swap_list[sl_idx] = -swap_list[sl_idx - 1] + see_val[capture];
		if (piece == PAWN && test_bit(PPromotionRank[stm], tsq)) {
			swap_list[sl_idx] += see_val[QUEEN] - see_val[PAWN];
			capture = QUEEN;
		} else
			capture = piece;
		sl_idx++;

		stm = opp_color(stm);
		stm_attackers = attackers & B.get_pieces(stm);

		// Stop after a king capture
		if (piece == KING && stm_attackers) {
			assert(sl_idx < 32);
			swap_list[sl_idx++] = see_val[KING];
			break;
		}
	} while (stm_attackers);

	/* Having built the swap list, we negamax through it to find the best achievable score from the
	 * point of view of the side to move */
	while (--sl_idx)
		swap_list[sl_idx-1] = std::min(-swap_list[sl_idx], swap_list[sl_idx-1]);

	return swap_list[0];
}

bool test_see()
{
	struct TestSEE {
		const char *fen, *move;
		int value;
	};

	TestSEE test[] = {
		{"k6K/8/4b3/8/3N4/8/8/8 w - -", "d4e6", 300},
		{"k6K/3p4/4b3/8/3N4/8/8/8 w - -", "d4e6", 0},
		{"k6K/3p4/4b3/8/3N4/8/8/4R3 w - -", "d4e6", 100},
		{"k3r2K/3p4/4b3/8/3N4/8/4R3/4R3 w - -", "d4e6", 100},
		{"k6K/3P4/8/8/8/8/8/8 w - -", "d7d8q", 900},
		{"k6K/3P4/2n5/8/8/8/8/8 w - -", "d7d8q", -100},
		{"k6K/3P4/2n1N3/8/8/8/8/8 w - -", "d7d8q", 200},
		{"k6K/3PP3/2n5/b7/7B/8/8/3R4 w - -", "d7d8q", 400},
		{"3R3K/k3P3/8/b7/8/8/8/8 b - -", "a5d8", -700},
		{NULL, NULL, 0}
	};
	
	Board B;
	
	for (int i = 0; test[i].fen; ++i) {
		B.set_fen(test[i].fen);
		std::cout << test[i].fen << '\t' << test[i].move << std::endl;
		move_t m = string_to_move(B, test[i].move);		
		
		int s = see(B, m);		
				
		if (s != test[i].value) {
			std::cout << B << "SEE = " << see(B, m) << std::endl;
			std::cout << "should be " << test[i].value << std::endl;
			return false;
		}
	}
	
	return true;
}