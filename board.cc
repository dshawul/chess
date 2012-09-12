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

const char *PieceLabel[NB_COLOR] = { "PNBRQK", "pnbrqk" };

void Board::clear()
{
	assert(BitboardInitialized);

	turn = White;
	all[White] = all[Black] = 0;
	king_pos[White] = king_pos[Black] = NoSquare;

	for (Square sq = A1; sq <= H8; piece_on[sq++] = NoPiece);
	memset(b, 0, sizeof(b));

	st = game_stack;
	memset(st, 0, sizeof(game_info));
	st->epsq = NoSquare;
	st->last_move = {NoSquare, NoSquare, NoPiece, false};

	initialized = true;
}

void Board::set_fen(const char *fen)
{
	assert(fen);
	Rank r;
	File f;
	char c = *fen++;

	// load the board
	clear();
	for (r = Rank8, f = FileA; c != ' ' && r >= Rank1; c = *fen++)
	{
		// empty squares
		if ('1' <= c && c <= '8')
		{
			f += c - '0';
			assert(f <= NB_RANK_FILE);
		}
		// EOL separator
		else if (c == '/')
		{
			assert(f == NB_RANK_FILE);
			--r;
			f = FileA;
		}
		// piece
		else
		{
			Square sq = square(r, f++);
			Piece piece;
			for (piece = Pawn; piece <= King; ++piece)
			{
				if (c == PieceLabel[isupper(c) ? White : Black][piece])
					break;
			}
			assert(piece_ok(piece));
			Color color = isupper(c) ? White : Black;
			set_square(color, piece, sq, true);
			if (piece == King)
				king_pos[color] = sq;
		}
	}

	// turn of play
	c = *fen++;
	assert(c == 'w' || c == 'b');
	turn = c == 'w' ? White : Black;
	st->key ^= turn ? zob_turn : 0ULL;
	const Color us = turn, them = opp_color(us);

	// Castling rights
	if (*fen++ != ' ') assert(0);
	c = *fen++;
	st->crights = 0;

	if (c == '-')	// no castling rights
		c = *fen++;
	else  			// set castling rights (in reading order KQkq)
	{
		if (c == 'K')
		{
			st->crights ^= OO;
			c = *fen++;
		}
		if (c == 'Q')
		{
			st->crights ^= OOO;
			c = *fen++;
		}
		if (c == 'k')
		{
			st->crights ^= OO << 2;
			c = *fen++;
		}
		if (c == 'q')
		{
			st->crights ^= OOO << 2;
			c = *fen++;
		}
	}

	// en passant square
	assert(c == ' ');
	c = *fen++;
	if (c == '-')
	{
		// no en passant square
		c = *fen++;
		st->epsq = NoSquare;
	}
	else
	{
		Rank r;
		File f;
		assert('a' <= c && c <= 'h');
		f = File(c - 'a');
		c = *fen++;
		assert(isdigit(c));
		r = Rank(c - '1');
		st->epsq = square(r, f);
	}

	st->pinned = hidden_checkers(1, us);
	st->dcheckers = hidden_checkers(0, us);
	
	st->attacked = calc_attacks(them);
	st->checkers = test_bit(st->attacked, king_pos[us])
	               ? calc_checkers(us) : 0ULL;

	assert(calc_key() == st->key);
}

void Board::get_fen(char *fen) const
{
	assert(initialized && fen);

	// write the board
	for (Rank r = Rank8; r >= Rank1; --r)
	{
		unsigned empty_cnt = 0;
		for (File f = FileA; f <= FileH; ++f)
		{
			Square sq = square(r, f);
			if (piece_on[sq] == NoPiece)
				empty_cnt++;
			else
			{
				if (empty_cnt)
				{
					*fen++ = empty_cnt + '0';
					empty_cnt = 0;
				}
				*fen++ = PieceLabel[color_on(sq)][piece_on[sq]];
			}
		}
		if (empty_cnt) *fen++ = empty_cnt + '0';
		*fen++ = r == Rank1 ? ' ' : '/';
	}

	// turn of play
	*fen++ = turn ? 'b' : 'w';
	*fen++ = ' ';

	// castling rights
	unsigned crights = st->crights;
	if (crights)
	{
		if (crights & OO) *fen++ = 'K';
		if (crights & OOO) *fen++ = 'Q';
		if (crights & (OO << 2)) *fen++ = 'k';
		if (crights & (OOO << 2)) *fen++ = 'q';
	}
	else
		*fen++ = '-';
	*fen++ = ' ';

	// en passant square
	Square epsq = st->epsq;
	if (epsq != NoSquare)
	{
		*fen++ = file(epsq) + 'a';
		*fen++ = rank(epsq) + '1';
	}
	else
		*fen++ = '-';

	*fen++ = '\0';
}

void print(const Board& B)
{
	for (Rank r = Rank8; r >= Rank1; --r)
	{
		for (File f = FileA; f <= FileH; ++f)
		{
			Square sq = square(r, f);
			Color color = B.color_on(sq);
			char c = color != NoColor
			         ? PieceLabel[color][B.get_piece_on(sq)]
			         : (sq == B.st->epsq ? '*' : '.');
			printf(" %c", c);
		}
		printf("\n");
	}

	char fen[MAX_FEN];
	B.get_fen(fen);
	printf("fen: %s\n", fen);
}

void Board::play(const move_t& m)
{
	assert(initialized);

	++st;
	memcpy(st, st-1, sizeof(game_info));
	st->last_move = m;
	st->rule50++;

	const Color us = turn, them = opp_color(us);
	const Piece piece = piece_on[m.fsq], capture = piece_on[m.tsq];

	// normal capture: remove captured piece
	if (piece_ok(capture))
	{
		st->rule50 = 0;
		clear_square(them, capture, m.tsq, true);
	}

	// move our piece
	clear_square(us, piece, m.fsq, true);
	set_square(us, m.promotion == NoPiece ? piece : m.promotion, m.tsq, true);

	if (piece == Pawn)
	{
		st->rule50 = 0;
		int inc_pp = us ? -8 : 8;
		// set the epsq if double push
		st->epsq = (m.tsq - m.fsq == 2 * inc_pp)
		           ? m.fsq + inc_pp
		           : NoSquare;
		// capture en passant
		if (m.ep)
			clear_square(them, Pawn, m.tsq - inc_pp, true);
	}
	else
	{
		st->epsq = NoSquare;

		if (piece == Rook)
		{
			// a rook move can alter castling rights
			if (m.fsq == (us ? H8 : H1))
				st->crights &= ~(OO << (2 * us));
			else if (m.fsq == (us ? A8 : A1))
				st->crights &= ~(OOO << (2 * us));
		}
		else if (piece == King)
		{
			// update king_pos and clear crights
			king_pos[us] = m.tsq;
			st->crights &= ~((OO | OOO) << (2 * us));
			// move the rook (jump over the king)
			if (m.tsq == m.fsq+2)  			// OO
			{
				clear_square(us, Rook, us ? H8 : H1, true);
				set_square(us, Rook, us ? F8 : F1, true);
			}
			else if (m.tsq == m.fsq-2)  	// OOO
			{
				clear_square(us, Rook, us ? A8 : A1, true);
				set_square(us, Rook, us ? D8 : D1, true);
			}
		}
	}

	if (capture == Rook)
	{
		// Rook captures can alter opponent's castling rights
		if (m.tsq == (us ? H1 : H8))
			st->crights &= ~(OO << (2 * them));
		else if (m.tsq == (us ? A1 : A8))
			st->crights &= ~(OOO << (2 * them));
	}

	turn = them;
	st->key ^= zob_turn;
	st->capture = capture;
	st->pinned = hidden_checkers(1, them);
	st->dcheckers = hidden_checkers(0, them);	
	st->attacked = calc_attacks(us);
	st->checkers = test_bit(st->attacked, king_pos[them]) ? calc_checkers(them) : 0ULL;

	assert(calc_key() == st->key);
}

void Board::undo()
{
	assert(initialized);
	const Color us = opp_color(turn), them = turn;
	const move_t m = st->last_move;
	const Piece piece = piece_ok(m.promotion) ? Pawn : piece_on[m.tsq];
	const Piece capture = st->capture;

	// move our piece back
	clear_square(us, m.promotion == NoPiece ? piece : m.promotion, m.tsq, false);
	set_square(us, piece, m.fsq, false);

	// restore the captured piece (if any)
	if (piece_ok(capture))
		set_square(them, capture, m.tsq, false);

	if (piece == King)
	{
		// update king_pos
		king_pos[us] = m.fsq;
		// undo rook jump (castling)
		if (m.tsq == m.fsq+2)  			// OO
		{
			clear_square(us, Rook, us ? F8 : F1, false);
			set_square(us, Rook, us ? H8 : H1, false);
		}
		else if (m.tsq == m.fsq-2)  	// OOO
		{
			clear_square(us, Rook, us ? D8 : D1, false);
			set_square(us, Rook, us ? A8 : A1, false);
		}
	}
	else if (m.ep)	// restore the en passant captured pawn
		set_square(them, Pawn, m.tsq + (us ? 8 : -8), false);

	turn = us;
	--st;
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
		r |= rook_attack(next_bit(&fss), st->occ);

	// Diagonal
	fss = get_BQ(color);
	while (fss)
		r |= bishop_attack(next_bit(&fss), st->occ);

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
	if (st->rule50 >= 100)
		return Result50Move;

	// 3-fold repetition
	for (int i = 4; i <= st->rule50; i += 2)
		if (st[-i].key == st->key)
			return ResultThreefold;

	if (get_checkers())
		return is_mate(*this) ? ResultMate : ResultNone;
	else
		return is_stalemate(*this) ? ResultStalemate : ResultNone;
}

Color Board::color_on(Square sq) const
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

uint64_t Board::get_epsq_bb() const
{
	assert(initialized);
	return st->epsq < NoSquare ? (1ULL << st->epsq) : 0ULL;
}

uint64_t Board::get_checkers() const
{
	assert(initialized);
	return st->checkers;
}

void Board::set_square(Color color, Piece piece, Square sq, bool play)
{
	assert(initialized);
	assert(square_ok(sq) && color_ok(color) && piece_ok(piece));
	assert(piece_on[sq] == NoPiece);

	set_bit(&b[color][piece], sq);
	set_bit(&all[color], sq);
	piece_on[sq] = piece;

	if (play)
	{
		set_bit(&st->occ, sq);
		st->key ^= zob[color][piece][sq];
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

	if (play)
	{
		clear_bit(&st->occ, sq);
		st->key ^= zob[color][piece][sq];
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

	while (pinners)
	{
		Square sq = next_bit(&pinners);
		uint64_t b = Between[ksq][sq] & ~(1ULL << sq) & st->occ;
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

	return (RQ & rook_attack(kpos, st->occ))
	       | (BQ & bishop_attack(kpos, st->occ))
	       | (b[them][Knight] & NAttacks[kpos])
	       | (b[them][Pawn] & PAttacks[kcolor][kpos]);
}

uint64_t Board::calc_key() const
/* Calculates the zobrist key from scratch. Used to assert() the dynamically computed st->key */
{
	uint64_t key = 0ULL;

	for (Color color = White; color <= Black; ++color)
		for (unsigned piece = Pawn; piece <= King; ++piece)
		{
			uint64_t sqs = b[color][piece];
			while (sqs)
			{
				const unsigned sq = next_bit(&sqs);
				key ^= zob[color][piece][sq];
			}
		}

	return turn ? key ^ zob_turn : key;
}

bool is_stalemate(const Board& B)
{
	assert(!B.get_checkers());
	move_t mlist[MAX_MOVES];
	const uint64_t targets = ~B.get_pieces(B.get_turn());

	return ( !has_piece_moves(B, targets)
	         && gen_pawn_moves(B, targets, mlist, true) == mlist);
}

bool is_mate(const Board& B)
{
	assert(B.get_checkers());
	move_t mlist[MAX_MOVES];
	return gen_evasion(B, mlist) == mlist;
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
