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
#include <sstream>
#include <algorithm>
#include "move.h"

namespace
{
	std::string square_to_string(int sq)
	{
		std::ostringstream s;
		s << char(file(sq) + 'a') << char(rank(sq) + '1');
		return s.str();
	}
}

int move_is_check(const Board& B, move_t m)
/* Tests if a move is checking the enemy king. General case: direct checks, revealed checks (when
 * piece is a dchecker moving out of ray). Special cases: castling (check by the rook), en passant
 * (check through a newly revealed sliding attacker, once the ep capture square has been vacated)
 * returns 2 for a discovered check, 1 for any other check, 0 otherwise */
{
	const int us = B.get_turn(), them = opp_color(us);
	const int fsq = m.get_fsq(), tsq = m.get_tsq(), flag = m.get_flag();
	int kpos = B.get_king_pos(them);

	// test discovered check
	if ( (test_bit(B.st().dcheckers, fsq))		// discovery checker
	     && (!test_bit(Direction[kpos][fsq], tsq)))	// move out of its dc-ray
		return 2;
	// test direct check
	else if (flag != PROMOTION) {
		const int piece = B.get_piece_on(fsq);
		Bitboard tss = piece == PAWN ? PAttacks[us][tsq]
		               : piece_attack(piece, tsq, B.st().occ);
		if (test_bit(tss, kpos))
			return 1;
	}

	if (flag == EN_PASSANT) {
		Bitboard occ = B.st().occ;
		// play the ep capture on occ
		clear_bit(&occ, fsq);
		clear_bit(&occ, tsq + (us ? 8 : -8));
		set_bit(&occ, tsq);
		// test for new sliding attackers to the enemy king
		if ((B.get_RQ(us) & RPseudoAttacks[kpos] & rook_attack(kpos, occ))
		    || (B.get_BQ(us) & BPseudoAttacks[kpos] & bishop_attack(kpos, occ)))
			return 2;	// discovered check through the fsq or the ep captured square
	} else if (flag == CASTLING) {
		// position of the Rook after playing the castling move
		int rook_sq = (fsq + tsq) / 2;
		Bitboard occ = B.st().occ;
		clear_bit(&occ, fsq);
		Bitboard RQ = B.get_RQ(us);
		set_bit(&RQ, rook_sq);
		if (RQ & RPseudoAttacks[kpos] & rook_attack(kpos, occ))
			return 1;	// direct check by the castled rook
	} else if (flag == PROMOTION) {
		// test check by the promoted piece
		Bitboard occ = B.st().occ;
		clear_bit(&occ, fsq);
		if (test_bit(piece_attack(m.get_prom(), tsq, occ), kpos))
			return 1;	// direct check by the promoted piece
	}

	return 0;
}

int move_is_cop(const Board& B, move_t m)
{
	return piece_ok(B.get_piece_on(m.get_tsq()))
		|| m.get_flag() == EN_PASSANT
		|| m.get_flag() == PROMOTION;
}

move_t string_to_move(const Board& B, const std::string& s)
{
	move_t m;
	m.set_fsq(square(s[1]-'1', s[0]-'a'));
	m.set_tsq(square(s[3]-'1', s[2]-'a'));
	m.set_flag(NORMAL);
	
	if (B.get_piece_on(m.get_fsq()) == PAWN && m.get_tsq() == B.st().epsq)
		m.set_flag(EN_PASSANT);

	if (s[4])
		m.set_prom(PieceLabel[BLACK].find(s[4]));
	else if (B.get_piece_on(m.get_fsq()) == KING && (m.get_fsq()+2 == m.get_tsq() || m.get_tsq()+2==m.get_fsq()))
		m.set_flag(CASTLING);

	return m;
}

std::string move_to_string(move_t m)
{
	std::ostringstream s;

	s << square_to_string(m.get_fsq());
	s << square_to_string(m.get_tsq());

	if (m.get_flag() == PROMOTION)
		s << PieceLabel[BLACK][m.get_prom()];

	return s.str();
}

std::string move_to_san(const Board& B, move_t m)
{
	std::ostringstream s;
	const int us = B.get_turn();
	const int piece = B.get_piece_on(m.get_fsq());
	const bool capture = m.get_flag() == EN_PASSANT || B.get_piece_on(m.get_tsq()) != NO_PIECE;

	if (piece != PAWN) {
		if (m.get_flag() == CASTLING) {
			if (file(m.get_tsq()) == FILE_C)
				s << "OOO";
			else
				s << "OO";
		} else {
			s << PieceLabel[WHITE][piece];
			Bitboard b = B.get_pieces(us, piece)
			             & piece_attack(piece, m.get_tsq(), B.st().occ)
			             & ~B.st().pinned;
			if (several_bits(b)) {
				clear_bit(&b, m.get_fsq());
				const int sq = lsb(b);
				if (file(m.get_fsq()) == file(sq))
					s << char(rank(m.get_fsq()) + '1');
				else
					s << char(file(m.get_fsq()) + 'a');
			}
		}
	} else if (capture)
		s << char(file(m.get_fsq()) + 'a');

	if (capture)
		s << 'x';

	if (m.get_flag() != CASTLING)
		s << square_to_string(m.get_tsq());

	if (m.get_flag() == PROMOTION)
		s << PieceLabel[WHITE][m.get_prom()];

	if (move_is_check(B, m))
		s << '+';

	return s.str();
}

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
	int from_value = see_val[B.get_piece_on(m.get_fsq())];
	int to_value = see_val[B.get_piece_on(m.get_tsq())];

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
	int fsq = m.get_fsq(), tsq = m.get_tsq();
	int stm = B.get_color_on(fsq);	// side to move
	uint64_t attackers, stm_attackers;
	int swap_list[32], sl_idx = 1;
	uint64_t occ = B.st().occ;
	int piece = B.get_piece_on(fsq), capture;

	// Determine captured piece
	if (m.get_flag() == EN_PASSANT) {
		clear_bit(&occ, pawn_push(opp_color(stm), tsq));
		capture = PAWN;
	} else
		capture = B.get_piece_on(tsq);
	assert(capture != KING);

	swap_list[0] = see_val[capture];
	clear_bit(&occ, fsq);

	// Handle promotion
	if (m.get_flag() == PROMOTION) {
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

int mvv_lva(const Board& B, move_t m)
{
	// Queen is the best capture available (King can't be captured since move is legal)
	static const int victim_value[NB_PIECE+1] = {1, 2, 2, 3, 4, 0, 0};
	// King is the best attacker (since move is legal) followed by Pawn etc.
	static const int attacker_value[NB_PIECE] = {4, 3, 3, 2, 1, 5};

	const int piece_value = attacker_value[B.get_piece_on(m.get_fsq())];
	const int capture_value = victim_value[m.get_flag() == EN_PASSANT ? PAWN : B.get_piece_on(m.get_tsq())];
	const int promotion_value = m.get_flag() == PROMOTION ? victim_value[m.get_prom()] : 0;
	
	return (capture_value + promotion_value) * 8 + piece_value;	
}