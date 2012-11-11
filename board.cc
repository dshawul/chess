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
#include <cstring>
#include "board.h"

const std::string PieceLabel[NB_COLOR] = { "PNBRQK", "pnbrqk" };

void Board::clear()
{
	assert(BitboardInitialized);

	turn = WHITE;
	all[WHITE] = all[BLACK] = 0;
	king_pos[WHITE] = king_pos[BLACK] = 0;

	for (int sq = A1; sq <= H8; piece_on[sq++] = NO_PIECE);
	memset(b, 0, sizeof(b));

	_st = game_stack;
	memset(_st, 0, sizeof(game_info));
	_st->epsq = NO_SQUARE;
	_st->last_move = {0, 0, 0, NORMAL};
	move_count = 1;

	initialized = true;
}

void Board::set_fen(const std::string& _fen)
{
	clear();

	std::istringstream fen(_fen);
	fen >> std::noskipws;
	int sq = A8;
	char c, r, f;

	// piece placement
	while ((fen >> c) && !isspace(c)) {
		if ('1' <= c && c <= '8')
			sq += c - '0';
		else if (c == '/')
			sq -= 16;
		else {
			int color = isupper(c) ? WHITE : BLACK;
			int piece = PieceLabel[color].find(c);
			if (piece_ok(piece)) {
				set_square(color, piece, sq);
				if (piece == KING)
					king_pos[color] = sq;
			}
			sq++;
		}
	}

	// turn of play
	fen >> c;
	turn = c == 'w' ? WHITE : BLACK;
	_st->key ^= turn ? zob_turn : 0ULL;
	fen >> c;

	// castling rights
	while ((fen >> c) && !isspace(c)) {
		int color = isupper(c) ? WHITE : BLACK;
		c = toupper(c);
		if (c == 'K')
			_st->crights |= OO << (2 * color);
		else if (c == 'Q')
			_st->crights |= OOO << (2 * color);
	}

	if ( (fen >> f) && ('a' <= f && f <= 'h')
	     && (fen >> r) && ('1' <= r && r <= '8') )
		_st->epsq = square(r - '1', f - 'a');

	fen >> std::skipws >> _st->rule50 >> move_count;

	const int us = turn, them = opp_color(us);
	_st->pinned = hidden_checkers(1, us);
	_st->dcheckers = hidden_checkers(0, us);
	_st->attacked = calc_attacks(them);
	_st->checkers = test_bit(st().attacked, king_pos[us]) ? calc_checkers(us) : 0ULL;

	assert(calc_key() == st().key);
}

std::string Board::get_fen() const
{
	assert(initialized);
	std::ostringstream fen;

	// write the board
	for (int r = RANK_8; r >= RANK_1; --r) {
		int empty_cnt = 0;
		for (int f = FILE_A; f <= FILE_H; ++f) {
			int sq = square(r, f);
			if (piece_on[sq] == NO_PIECE)
				empty_cnt++;
			else {
				if (empty_cnt) {
					fen << empty_cnt;
					empty_cnt = 0;
				}
				fen << PieceLabel[get_color_on(sq)][piece_on[sq]];
			}
		}
		if (empty_cnt)
			fen << empty_cnt;
		if (r > RANK_1)
			fen << '/';
	}

	// turn of play
	fen << (turn ? " b " : " w ");

	// castling rights
	int crights = st().crights;
	if (crights) {
		if (crights & OO)
			fen << 'K';
		if (crights & OOO)
			fen << 'Q';
		if (crights & (OO << 2))
			fen << 'k';
		if (crights & (OOO << 2))
			fen << 'q';
	} else
		fen << '-';
	fen << ' ';

	// en passant square
	int epsq = st().epsq;
	if (square_ok(epsq)) {
		fen << char(file(epsq) + 'a');
		fen << char(rank(epsq) + '1');
	} else
		fen << '-';

	fen << ' ' << st().rule50 << ' ' << move_count;

	return fen.str();
}

std::ostream& operator<< (std::ostream& ostrm, const Board& B)
{
	for (int r = RANK_8; r >= RANK_1; --r) {
		for (int f = FILE_A; f <= FILE_H; ++f) {
			int sq = square(r, f);
			int color = B.get_color_on(sq);
			char c = color != NO_COLOR
			         ? PieceLabel[color][B.get_piece_on(sq)]
			         : (sq == B.st().epsq ? '*' : '.');
			ostrm << ' ' << c;
		}
		ostrm << std::endl;
	}

	return ostrm << B.get_fen() << std::endl;
}

void Board::play(move_t m)
{
	assert(initialized);

	++_st;
	memcpy(_st, _st-1, sizeof(game_info));
	_st->last_move = m;
	_st->rule50++;

	const int us = turn, them = opp_color(us);
	const int fsq = m.fsq, tsq = m.tsq;
	const int piece = piece_on[fsq], capture = piece_on[tsq];

	// normal capture: remove captured piece
	if (piece_ok(capture)) {
		_st->rule50 = 0;
		clear_square(them, capture, tsq);
	}

	// move our piece
	clear_square(us, piece, fsq);
	set_square(us, m.flag == PROMOTION ? m.get_prom() : piece, tsq);

	if (piece == PAWN) {
		_st->rule50 = 0;
		int inc_pp = us ? -8 : 8;
		// set the epsq if double push
		_st->epsq = (tsq == fsq + 2 * inc_pp) ? fsq + inc_pp : NO_SQUARE;
		// capture en passant
		if (m.flag == EN_PASSANT)
			clear_square(them, PAWN, tsq - inc_pp);
	} else {
		_st->epsq = NO_SQUARE;

		if (piece == ROOK) {
			// a rook move can alter castling rights
			if (fsq == (us ? H8 : H1))
				_st->crights &= ~(OO << (2 * us));
			else if (fsq == (us ? A8 : A1))
				_st->crights &= ~(OOO << (2 * us));
		} else if (piece == KING) {
			// update king_pos and clear crights
			king_pos[us] = tsq;
			_st->crights &= ~((OO | OOO) << (2 * us));
			
			if (m.flag == CASTLING) {
				// rook jump
				if (tsq == fsq+2) {			// OO
					clear_square(us, ROOK, us ? H8 : H1);
					set_square(us, ROOK, us ? F8 : F1);
				} else if (tsq == fsq-2) {	// OOO
					clear_square(us, ROOK, us ? A8 : A1);
					set_square(us, ROOK, us ? D8 : D1);
				}
			}
		}
	}

	if (capture == ROOK) {
		// Rook captures can alter opponent's castling rights
		if (tsq == (us ? H1 : H8))
			_st->crights &= ~(OO << (2 * them));
		else if (tsq == (us ? A1 : A8))
			_st->crights &= ~(OOO << (2 * them));
	}

	turn = them;
	if (turn == WHITE)
		++move_count;

	_st->key ^= zob_turn;
	_st->capture = capture;
	_st->pinned = hidden_checkers(1, them);
	_st->dcheckers = hidden_checkers(0, them);
	_st->attacked = calc_attacks(us);
	_st->checkers = test_bit(st().attacked, king_pos[them]) ? calc_checkers(them) : 0ULL;

	assert(calc_key() == st().key);
}

void Board::undo()
{
	assert(initialized);
	const move_t m = st().last_move;
	const int us = opp_color(turn), them = turn;
	const int fsq = m.fsq, tsq = m.tsq;
	const int piece = m.flag == PROMOTION ? PAWN : piece_on[tsq];
	const int capture = st().capture;

	// move our piece back
	clear_square(us, get_piece_on(tsq), tsq, false);	// get_piece_on() is to handle a promotion
	set_square(us, piece, fsq, false);

	// restore the captured piece (if any)
	if (piece_ok(capture))
		set_square(them, capture, tsq, false);

	if (piece == KING) {
		// update king_pos
		king_pos[us] = fsq;
		
		if (m.flag == CASTLING) {
			// undo rook jump
			if (tsq == fsq+2) {			// OO
				clear_square(us, ROOK, us ? F8 : F1, false);
				set_square(us, ROOK, us ? H8 : H1, false);
			} else if (tsq == fsq-2) {	// OOO
				clear_square(us, ROOK, us ? D8 : D1, false);
				set_square(us, ROOK, us ? A8 : A1, false);
			}
		}
	} else if (m.flag == EN_PASSANT)	// restore the en passant captured pawn
		set_square(them, PAWN, tsq + (us ? 8 : -8), false);

	turn = us;
	if (turn == BLACK)
		--move_count;

	--_st;
}

Bitboard Board::calc_attacks(int color) const
{
	assert(initialized);

	// King, Knights
	Bitboard r = KAttacks[king_pos[color]];
	Bitboard fss = b[color][KNIGHT];
	while (fss)
		r |= NAttacks[pop_lsb(&fss)];

	// Lateral
	fss = get_RQ(color);
	while (fss)
		r |= rook_attack(pop_lsb(&fss), st().occ);

	// Diagonal
	fss = get_BQ(color);
	while (fss)
		r |= bishop_attack(pop_lsb(&fss), st().occ);

	// Pawns
	r |= shift_bit((b[color][PAWN] & ~FileA_bb), color ? -9 : 7);
	r |= shift_bit((b[color][PAWN] & ~FileH_bb), color ? -7 : 9);

	return r;
}

void Board::set_square(int color, int piece, int sq, bool play)
{
	assert(initialized);
	assert(square_ok(sq) && color_ok(color) && piece_ok(piece));
	assert(piece_on[sq] == NO_PIECE);

	set_bit(&b[color][piece], sq);
	set_bit(&all[color], sq);
	piece_on[sq] = piece;

	if (play) {
		set_bit(&_st->occ, sq);
		_st->key ^= zob[color][piece][sq];
	}
}

void Board::clear_square(int color, int piece, int sq, bool play)
{
	assert(initialized);
	assert(square_ok(sq) && color_ok(color) && piece_ok(piece));
	assert(piece_on[sq] == piece);

	clear_bit(&b[color][piece], sq);
	clear_bit(&all[color], sq);
	piece_on[sq] = NO_PIECE;

	if (play) {
		clear_bit(&_st->occ, sq);
		_st->key ^= zob[color][piece][sq];
	}
}

Bitboard Board::hidden_checkers(bool find_pins, int color) const
{
	assert(initialized && color_ok(color) && (find_pins == 0 || find_pins == 1));
	const int aside = color ^ find_pins, kside = opp_color(aside);
	Bitboard result = 0ULL, pinners;

	// Pinned pieces protect our king, dicovery checks attack the enemy king.
	const int ksq = king_pos[kside];

	// Pinners are only sliders with X-ray attacks to ksq
	pinners = (get_RQ(aside) & RPseudoAttacks[ksq]) | (get_BQ(aside) & BPseudoAttacks[ksq]);

	while (pinners) {
		int sq = pop_lsb(&pinners);
		Bitboard b = Between[ksq][sq] & ~(1ULL << sq) & st().occ;
		// NB: if b == 0 then we're in check

		if (!several_bits(b) && (b & all[color]))
			result |= b;
	}
	return result;
}

Bitboard Board::calc_checkers(int kcolor) const
{
	assert(initialized && color_ok(kcolor));
	const int kpos = king_pos[kcolor];
	const int them = opp_color(kcolor);

	const Bitboard RQ = get_RQ(them) & RPseudoAttacks[kpos];
	const Bitboard BQ = get_BQ(them) & BPseudoAttacks[kpos];

	return (RQ & rook_attack(kpos, st().occ))
	       | (BQ & bishop_attack(kpos, st().occ))
	       | (b[them][KNIGHT] & NAttacks[kpos])
	       | (b[them][PAWN] & PAttacks[kcolor][kpos]);
}

Key Board::calc_key() const
/* Calculates the zobrist key from scratch. Used to assert() the dynamically computed st->key */
{
	Key key = 0ULL;

	for (int color = WHITE; color <= BLACK; ++color)
		for (int piece = PAWN; piece <= KING; ++piece) {
			Bitboard sqs = b[color][piece];
			while (sqs) {
				const int sq = pop_lsb(&sqs);
				key ^= zob[color][piece][sq];
			}
		}

	return turn ? key ^ zob_turn : key;
}
