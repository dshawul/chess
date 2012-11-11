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
#include "board.h"

namespace
{
	std::string square_to_string(int sq)
	{
		std::ostringstream s;
		s << char(file(sq) + 'a') << char(rank(sq) + '1');
		return s.str();
	}
}

bool move_is_castling(const Board& B, move_t m)
{
	return B.get_piece_on(m.fsq) == KING && (m.tsq == m.fsq - 2 || m.tsq == m.fsq + 2);
}

int move_is_check(const Board& B, move_t m)
/* Tests if a move is checking the enemy king. General case: direct checks, revealed checks (when
 * piece is a dchecker moving out of ray). Special cases: castling (check by the rook), en passant
 * (check through a newly revealed sliding attacker, once the ep capture square has been vacated)
 * returns 2 for a discovered check, 1 for any other check, 0 otherwise */
{
	const int us = B.get_turn(), them = opp_color(us);
	const int fsq = m.fsq, tsq = m.tsq;
	int kpos = B.get_king_pos(them);

	// test discovered check
	if ( (test_bit(B.st().dcheckers, fsq))		// discovery checker
	     && (!test_bit(Direction[kpos][fsq], tsq)))	// move out of its dc-ray
		return 2;
	// test direct check
	else if (m.prom == NO_PIECE) {
		const int piece = B.get_piece_on(fsq);
		Bitboard tss = piece == PAWN ? PAttacks[us][tsq]
		               : piece_attack(piece, tsq, B.st().occ);
		if (test_bit(tss, kpos))
			return 1;
	}

	if (m.ep) {
		Bitboard occ = B.st().occ;
		// play the ep capture on occ
		clear_bit(&occ, fsq);
		clear_bit(&occ, tsq + (us ? 8 : -8));
		set_bit(&occ, tsq);
		// test for new sliding attackers to the enemy king
		if ((B.get_RQ(us) & RPseudoAttacks[kpos] & rook_attack(kpos, occ))
		    || (B.get_BQ(us) & BPseudoAttacks[kpos] & bishop_attack(kpos, occ)))
			return 2;	// discovered check through the fsq or the ep captured square
	} else if (move_is_castling(B, m)) {
		// position of the Rook after playing the castling move
		int rook_sq = (fsq + tsq) / 2;
		Bitboard occ = B.st().occ;
		clear_bit(&occ, fsq);
		Bitboard RQ = B.get_RQ(us);
		set_bit(&RQ, rook_sq);
		if (RQ & RPseudoAttacks[kpos] & rook_attack(kpos, occ))
			return 1;	// direct check by the castled rook
	} else if (m.prom < NO_PIECE) {
		// test check by the promoted piece
		Bitboard occ = B.st().occ;
		clear_bit(&occ, fsq);
		if (test_bit(piece_attack(m.prom, tsq, occ), kpos))
			return 1;	// direct check by the promoted piece
	}

	return 0;
}

move_t string_to_move(const Board& B, const std::string& s)
{
	move_t m;

	m.fsq = square(s[1]-'1', s[0]-'a');
	m.tsq = square(s[3]-'1', s[2]-'a');
	m.ep = B.get_piece_on(m.fsq) == PAWN && m.tsq == B.st().epsq;

	if (s[4]) {
		int piece;
		for (piece = KNIGHT; piece <= QUEEN; ++piece)
			if (PieceLabel[BLACK][piece] == s[4])
				break;
		assert(piece < KING);
		m.prom = piece;
	} else
		m.prom = NO_PIECE;

	return m;
}

std::string move_to_string(move_t m)
{
	std::ostringstream s;

	s << square_to_string(m.fsq);
	s << square_to_string(m.tsq);

	if (piece_ok(m.prom))
		s << PieceLabel[BLACK][m.prom];

	return s.str();
}

std::string move_to_san(const Board& B, move_t m)
{
	std::ostringstream s;
	const int us = B.get_turn();
	const int piece = B.get_piece_on(m.fsq);
	const bool capture = m.ep || B.get_piece_on(m.tsq) != NO_PIECE, castling = move_is_castling(B, m);

	if (piece != PAWN) {
		if (castling) {
			if (file(m.tsq) == FILE_C)
				s << "OOO";
			else
				s << "OO";
		} else {
			s << PieceLabel[WHITE][piece];
			Bitboard b = B.get_pieces(us, piece)
			             & piece_attack(piece, m.tsq, B.st().occ)
			             & ~B.st().pinned;
			if (several_bits(b)) {
				clear_bit(&b, m.fsq);
				const int sq = lsb(b);
				if (file(m.fsq) == file(sq))
					s << char(rank(m.fsq) + '1');
				else
					s << char(file(m.fsq) + 'a');
			}
		}
	} else if (capture)
		s << char(file(m.fsq) + 'a');

	if (capture)
		s << 'x';

	if (!castling)
		s << square_to_string(m.tsq);

	if (m.prom != NO_PIECE)
		s << PieceLabel[WHITE][m.prom];

	if (move_is_check(B, m))
		s << '+';

	return s.str();
}
