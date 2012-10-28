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
#include <sstream>
#include <cstring>
#include "board.h"

const std::string PieceLabel[NB_COLOR] = { "PNBRQK", "pnbrqk" };

void Board::clear()
{
	assert(BitboardInitialized);

	turn = White;
	all[White] = all[Black] = 0;
	king_pos[White] = king_pos[Black] = NoSquare;

	for (Square sq = A1; sq <= H8; piece_on[sq++] = NoPiece);
	memset(b, 0, sizeof(b));

	_st = game_stack;
	memset(_st, 0, sizeof(game_info));
	_st->epsq = NoSquare;
	_st->last_move = {NoSquare, NoSquare, NoPiece, false};
	move_count = 1;

	initialized = true;
}

void Board::set_fen(const std::string& _fen)
{
	clear();

	std::istringstream fen(_fen);
	fen >> std::noskipws;
	Square sq = A8;
	char c, r, f;

	// piece placement
	while ((fen >> c) && !isspace(c)) {
		if ('1' <= c && c <= '8')
			sq += c - '0';
		else if (c == '/')
			sq -= 16;
		else {
			Color color = isupper(c) ? White : Black;
			Piece piece = Piece(PieceLabel[color].find(c));
			if (piece_ok(piece)) {
				set_square(color, piece, sq);
				if (piece == King)
					king_pos[color] = sq;
			}
			sq++;
		}
	}

	// turn of play
	fen >> c;
	turn = c == 'w' ? White : Black;
	_st->key ^= turn ? zob_turn : 0ULL;
	fen >> c;

	// castling rights
	while ((fen >> c) && !isspace(c)) {
		Color color = isupper(c) ? White : Black;
		c = toupper(c);
		if (c == 'K')
			_st->crights |= OO << (2 * color);
		else if (c == 'Q')
			_st->crights |= OOO << (2 * color);
	}

	if ( (fen >> f) && ('a' <= f && f <= 'h')
	        && (fen >> r) && ('1' <= r && r <= '8') )
		_st->epsq = square(Rank(r - '1'), File(f - 'a'));

	fen >> std::skipws >> _st->rule50 >> move_count;

	const Color us = turn, them = opp_color(us);
	_st->pinned = hidden_checkers(1, us);
	_st->dcheckers = hidden_checkers(0, us);
	_st->attacked = calc_attacks(them);
	_st->checkers = test_bit(get_st().attacked, king_pos[us]) ? calc_checkers(us) : 0ULL;

	assert(calc_key() == get_st().key);
}

std::string Board::get_fen() const
{
	assert(initialized);
	std::ostringstream fen;

	// write the board
	for (Rank r = Rank8; r >= Rank1; --r) {
		unsigned empty_cnt = 0;
		for (File f = FileA; f <= FileH; ++f) {
			Square sq = square(r, f);
			if (piece_on[sq] == NoPiece)
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
		if (r > Rank1)
			fen << '/';
	}

	// turn of play
	fen << (turn ? " b " : " w ");

	// castling rights
	unsigned crights = get_st().crights;
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
	Square epsq = get_st().epsq;
	if (epsq != NoSquare) {
		fen << char(file(epsq) + 'a');
		fen << char(rank(epsq) + '1');
	} else
		fen << '-';

	fen << ' ' << get_st().rule50 << ' ' << move_count;

	return fen.str();
}

std::ostream& operator<< (std::ostream& ostrm, const Board& B)
{
	for (Rank r = Rank8; r >= Rank1; --r) {
		for (File f = FileA; f <= FileH; ++f) {
			Square sq = square(r, f);
			Color color = B.get_color_on(sq);
			char c = color != NoColor
			         ? PieceLabel[color][B.get_piece_on(sq)]
			         : (sq == B.get_st().epsq ? '*' : '.');
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

	const Color us = turn, them = opp_color(us);
	const Square fsq = m.fsq, tsq = m.tsq;
	const Piece piece = piece_on[fsq], capture = piece_on[tsq];

	// normal capture: remove captured piece
	if (piece_ok(capture)) {
		_st->rule50 = 0;
		clear_square(them, capture, tsq);
	}

	// move our piece
	clear_square(us, piece, fsq);
	set_square(us, m.promotion == NoPiece ? piece : m.promotion, tsq);

	if (piece == Pawn) {
		_st->rule50 = 0;
		int inc_pp = us ? -8 : 8;
		// set the epsq if double push
		_st->epsq = (tsq - fsq == 2 * inc_pp)
		           ? fsq + inc_pp
		           : NoSquare;
		// capture en passant
		if (m.ep)
			clear_square(them, Pawn, tsq - inc_pp);
	} else {
		_st->epsq = NoSquare;

		if (piece == Rook) {
			// a rook move can alter castling rights
			if (fsq == (us ? H8 : H1))
				_st->crights &= ~(OO << (2 * us));
			else if (fsq == (us ? A8 : A1))
				_st->crights &= ~(OOO << (2 * us));
		} else if (piece == King) {
			// update king_pos and clear crights
			king_pos[us] = tsq;
			_st->crights &= ~((OO | OOO) << (2 * us));
			// move the rook (jump over the king)
			if (tsq == fsq+2) {			// OO
				clear_square(us, Rook, us ? H8 : H1);
				set_square(us, Rook, us ? F8 : F1);
			} else if (tsq == fsq-2) {	// OOO
				clear_square(us, Rook, us ? A8 : A1);
				set_square(us, Rook, us ? D8 : D1);
			}
		}
	}

	if (capture == Rook) {
		// Rook captures can alter opponent's castling rights
		if (tsq == (us ? H1 : H8))
			_st->crights &= ~(OO << (2 * them));
		else if (tsq == (us ? A1 : A8))
			_st->crights &= ~(OOO << (2 * them));
	}

	turn = them;
	if (turn == White)
		++move_count;

	_st->key ^= zob_turn;
	_st->capture = capture;
	_st->pinned = hidden_checkers(1, them);
	_st->dcheckers = hidden_checkers(0, them);
	_st->attacked = calc_attacks(us);
	_st->checkers = test_bit(get_st().attacked, king_pos[them]) ? calc_checkers(them) : 0ULL;

	assert(calc_key() == get_st().key);
}

void Board::undo()
{
	assert(initialized);
	const move_t m = get_st().last_move;
	const Color us = opp_color(turn), them = turn;
	const Square fsq = m.fsq, tsq = m.tsq;
	const Piece piece = piece_ok(m.promotion) ? Pawn : piece_on[tsq];
	const Piece capture = get_st().capture;

	// move our piece back
	clear_square(us, m.promotion == NoPiece ? piece : m.promotion, tsq, false);
	set_square(us, piece, fsq, false);

	// restore the captured piece (if any)
	if (piece_ok(capture))
		set_square(them, capture, tsq, false);

	if (piece == King) {
		// update king_pos
		king_pos[us] = fsq;
		// undo rook jump (castling)
		if (tsq == fsq+2) {			// OO
			clear_square(us, Rook, us ? F8 : F1, false);
			set_square(us, Rook, us ? H8 : H1, false);
		} else if (tsq == fsq-2) {	// OOO
			clear_square(us, Rook, us ? D8 : D1, false);
			set_square(us, Rook, us ? A8 : A1, false);
		}
	} else if (m.ep)	// restore the en passant captured pawn
		set_square(them, Pawn, tsq + (us ? 8 : -8), false);

	turn = us;
	if (turn == Black)
		--move_count;

	--_st;
}

uint64_t Board::calc_attacks(Color color) const
{
	assert(initialized);

	// King, Knights
	uint64_t r = KAttacks[king_pos[color]];
	uint64_t fss = b[color][Knight];
	while (fss)
		r |= NAttacks[next_bit(&fss)];

	// Lateral
	fss = get_RQ(color);
	while (fss)
		r |= rook_attack(next_bit(&fss), get_st().occ);

	// Diagonal
	fss = get_BQ(color);
	while (fss)
		r |= bishop_attack(next_bit(&fss), get_st().occ);

	// Pawns
	r |= shift_bit((b[color][Pawn] & ~FileA_bb), color ? -9 : 7);
	r |= shift_bit((b[color][Pawn] & ~FileH_bb), color ? -7 : 9);

	return r;
}

Result Board::game_over() const
{
	// insufficient material
	if (!b[White][Pawn] && !b[Black][Pawn]
	        && !get_RQ(White) && !get_RQ(Black)
	        && !several_bits(b[White][Knight] | b[White][Bishop])
	        && !several_bits(b[Black][Knight] | b[Black][Bishop]))
		return ResultMaterial;

	// 50 move rule
	if (get_st().rule50 >= 100)
		return Result50Move;

	// 3-fold repetition
	for (int i = 4; i <= get_st().rule50; i += 2)
		if (_st[-i].key == _st->key)
			return ResultThreefold;

	if (get_st().checkers)
		return is_mate() ? ResultMate : ResultNone;
	else
		return is_stalemate() ? ResultStalemate : ResultNone;
}

Color Board::get_color_on(Square sq) const
{
	assert(initialized && square_ok(sq));
	return test_bit(all[White], sq) ? White : (test_bit(all[Black], sq) ? Black : NoColor);
}

uint64_t Board::get_RQ(Color color) const
{
	assert(initialized && color_ok(color));
	return b[color][Rook] | b[color][Queen];
}

uint64_t Board::get_BQ(Color color) const
{
	assert(initialized && color_ok(color));
	return b[color][Bishop] | b[color][Queen];
}

void Board::set_square(Color color, Piece piece, Square sq, bool play)
{
	assert(initialized);
	assert(square_ok(sq) && color_ok(color) && piece_ok(piece));
	assert(piece_on[sq] == NoPiece);

	set_bit(&b[color][piece], sq);
	set_bit(&all[color], sq);
	piece_on[sq] = piece;

	if (play) {
		set_bit(&_st->occ, sq);
		_st->key ^= zob[color][piece][sq];
	}
}

void Board::clear_square(Color color, Piece piece, Square sq, bool play)
{
	assert(initialized);
	assert(square_ok(sq) && color_ok(color) && piece_ok(piece));
	assert(piece_on[sq] == piece);

	clear_bit(&b[color][piece], sq);
	clear_bit(&all[color], sq);
	piece_on[sq] = NoPiece;

	if (play) {
		clear_bit(&_st->occ, sq);
		_st->key ^= zob[color][piece][sq];
	}
}

uint64_t Board::hidden_checkers(bool find_pins, Color color) const
{
	assert(initialized && color_ok(color) && (find_pins == 0 || find_pins == 1));
	const Color aside = color ^ Color(find_pins), kside = opp_color(aside);
	uint64_t result = 0ULL, pinners;

	// Pinned pieces protect our king, dicovery checks attack the enemy king.
	const Square ksq = king_pos[kside];

	// Pinners are only sliders with X-ray attacks to ksq
	pinners = (get_RQ(aside) & RPseudoAttacks[ksq]) | (get_BQ(aside) & BPseudoAttacks[ksq]);

	while (pinners) {
		Square sq = next_bit(&pinners);
		uint64_t b = Between[ksq][sq] & ~(1ULL << sq) & get_st().occ;
		// NB: if b == 0 then we're in check

		if (!several_bits(b) && (b & all[color]))
			result |= b;
	}
	return result;
}

uint64_t Board::calc_checkers(Color kcolor) const
{
	assert(initialized && color_ok(kcolor));
	const Square kpos = king_pos[kcolor];
	const Color them = opp_color(kcolor);

	const uint64_t RQ = get_RQ(them) & RPseudoAttacks[kpos];
	const uint64_t BQ = get_BQ(them) & BPseudoAttacks[kpos];

	return (RQ & rook_attack(kpos, get_st().occ))
	       | (BQ & bishop_attack(kpos, get_st().occ))
	       | (b[them][Knight] & NAttacks[kpos])
	       | (b[them][Pawn] & PAttacks[kcolor][kpos]);
}

uint64_t Board::calc_key() const
/* Calculates the zobrist key from scratch. Used to assert() the dynamically computed st->key */
{
	uint64_t key = 0ULL;

	for (Color color = White; color <= Black; ++color)
		for (unsigned piece = Pawn; piece <= King; ++piece) {
			uint64_t sqs = b[color][piece];
			while (sqs) {
				const unsigned sq = next_bit(&sqs);
				key ^= zob[color][piece][sq];
			}
		}

	return turn ? key ^ zob_turn : key;
}

bool Board::is_stalemate() const
{
	assert(!get_st().checkers);
	move_t mlist[MAX_MOVES];
	const uint64_t targets = ~get_pieces(get_turn());

	return ( !has_piece_moves(*this, targets)
	         && gen_pawn_moves(*this, targets, mlist, true) == mlist);
}

bool Board::is_mate() const
{
	assert(get_st().checkers);
	move_t mlist[MAX_MOVES];
	return gen_evasion(*this, mlist) == mlist;
}

Piece Board::get_piece_on(Square sq) const
{
	assert(initialized && square_ok(sq));
	return piece_on[sq];
}

uint64_t Board::get_pieces(Color color) const
{
	assert(initialized && color_ok(color));
	return all[color];
}

uint64_t Board::get_pieces(Color color, Piece piece) const
{
	assert(initialized && color_ok(color) && piece_ok(piece));
	return b[color][piece];
}

Color Board::get_turn() const
{
	assert(initialized);
	return turn;
}

Square Board::get_king_pos(Color c) const
{
	assert(initialized);
	return king_pos[c];
}

const game_info& Board::get_st() const
{
	assert(initialized);
	return *_st;
}

int Board::get_move_count() const
{
	assert(initialized);
	return move_count;
}
