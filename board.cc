#include "board.h"

const char *PieceLabel[NB_COLOR] = { "PNBRQK", "pnbrqk" };

namespace
{
	const move_t NoMove = {0,0,0,0};

	void set_square(Board *B, unsigned color, unsigned piece, unsigned sq, bool play);
	void clear_square(Board *B, unsigned color, unsigned piece, unsigned sq, bool play);

	uint64_t calc_key(const Board *B);
	uint64_t calc_checkers(const Board *B, unsigned kcolor);
	uint64_t hidden_checkers(const Board *B, unsigned find_pins, unsigned color);

	bool is_stalemate(const Board *B);
	bool is_mate(const Board *B);
}

void clear_board(Board *B)
{
	assert(BitboardInitialized);
	memset(B, 0, sizeof(Board));

	for (unsigned sq = A1; sq <= H8; B->piece_on[sq++] = NoPiece);
	B->st = B->game_stack;
	B->st->epsq = NoSquare;
	B->st->last_move = NoMove;

	B->initialized = true;
}

void set_fen(Board *B, const char *fen)
{
	assert(fen);
	unsigned r, f;
	char c = *fen++;

	// load the board
	clear_board(B);
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
			r--;
			f = FileA;
		}
		// piece
		else
		{
			unsigned sq = square(r, f++), piece;
			for (piece = Pawn; piece <= King; piece++)
			{
				if (c == PieceLabel[isupper(c) ? White : Black][piece])
					break;
			}
			assert(piece_ok(piece));
			unsigned color = isupper(c) ? White : Black;
			set_square(B, color, piece, sq, true);
			if (piece == King)
				B->king_pos[color] = sq;
		}
	}

	// turn of play
	c = *fen++;
	assert(c == 'w' || c == 'b');
	B->turn = c == 'w' ? White : Black;
	B->st->key ^= B->turn ? zob_turn : 0ULL;
	const unsigned us = B->turn, them = opp_color(us);

	// Castling rights
	if (*fen++ != ' ') assert(0);
	c = *fen++;
	B->st->crights = 0;

	if (c == '-')	// no castling rights
		c = *fen++;
	else  			// set castling rights (in reading order KQkq)
	{
		if (c == 'K')
		{
			B->st->crights ^= OO;
			c = *fen++;
		}
		if (c == 'Q')
		{
			B->st->crights ^= OOO;
			c = *fen++;
		}
		if (c == 'k')
		{
			B->st->crights ^= OO << 2;
			c = *fen++;
		}
		if (c == 'q')
		{
			B->st->crights ^= OOO << 2;
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
		B->st->epsq = NoSquare;
	}
	else
	{
		unsigned r, f;
		assert('a' <= c && c <= 'h');
		f = c - 'a';
		c = *fen++;
		assert(isdigit(c));
		r = c - '1';
		B->st->epsq = square (r, f);
	}

	B->st->pinned = hidden_checkers(B, 1, us);
	B->st->dcheckers = hidden_checkers(B, 0, us);

	B->st->attacked = calc_attacks(B, them);
	B->st->checkers = test_bit(B->st->attacked, B->king_pos[us]) ? calc_checkers(B, us) : 0ULL;

	assert(calc_key(B) == B->st->key);
}

void get_fen(const Board *B, char *fen)
{
	assert(B->initialized && fen);

	// write the board
	for (int r = Rank8; r >= Rank1; r--)
	{
		unsigned empty_cnt = 0;
		for (int f = FileA; f <= FileH; f++)
		{
			unsigned sq = square(r, f);
			if (B->piece_on[sq] == NoPiece)
				empty_cnt++;
			else
			{
				if (empty_cnt)
				{
					*fen++ = empty_cnt + '0';
					empty_cnt = 0;
				}
				*fen++ = PieceLabel[color_on(B, sq)][B->piece_on[sq]];
			}
		}
		if (empty_cnt) *fen++ = empty_cnt + '0';
		*fen++ = r == Rank1 ? ' ' : '/';
	}

	// turn of play
	*fen++ = B->turn ? 'b' : 'w';
	*fen++ = ' ';

	// castling rights
	unsigned crights = B->st->crights;
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
	unsigned epsq = B->st->epsq;
	if (epsq != NoSquare)
	{
		*fen++ = file(epsq) + 'a';
		*fen++ = rank(epsq) + '1';
	}
	else
		*fen++ = '-';

	*fen++ = '\0';
}

void print_board(const Board *B)
{
	assert(B->initialized);

	for (int r = Rank8; r >= Rank1; r--)
	{
		for (int f = FileA; f <= FileH; f++)
		{
			unsigned sq = square(r, f),
			         color = color_on(B, sq);
			char c = color != NoColor
			         ? PieceLabel[color][B->piece_on[sq]]
			         : (sq == B->st->epsq ? '*' : '.');
			printf(" %c", c);
		}
		printf("\n");
	}

	char fen[MAX_FEN];
	get_fen(B, fen);
	printf("fen: %s\n", fen);
}

void play(Board *B, move_t m)
{
	assert(B->initialized);

	++B->st;
	memcpy(B->st, B->st-1, sizeof(game_info));
	B->st->last_move = m;
	B->st->rule50++;

	const unsigned us = B->turn, them = opp_color(us);
	const unsigned piece = B->piece_on[m.fsq], capture = B->piece_on[m.tsq];

	if (move_equal(m, NoMove))
	{
		assert(!board_is_check(B));
		B->st->epsq = NoSquare;
	}
	else
	{
		// normal capture: remove captured piece
		if (piece_ok(capture))
		{
			B->st->rule50 = 0;
			clear_square(B, them, capture, m.tsq, true);
		}

		// move our piece
		clear_square(B, us, piece, m.fsq, true);
		set_square(B, us, m.promotion == NoPiece ? piece : m.promotion, m.tsq, true);

		if (piece == Pawn)
		{
			B->st->rule50 = 0;
			int inc_pp = us ? -8 : 8;
			// set the epsq if double push
			B->st->epsq = (m.tsq - m.fsq == 2 * inc_pp)
			              ? m.fsq + inc_pp
			              : NoSquare;
			// capture en passant
			if (m.ep)
				clear_square(B, them, Pawn, m.tsq - inc_pp, true);
		}
		else
		{
			B->st->epsq = NoSquare;

			if (piece == Rook)
			{
				// a rook move can alter castling rights
				if (m.fsq == (us ? H8 : H1))
					B->st->crights &= ~(OO << (2 * us));
				else if (m.fsq == (us ? A8 : A1))
					B->st->crights &= ~(OOO << (2 * us));
			}
			else if (piece == King)
			{
				// update king_pos and clear crights
				B->king_pos[us] = m.tsq;
				B->st->crights &= ~((OO | OOO) << (2 * us));
				// move the rook (jump over the king)
				if (m.tsq == m.fsq+2)  			// OO
				{
					clear_square(B, us, Rook, us ? H8 : H1, true);
					set_square(B, us, Rook, us ? F8 : F1, true);
				}
				else if (m.tsq == m.fsq-2)  	// OOO
				{
					clear_square(B, us, Rook, us ? A8 : A1, true);
					set_square(B, us, Rook, us ? D8 : D1, true);
				}
			}
		}

		if (capture == Rook)
		{
			// Rook captures can alter opponent's castling rights
			if (m.tsq == (us ? H1 : H8))
				B->st->crights &= ~(OO << (2 * them));
			else if (m.tsq == (us ? A1 : A8))
				B->st->crights &= ~(OOO << (2 * them));
		}
	}

	B->turn = them;
	B->st->key ^= zob_turn;

	B->st->piece = piece;
	B->st->capture = capture;

	B->st->pinned = hidden_checkers(B, 1, them);
	B->st->dcheckers = hidden_checkers(B, 0, them);

	B->st->attacked = calc_attacks(B, us);
	B->st->checkers = test_bit(B->st->attacked, B->king_pos[them]) ? calc_checkers(B, them) : 0ULL;

	assert(calc_key(B) == B->st->key);
}

void undo(Board *B)
{
	assert(B->initialized);
	const unsigned us = opp_color(B->turn), them = B->turn;
	const move_t m = B->st->last_move;
	const unsigned piece = B->st->piece, capture = B->st->capture;

	if (move_equal(m, NoMove))
		assert(!board_is_check(B));
	else
	{
		// move our piece back
		clear_square(B, us, m.promotion == NoPiece ? piece : m.promotion, m.tsq, false);
		set_square(B, us, piece, m.fsq, false);

		// restore the captured piece (if any)
		if (piece_ok(capture))
			set_square(B, them, capture, m.tsq, false);

		if (piece == King)
		{
			// update king_pos
			B->king_pos[us] = m.fsq;
			// undo rook jump (castling)
			if (m.tsq == m.fsq+2)  			// OO
			{
				clear_square(B, us, Rook, us ? F8 : F1, false);
				set_square(B, us, Rook, us ? H8 : H1, false);
			}
			else if (m.tsq == m.fsq-2)  	// OOO
			{
				clear_square(B, us, Rook, us ? D8 : D1, false);
				set_square(B, us, Rook, us ? A8 : A1, false);
			}
		}
		else if (m.ep)	// restore the en passant captured pawn
			set_square(B, them, Pawn, m.tsq + (us ? 8 : -8), false);
	}

	B->turn = us;
	--B->st;
}

uint64_t calc_attacks(const Board *B, unsigned color)
{
	assert(B->initialized);

	// King, Knights
	uint64_t b = KAttacks[B->king_pos[color]];
	uint64_t fss = B->b[color][Knight];
	while (fss)
		b |= NAttacks[next_bit(&fss)];

	// Lateral
	fss = get_RQ(B, color);
	while (fss)
		b |= rook_attack(next_bit(&fss), B->st->occ);

	// Diagonal
	fss = get_BQ(B, color);
	while (fss)
		b |= bishop_attack(next_bit(&fss), B->st->occ);

	// Pawns
	b |= shift_bit((B->b[color][Pawn] & ~FileA_bb), color ? -9 : 7);
	b |= shift_bit((B->b[color][Pawn] & ~FileH_bb), color ? -7 : 9);

	return b;
}

Result game_over(const Board *B)
{
	// insufficient material
	if (!B->b[White][Pawn] && !B->b[Black][Pawn]
	        && !get_RQ(B, White) && !get_RQ(B, Black)
	        && !several_bits(B->b[White][Knight] | B->b[White][Bishop])
	        && !several_bits(B->b[Black][Knight] | B->b[Black][Bishop]))
		return ResultMaterial;

	// 50 move rule
	if (B->st->rule50 >= 100)
		return Result50Move;

	// 3-fold repetition
	for (int i = 4; i <= B->st->rule50; i += 2)
		if (B->st[-i].key == B->st->key)
			return ResultThreefold;

	if (board_is_check(B))
		return is_mate(B) ? ResultMate : ResultNone;
	else
		return is_stalemate(B) ? ResultStalemate : ResultNone;
}

unsigned color_on(const Board *B, unsigned sq)
{
	assert(B->initialized && square_ok(sq));
	return test_bit(B->all[White], sq) ? White : (test_bit(B->all[Black], sq) ? Black : NoColor);
}

uint64_t get_RQ(const Board *B, unsigned color)
{
	assert(B->initialized && color_ok(color));
	return B->b[color][Rook] | B->b[color][Queen];
}

uint64_t get_BQ(const Board *B, unsigned color)
{
	assert(B->initialized && color_ok(color));
	return B->b[color][Bishop] | B->b[color][Queen];
}

uint64_t get_epsq_bb(const Board *B)
{
	assert(B->initialized);
	return B->st->epsq < NoSquare ? (1ULL << B->st->epsq) : 0ULL;
}

bool board_is_check(const Board *B)
{
	assert(B->initialized);
	return B->st->checkers;
}

bool board_is_double_check(const Board *B)
{
	assert(B->initialized);
	return several_bits(B->st->checkers);
}

namespace
{
	uint64_t hidden_checkers(const Board *B, unsigned find_pins, unsigned color)
	{
		assert(B->initialized && color_ok(color) && (find_pins == 0 || find_pins == 1));
		const unsigned aside = color ^ find_pins, kside = opp_color(aside);
		uint64_t result = 0ULL, pinners;

		// Pinned pieces protect our king, dicovery checks attack the enemy king.
		const unsigned ksq = B->king_pos[kside];

		// Pinners are only sliders with X-ray attacks to ksq
		pinners = (get_RQ(B, aside) & RPseudoAttacks[ksq]) | (get_BQ(B, aside) & BPseudoAttacks[ksq]);

		while (pinners)
		{
			unsigned sq = next_bit(&pinners);
			uint64_t b = Between[ksq][sq] & ~(1ULL << sq) & B->st->occ;
			// NB: if b == 0 then we're in check

			if (!several_bits(b) && (b & B->all[color]))
				result |= b;
		}
		return result;
	}

	uint64_t calc_checkers(const Board *B, unsigned kcolor)
	{
		assert(B->initialized && color_ok(kcolor));
		const unsigned
		kpos = B->king_pos[kcolor],
		them = opp_color(kcolor);
		const uint64_t
		RQ = get_RQ(B, them) & RPseudoAttacks[kpos],
		BQ = get_BQ(B, them) & BPseudoAttacks[kpos];

		return (RQ & rook_attack(kpos, B->st->occ))
		       | (BQ & bishop_attack(kpos, B->st->occ))
		       | (B->b[them][Knight] & NAttacks[kpos])
		       | (B->b[them][Pawn] & PAttacks[kcolor][kpos]);
	}

	bool is_stalemate(const Board *B)
	{
		assert(B->initialized && !board_is_check(B));
		move_t mlist[MAX_MOVES];
		const uint64_t targets = ~B->all[B->turn];

		return ( !has_piece_moves(B, targets)
		         && gen_pawn_moves(B, targets, mlist, true) == mlist);
	}

	bool is_mate(const Board *B)
	{
		assert(B->initialized && board_is_check(B));
		move_t mlist[MAX_MOVES];
		return gen_evasion(B, mlist) == mlist;
	}

	void set_square(Board *B, unsigned color, unsigned piece, unsigned sq, bool play)
	{
		assert(B->initialized);
		assert(square_ok(sq) && color_ok(color) && piece_ok(piece) && B->piece_on[sq] == NoPiece);

		set_bit(&B->b[color][piece], sq);
		set_bit(&B->all[color], sq);
		B->piece_on[sq] = piece;

		if (play)
		{
			set_bit(&B->st->occ, sq);
			B->st->key ^= zob[color][piece][sq];
		}
	}

	void clear_square(Board *B, unsigned color, unsigned piece, unsigned sq, bool play)
	{
		assert(B->initialized);
		assert(square_ok(sq) && color_ok(color) && piece_ok(piece) && B->piece_on[sq] == piece);

		clear_bit(&B->b[color][piece], sq);
		clear_bit(&B->all[color], sq);
		B->piece_on[sq] = NoPiece;

		if (play)
		{
			clear_bit(&B->st->occ, sq);
			B->st->key ^= zob[color][piece][sq];
		}
	}

	uint64_t calc_key(const Board *B)
	/* Calculates the zobrist key from scratch. Used to assert() the dynamically computed B->st->key */
	{
		uint64_t key = 0ULL;

		for (unsigned color = White; color <= Black; color++)
			for (unsigned piece = Pawn; piece <= King; piece++)
			{
				uint64_t sqs = B->b[color][piece];
				while (sqs)
				{
					const unsigned sq = next_bit(&sqs);
					key ^= zob[color][piece][sq];
				}
			}

		return B->turn ? key ^ zob_turn : key;
	}
}
