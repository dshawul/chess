#include "board.h"

bool move_equal(move_t m1, move_t m2)
{
	return m1.fsq == m2.fsq && m1.tsq == m2.tsq && m1.promotion == m2.promotion && m1.ep == m2.ep;
}

bool move_is_castling(const Board *B, move_t m)
{
	return B->piece_on[m.fsq] == King
		&& (m.tsq == m.fsq - 2 || m.tsq == m.fsq + 2);
}

bool move_ok(Board *B, move_t m)
/* Determines whether a move is legal in the current board position. This function makes no
 * assumption or shortcut (like assuming the move is at least pseudo-legal for example), and tests
 * every single component of the move_t structure. So it is quite long and complex, but its execution
 * time should be rather fast (with the notable exception of check evasions)
 * */
{
	const unsigned us = B->turn, them = opp_color(us), kpos = B->king_pos[us];
	const unsigned piece = B->piece_on[m.fsq], capture = B->piece_on[m.tsq];
	const unsigned fsq = m.fsq, tsq = m.tsq;
	
	// verify fsq and piece
	if (color_on(B, fsq) != us)
		return false;
	
	// verify tsq and capture
	if (color_on(B, tsq) != (capture == NoPiece ? NoColor : them))
		return false;

	bool pinned = test_bit(B->st->pinned, fsq);
	uint64_t pin_ray = Direction[kpos][fsq];
	
	// pawn moves
	if (piece == Pawn) {
		// generic self check
		if (pinned && !test_bit(pin_ray, tsq))
			return false;
			
		// m.promotion is a valid promotion piece <==> tsq is on the 8th rank
		if ((bool)test_bit(PPromotionRank[us], tsq) != (Knight <= m.promotion && m.promotion <= Queen))
			return false;

		const bool is_capture = test_bit(PAttacks[us][fsq], tsq);
		const bool is_push = tsq == (us ? fsq - 8 : fsq + 8);
		const bool is_dpush = tsq == (us ? fsq - 16 : fsq + 16);

		if (m.ep) {
			// must be a capture and land on the epsq
			if (B->st->epsq != tsq || !is_capture)
				return false;
			
			// self check through en passant captured pawn
			const unsigned ep_cap = pawn_push(them, tsq);
			const uint64_t ray = Direction[kpos][ep_cap];
			if (ray) {
				uint64_t occ = B->st->occ;
				clear_bit(&occ, ep_cap);
				set_bit(&occ, tsq);
				
				const uint64_t RQ = get_RQ(B, them);
				if (ray & RQ & RPseudoAttacks[kpos] & rook_attack(kpos, occ))
					return false;
				
				const uint64_t BQ = get_BQ(B, them);
				if (ray & BQ & BPseudoAttacks[kpos] & bishop_attack(kpos, occ))
					return false;
			}
			
			return true;
		}
		
		if (is_capture)
			return capture != NoPiece;
		else if (is_push)
			return capture == NoPiece;
		else if (is_dpush)
			return capture == NoPiece
				&& B->piece_on[pawn_push(us, fsq)] == NoPiece
				&& test_bit(PInitialRank[us], fsq);
		else
			return false;
	}

	if (m.ep || m.promotion != NoPiece)
		return false;
		
	uint64_t tss = piece_attack(piece, fsq, B->st->occ);
	
	if (piece == King) {
		// castling moves
		if (move_is_castling(B, m)) {
			uint64_t safe /* must not be attacked */, empty /* must be empty */;
			
			if ((fsq+2 == tsq) && (B->st->crights & (OO << (2 * us)))) {
				empty = safe = 3ULL << (m.fsq + 1);
				return !(B->st->attacked & safe) && !(B->st->occ & empty);
			} else if ((fsq-2 == tsq) && (B->st->crights & (OOO << (2 * us)))) {
				safe = 3ULL << (m.fsq - 2);
				empty = safe | (1ULL << (m.fsq - 3));
				return !(B->st->attacked & safe) && !(B->st->occ & empty);
			} else
				return false;
		}
		
		// normal king moves
		return test_bit(tss & ~B->st->attacked, tsq);
	}

	// piece moves
	if (pinned) tss &= pin_ray;
	if (!test_bit(tss, tsq)) return false;

	// check evasion: use the slow method (optimize later if needs be)
	if (board_is_check(B)) {
		play(B, m);
		bool ok = !test_bit(calc_attacks(B, them), kpos);
		undo(B);
		return ok;
	}
	
	return true;
}

move_t string_to_move(const Board *B, const char *s)
{
	assert(B->initialized && s);
	move_t m;

	m.fsq = square(s[1]-'1', s[0]-'a');
	m.tsq = square(s[3]-'1', s[2]-'a');
	m.ep = B->piece_on[m.fsq] == Pawn && m.tsq == B->st->epsq;

	if (s[4]) {
		unsigned piece;
		for (piece = Knight; piece <= Queen; piece++)
			if (PieceLabel[Black][piece] == s[4])
				break;
		assert(piece < King);
		m.promotion = piece;
	} else
		m.promotion = NoPiece;

	return m;
}

void move_to_string(const Board *B, move_t m, char *s)
{
	assert(B->initialized && s);
	
	*s++ = file(m.fsq) + 'a';
	*s++ = rank(m.fsq) + '1';
	assert(square_ok(m.fsq));

	*s++ = file(m.tsq) + 'a';
	*s++ = rank(m.tsq) + '1';
	assert(square_ok(m.tsq));

	if (piece_ok(m.promotion))
		*s++ = PieceLabel[Black][m.promotion];

	*s++ = '\0';
}

void move_to_san(const Board *B, move_t m, char *s)
{
	assert(B->initialized && s);
	const unsigned us = B->turn, piece = B->piece_on[m.fsq];
	const bool capture = m.ep || B->piece_on[m.tsq] != NoPiece, castling = move_is_castling(B, m);
	
	if (piece != Pawn) {
		if (castling) {
			*s++ = 'O'; *s++ = 'O';
			if (file(m.tsq) == FileC)
				*s++ = 'O';
		} else if (piece != King) {
			*s++ = PieceLabel[White][piece];
			uint64_t b = B->b[us][piece] & piece_attack(piece, m.tsq, B->st->occ) & ~B->st->pinned;
			if (several_bits(b)) {
				clear_bit(&b, m.fsq);
				const unsigned sq = first_bit(b);
				if (file(m.fsq) == file(sq))
					*s++ = rank(m.fsq) + '1';
				else
					*s++ = file(m.fsq) + 'a';
			}
		}
	} else if (capture)
		*s++ = file(m.fsq) + 'a';
	
	if (capture) *s++ = 'x';
		
	if (!castling) {
		*s++ = file(m.tsq) + 'a';
		*s++ = rank(m.tsq) + '1';
	}
	
	if (m.promotion != NoPiece)
		*s++ = PieceLabel[White][m.promotion];		
	
	if (move_is_check(B, m)) *s++ = '+';
	*s++ = '\0';
}

unsigned move_is_check(const Board *B, move_t m)
/* Tests if a move is checking the enemy king. General case: direct checks, revealed checks (when
 * piece is a dchecker moving out of ray). Special cases: castling (check by the rook), en passant
 * (check through a newly revealed sliding attacker, once the ep capture square has been vacated)
 * returns 2 for a discovered check, 1 for any other check, 0 otherwise */
{
	assert(B->initialized);
	const unsigned us = B->turn, them = opp_color(us), kpos = B->king_pos[them];

	// test discovered check
	if ((test_bit(B->st->dcheckers, m.fsq))		// discovery checker
		&& (!test_bit(Direction[kpos][m.fsq], m.tsq)))	// move out of its dc-ray
		return 2;
	// test direct check
	else if (m.promotion == NoPiece) {
		const unsigned piece = B->piece_on[m.fsq];
		uint64_t tss = piece == Pawn ? PAttacks[us][m.tsq]
			: piece_attack(piece, m.tsq, B->st->occ);
		if (test_bit(tss, kpos))
			return 1;
	}

	if (m.ep) {
		uint64_t occ = B->st->occ;
		// play the ep capture on occ
		clear_bit(&occ, m.fsq);
		clear_bit(&occ, m.tsq + (us ? 8 : -8));
		set_bit(&occ, m.tsq);
		// test for new sliding attackers to the enemy king
		if ((get_RQ(B, us) & RPseudoAttacks[kpos] & rook_attack(kpos, occ))
		|| (get_BQ(B, us) & BPseudoAttacks[kpos] & bishop_attack(kpos, occ)))
			return 2;	// discovered check through the fsq or the ep captured square
	}
	else if (move_is_castling(B, m)) {
		// position of the Rook after playing the castling move
		unsigned rook_sq = m.tsq == (m.fsq + m.tsq) / 2;
		uint64_t occ = B->st->occ;
		clear_bit(&occ, m.fsq);
		uint64_t RQ = get_RQ(B, us);
		set_bit(&RQ, rook_sq);
		if (RQ & RPseudoAttacks[kpos] & rook_attack(kpos, occ))
			return 1;	// direct check by the castled rook
	}
	else if (m.promotion < NoPiece) {
		// test check by the promoted piece
		uint64_t occ = B->st->occ;
		clear_bit(&occ, m.fsq);
		if (test_bit(piece_attack(m.promotion, m.tsq, occ), kpos))
			return 1;	// direct check by the promoted piece
	}

	return 0;
}