/*
 * Zinc, an UCI chess interface. Copyright (C) 2012 Lucas Braesch.
 *
 * Zinc is free software: you can redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zinc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not,
 * see <http://www.gnu.org/licenses/>.
*/
#include "board.h"

bool move_t::operator== (move_t m) const
{
	return fsq == m.fsq && tsq == m.tsq && promotion == m.promotion && ep == m.ep;
}

bool Board::is_castling(move_t m) const
{
	return piece_on[m.fsq] == King
	       && (m.tsq == m.fsq - 2 || m.tsq == m.fsq + 2);
}

bool Board::is_legal(move_t m) const
/* Determines whether a move is legal in the current board position. This function makes no
 * assumption or shortcut (like assuming the move is at least pseudo-legal for example), and tests
 * every single component of the move_t structure. So it is quite long and complex, but its execution
 * time should be rather fast (with the notable exception of check evasions)
 * */
{
	const Color us = turn, them = opp_color(us);
	const Square kpos = king_pos[us];
	const Piece piece = piece_on[m.fsq], capture = piece_on[m.tsq];
	const Square fsq = m.fsq, tsq = m.tsq;

	// verify fsq and piece
	if (get_color_on(fsq) != us)
		return false;

	// verify tsq and capture
	if (get_color_on(tsq) != (capture == NoPiece ? NoColor : them))
		return false;

	bool pinned = test_bit(st->pinned, fsq);
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
			if (st->epsq != tsq || !is_capture)
				return false;

			// self check through en passant captured pawn
			const Square ep_cap = pawn_push(them, tsq);
			const uint64_t ray = Direction[kpos][ep_cap];
			if (ray) {
				uint64_t occ = st->occ;
				clear_bit(&occ, ep_cap);
				set_bit(&occ, tsq);

				const uint64_t RQ = get_RQ(them);
				if (ray & RQ & RPseudoAttacks[kpos] & rook_attack(kpos, occ))
					return false;

				const uint64_t BQ = get_BQ(them);
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
			       && piece_on[pawn_push(us, fsq)] == NoPiece
			       && test_bit(PInitialRank[us], fsq);
		else
			return false;
	}

	if (m.ep || m.promotion != NoPiece)
		return false;

	uint64_t tss = piece_attack(piece, fsq, st->occ);

	if (piece == King) {
		// castling moves
		if (is_castling(m)) {
			uint64_t safe /* must not be attacked */, empty /* must be empty */;

			if ((fsq+2 == tsq) && (st->crights & (OO << (2 * us)))) {
				empty = safe = 3ULL << (m.fsq + 1);
				return !(st->attacked & safe) && !(st->occ & empty);
			} else if ((fsq-2 == tsq) && (st->crights & (OOO << (2 * us)))) {
				safe = 3ULL << (m.fsq - 2);
				empty = safe | (1ULL << (m.fsq - 3));
				return !(st->attacked & safe) && !(st->occ & empty);
			} else
				return false;
		}

		// normal king moves
		return test_bit(tss & ~st->attacked, tsq);
	}

	// piece moves
	if (pinned) tss &= pin_ray;
	if (!test_bit(tss, tsq)) return false;

	// check evasion: use the slow method (optimize later if needs be)
	if (st->checkers) {
		const_cast<Board*>(this)->play(m);
		bool check = calc_checkers(us);
		const_cast<Board*>(this)->undo();
		return !check;
	}

	return true;
}

move_t string_to_move(const Board& B, const std::string& s)
{
	move_t m;

	m.fsq = square(Rank(s[1]-'1'), File(s[0]-'a'));
	m.tsq = square(Rank(s[3]-'1'), File(s[2]-'a'));
	m.ep = B.get_piece_on(m.fsq) == Pawn && m.tsq == B.get_st().epsq;

	if (s[4]) {
		Piece piece;
		for (piece = Knight; piece <= Queen; ++piece)
			if (PieceLabel[Black][piece] == s[4])
				break;
		assert(piece < King);
		m.promotion = piece;
	} else
		m.promotion = NoPiece;

	return m;
}

std::string move_to_string(const Board& B, move_t m)
{
	std::string s;

	s.push_back(file(m.fsq) + 'a');
	s.push_back(rank(m.fsq) + '1');
	assert(square_ok(m.fsq));

	s.push_back(file(m.tsq) + 'a');
	s.push_back(rank(m.tsq) + '1');
	assert(square_ok(m.tsq));

	if (piece_ok(m.promotion))
		s.push_back(PieceLabel[Black][m.promotion]);

	return s;
}

std::string move_to_san(const Board& B, move_t m)
{
	std::string s;
	const Color us = B.get_turn();
	const Piece piece = B.get_piece_on(m.fsq);
	const bool capture = m.ep || B.get_piece_on(m.tsq) != NoPiece, castling = B.is_castling(m);

	if (piece != Pawn) {
		if (castling) {
			if (file(m.tsq) == FileC)
				s += "OOO";
			else
				s += "OO";
		} else {
			s.push_back(PieceLabel[White][piece]);
			uint64_t b = B.get_pieces(us, piece)
			             & piece_attack(piece, m.tsq, B.get_st().occ)
			             & ~B.get_st().pinned;
			if (several_bits(b)) {
				clear_bit(&b, m.fsq);
				const Square sq = first_bit(b);
				if (file(m.fsq) == file(sq))
					s.push_back(rank(m.fsq) + '1');
				else
					s.push_back(file(m.fsq) + 'a');
			}
		}
	} else if (capture)
		s.push_back(file(m.fsq) + 'a');

	if (capture)
		s.push_back('x');

	if (!castling) {
		s.push_back(file(m.tsq) + 'a');
		s.push_back(rank(m.tsq) + '1');
	}

	if (m.promotion != NoPiece)
		s.push_back(PieceLabel[White][m.promotion]);

	if (B.is_check(m))
		s.push_back('+');

	return s;
}

unsigned Board::is_check(move_t m) const
/* Tests if a move is checking the enemy king. General case: direct checks, revealed checks (when
 * piece is a dchecker moving out of ray). Special cases: castling (check by the rook), en passant
 * (check through a newly revealed sliding attacker, once the ep capture square has been vacated)
 * returns 2 for a discovered check, 1 for any other check, 0 otherwise */
{
	const Color us = turn, them = opp_color(us);
	const Square fsq = m.fsq, tsq = m.tsq;
	Square kpos = king_pos[them];

	// test discovered check
	if ( (test_bit(st->dcheckers, fsq))		// discovery checker
	        && (!test_bit(Direction[kpos][fsq], tsq)))	// move out of its dc-ray
		return 2;
	// test direct check
	else if (m.promotion == NoPiece) {
		const Piece piece = piece_on[fsq];
		uint64_t tss = piece == Pawn ? PAttacks[us][tsq]
		               : piece_attack(piece, tsq, st->occ);
		if (test_bit(tss, kpos))
			return 1;
	}

	if (m.ep) {
		uint64_t occ = st->occ;
		// play the ep capture on occ
		clear_bit(&occ, fsq);
		clear_bit(&occ, tsq + (us ? 8 : -8));
		set_bit(&occ, tsq);
		// test for new sliding attackers to the enemy king
		if ((get_RQ(us) & RPseudoAttacks[kpos] & rook_attack(kpos, occ))
		        || (get_BQ(us) & BPseudoAttacks[kpos] & bishop_attack(kpos, occ)))
			return 2;	// discovered check through the fsq or the ep captured square
	} else if (is_castling(m)) {
		// position of the Rook after playing the castling move
		Square rook_sq = Square(int(fsq + tsq) / 2);
		uint64_t occ = st->occ;
		clear_bit(&occ, fsq);
		uint64_t RQ = get_RQ(us);
		set_bit(&RQ, rook_sq);
		if (RQ & RPseudoAttacks[kpos] & rook_attack(kpos, occ))
			return 1;	// direct check by the castled rook
	} else if (m.promotion < NoPiece) {
		// test check by the promoted piece
		uint64_t occ = st->occ;
		clear_bit(&occ, fsq);
		if (test_bit(piece_attack(m.promotion, tsq, occ), kpos))
			return 1;	// direct check by the promoted piece
	}

	return 0;
}
