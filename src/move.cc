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
	const int fsq = m.fsq(), tsq = m.tsq(), flag = m.flag();
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
		if (test_bit(piece_attack(m.prom(), tsq, occ), kpos))
			return 1;	// direct check by the promoted piece
	}

	return 0;
}

bool move_is_cop(const Board& B, move_t m)
{
	return piece_ok(B.get_piece_on(m.tsq()))
	       || m.flag() == EN_PASSANT
	       || m.flag() == PROMOTION;
}

bool move_is_pawn_threat(const Board& B, move_t m)
{
	if (B.get_piece_on(m.fsq()) == PAWN) {
		const int us = B.get_turn(), them = opp_color(us), sq = m.tsq();
		
		if (test_bit(HalfBoard[them], sq)) {
			const Bitboard our_pawns = B.get_pieces(us, PAWN), their_pawns = B.get_pieces(them, PAWN);
			return !(PawnSpan[us][sq] & their_pawns)
				&& !(SquaresInFront[us][sq] & (our_pawns | their_pawns));
		}
	}
	
	return false;
}

move_t string_to_move(const Board& B, const std::string& s)
{
	move_t m;
	m.fsq(square(s[1]-'1', s[0]-'a'));
	m.tsq(square(s[3]-'1', s[2]-'a'));
	m.flag(NORMAL);

	if (B.get_piece_on(m.fsq()) == PAWN && m.tsq() == B.st().epsq)
		m.flag(EN_PASSANT);

	if (s[4]) {
		m.flag(PROMOTION);
		m.prom(PieceLabel[BLACK].find(s[4]));
	} else if (B.get_piece_on(m.fsq()) == KING && (m.fsq()+2 == m.tsq() || m.tsq()+2==m.fsq()))
		m.flag(CASTLING);

	return m;
}

std::string move_to_string(move_t m)
{
	std::ostringstream s;

	s << square_to_string(m.fsq());
	s << square_to_string(m.tsq());

	if (m.flag() == PROMOTION)
		s << PieceLabel[BLACK][m.prom()];

	return s.str();
}

std::string move_to_san(const Board& B, move_t m)
{
	std::ostringstream s;
	const int us = B.get_turn();
	const int fsq = m.fsq(), tsq = m.tsq();
	const int piece = B.get_piece_on(fsq);
	const bool capture = m.flag() == EN_PASSANT || B.get_piece_on(tsq) != NO_PIECE;

	if (piece != PAWN) {
		if (m.flag() == CASTLING) {
			if (file(tsq) == FILE_C)
				s << "OOO";
			else
				s << "OO";
		} else {
			s << PieceLabel[WHITE][piece];
			Bitboard b = B.get_pieces(us, piece)
			             & piece_attack(piece, tsq, B.st().occ)
			             & ~B.st().pinned;
			if (several_bits(b)) {
				clear_bit(&b, fsq);
				const int sq = lsb(b);
				if (file(fsq) == file(sq))
					s << char(rank(fsq) + '1');
				else
					s << char(file(fsq) + 'a');
			}
		}
	} else if (capture)
		s << char(file(fsq) + 'a');

	if (capture)
		s << 'x';

	if (m.flag() != CASTLING)
		s << square_to_string(tsq);

	if (m.flag() == PROMOTION)
		s << PieceLabel[WHITE][m.prom()];

	if (move_is_check(B, m))
		s << '+';

	return s.str();
}

namespace
{
	const int see_val[NB_PIECE+1] = {vOP, vN, vB, vR, vQ, vK, 0};

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

int calc_see(const Board& B, move_t m)
/* SEE largely inspired by Glaurung. Adapted and improved to handle promotions and en-passant. */
{
	int fsq = m.fsq(), tsq = m.tsq();
	int stm = B.get_color_on(fsq);	// side to move
	uint64_t attackers, stm_attackers;
	int swap_list[32], sl_idx = 1;
	uint64_t occ = B.st().occ;
	int piece = B.get_piece_on(fsq), capture;

	// Determine captured piece
	if (m.flag() == EN_PASSANT) {
		clear_bit(&occ, pawn_push(opp_color(stm), tsq));
		capture = PAWN;
	} else
		capture = B.get_piece_on(tsq);
	assert(capture != KING);

	swap_list[0] = see_val[capture];
	clear_bit(&occ, fsq);

	// Handle promotion
	if (m.flag() == PROMOTION) {
		swap_list[0] += see_val[m.prom()] - see_val[PAWN];
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
		{"k6K/8/4b3/8/3N4/8/8/8 w - -", "d4e6", vB},
		{"k6K/3p4/4b3/8/3N4/8/8/8 w - -", "d4e6", 0},
		{"k6K/3p4/4b3/8/3N4/8/8/4R3 w - -", "d4e6", vB-vN+vOP},
		{"k3r2K/3p4/4b3/8/3N4/8/4R3/4R3 w - -", "d4e6", vB-vN+vOP},
		{"k6K/3P4/8/8/8/8/8/8 w - -", "d7d8q", vQ-vOP},
		{"k6K/3P4/2n5/8/8/8/8/8 w - -", "d7d8q", -vOP},
		{"k6K/3P4/2n1N3/8/8/8/8/8 w - -", "d7d8q", -vOP+vN},
		{"k6K/3PP3/2n5/b7/7B/8/8/3R4 w - -", "d7d8q", vN+vB-2*vOP},
		{"3R3K/k3P3/8/b7/8/8/8/8 b - -", "a5d8", vR-vB+vOP-vQ},
		{NULL, NULL, 0}
	};

	Board B;

	for (int i = 0; test[i].fen; ++i) {
		B.set_fen(test[i].fen);
		std::cout << test[i].fen << '\t' << test[i].move << std::endl;
		move_t m = string_to_move(B, test[i].move);

		const int s = calc_see(B, m);

		if (s != test[i].value) {
			std::cout << B << "SEE = " << s << std::endl;
			std::cout << "should be " << test[i].value << std::endl;
			return false;
		}
	}

	return true;
}

int mvv_lva(const Board& B, move_t m)
{
	// Queen is the best capture available (King can't be captured since move is legal)
	static const int victim[NB_PIECE+1] = {1, 2, 2, 3, 4, 0, 0};
	// King is the best attacker (since move is legal) followed by Pawn etc.
	static const int attacker[NB_PIECE] = {4, 3, 3, 2, 1, 5};

	int victim_value = victim[m.flag() == EN_PASSANT ? PAWN : B.get_piece_on(m.tsq())]
	                   + (m.flag() == PROMOTION ? victim[m.prom()] - victim[PAWN] : 0);
	int attacker_value = attacker[B.get_piece_on(m.fsq())];

	return victim_value * 8 + attacker_value;
}

bool refute(const Board& B, move_t m1, move_t m2)
{
	if (!m2)
		return false;

	const int m1fsq = m1.fsq(), m1tsq = m1.tsq();
	const int m2fsq = m2.fsq(), m2tsq = m2.tsq();
	
	// move the threatened piece
	if (m1fsq == m2tsq)
		return true;
	
	// block the threat path
	if (test_bit(Between[m2fsq][m2tsq], m1tsq))
		return true;
	
	// defend the threatened square
	if (Material[B.get_piece_on(m2tsq)].op <= Material[B.get_piece_on(m2fsq)].op)
	{		
		const int m1piece = m1.flag() == PROMOTION ? m1.prom() : B.get_piece_on(m1fsq);
		const Bitboard b = m1piece == PAWN ? PAttacks[B.get_turn()][m1tsq]
			: piece_attack(m1piece, m1tsq, B.st().occ);
		
		if (test_bit(b, m2tsq))
			return true;
	}
	
	return false;
}