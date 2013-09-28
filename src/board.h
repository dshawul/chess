/*
 * DiscoCheck, an UCI chess engine. Copyright (C) 2011-2013 Lucas Braesch.
 *
 * DiscoCheck is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * DiscoCheck is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not
 * see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include <string>
#include "bitboard.h"
#include "psq.h"
#include "move.h"

namespace board {

/* Castling flags: those are for White, use << 2 for Black */
enum {
	OO = 1,		// King side castle (OO = chess notation)
	OOO = 2		// Queen side castle (OOO = chess notation)
};

struct UndoInfo {
	Key key, kpkey, mat_key;	// zobrist key, king+pawn key, material key
	Bitboard pinned, dcheckers;	// pinned and discovery checkers for turn
	Bitboard attacked;			// squares attacked by opp_color(turn)
	Bitboard checkers;			// pieces checking turn's King
	Bitboard occ;				// occupancy
	Bitboard attacks[NB_COLOR][NB_PIECE + 1];
	Eval psq[NB_COLOR];			// PSQ Eval by color

	int capture;				// piece just captured
	int epsq;					// en passant square
	int crights;				// castling rights, 4 bits in FEN order KQkq
	int rule50;					// counter for the 50 move rule
	move::move_t last_move;			// last move played (for undo)
	int piece_psq[NB_COLOR];	// PSQ Eval.op for pieces only

	Bitboard epsq_bb() const {
		return epsq < NO_SQUARE ? (1ULL << epsq) : 0;
	}
};

class Position {
public:
	const UndoInfo& st() const;

	int get_turn() const;
	int get_move_count() const;
	int get_king_pos(int c) const;
	int get_color_on(int sq) const;
	int get_piece_on(int sq) const;

	Bitboard get_pieces(int color) const;
	Bitboard get_pieces(int color, int piece) const;

	Bitboard get_N() const;
	Bitboard get_K() const;
	Bitboard get_RQ() const;
	Bitboard get_BQ() const;
	Bitboard get_RQ(int color) const;
	Bitboard get_BQ(int color) const;
	Bitboard get_NB(int color) const;

	void set_fen(const std::string& fen);
	std::string get_fen() const;

	void play(move::move_t m);
	void undo();

	void set_root();	// set_root() remembers the root position in sp0 (for 2/3-fold is_draw())

	bool is_check() const;
	bool is_draw() const;

	Key get_key() const;	// full zobrist key of the position (including ep and crights)
	Key get_dm_key() const;	// hash key of the last two moves

private:
	Bitboard b[NB_PIECE];		// b[piece]: squares occupied by pieces of type piece (both colors)
	Bitboard all[NB_COLOR];		// all[color]: squares occupied by pieces of color
	int piece_on[NB_SQUARE];	// piece_on[sq]: what piece is on sq (can be NO_PIECE)

	UndoInfo game_stack[0x400];	// undo stack: use fixed size C-array for speed
	UndoInfo *sp;				// pointer to the stack top
	UndoInfo *sp0;				// see set_unwind() and unwind()

	int turn;
	int king_pos[NB_COLOR];
	int move_count;				// full move count, as per FEN standard
	bool initialized;

	void clear();
	void set_square(int color, int piece, int sq, bool play = true);
	void clear_square(int color, int piece, int sq, bool play = true);

	Bitboard calc_attacks(int color) const;
	Bitboard calc_checkers(int kcolor) const;
	Bitboard hidden_checkers(bool find_pins, int color) const;

	bool verify_keys() const;
	bool verify_psq() const;
};

extern const std::string PieceLabel[NB_COLOR];
extern std::ostream& operator<< (std::ostream& ostrm, const Position& B);

extern Bitboard hanging_pieces(const Position& B);
extern Bitboard calc_attackers(const Position& B, int sq, Bitboard occ);

inline int Position::get_color_on(int sq) const
{
	assert(initialized && square_ok(sq));
	return bb::test_bit(all[WHITE], sq) ? WHITE : bb::test_bit(all[BLACK], sq) ? BLACK : NO_COLOR;
}

inline int Position::get_piece_on(int sq) const
{
	assert(initialized && square_ok(sq));
	return piece_on[sq];
}

inline Bitboard Position::get_pieces(int color) const
{
	assert(initialized && color_ok(color));
	return all[color];
}

inline Bitboard Position::get_pieces(int color, int piece) const
{
	assert(initialized && color_ok(color) && piece_ok(piece));
	return b[piece] & all[color];
}

inline int Position::get_turn() const
{
	assert(initialized);
	return turn;
}

inline int Position::get_king_pos(int c) const
{
	assert(initialized);
	return king_pos[c];
}

inline const UndoInfo& Position::st() const
{
	assert(initialized);
	return *sp;
}

inline int Position::get_move_count() const
{
	assert(initialized);
	return move_count;
}

inline Key Position::get_dm_key() const
{
	const UndoInfo *p = std::max(sp - 2, sp0);
	return p->key ^ sp->key;
}

inline Bitboard Position::get_N() const
{
	return b[KNIGHT];
}

inline Bitboard Position::get_K() const
{
	return b[KING];
}

inline Bitboard Position::get_RQ(int color) const
{
	return (b[ROOK] | b[QUEEN]) & all[color];
}

inline Bitboard Position::get_BQ(int color) const
{
	return (b[BISHOP] | b[QUEEN]) & all[color];
}

inline Bitboard Position::get_NB(int color) const
{
	return (b[KNIGHT] | b[BISHOP]) & all[color];
}

inline Bitboard Position::get_RQ() const
{
	return b[ROOK] | b[QUEEN];
}

inline Bitboard Position::get_BQ() const
{
	return b[BISHOP] | b[QUEEN];
}

inline Key Position::get_key() const
{
	assert(initialized);
	return st().key
		^ (st().epsq == NO_SQUARE ? 0 : bb::zob_ep[st().epsq])
		^ bb::zob_castle[st().crights];
}

inline void Position::set_root()
{
	sp0 = sp;
}

inline bool Position::is_check() const
{
	return st().checkers;
}

}	// namespace board

