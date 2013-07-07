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
 * You should have received a copy of the GNU General Public License along with this program. If not,
 * see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include <string>
#include "bitboard.h"
#include "psq.h"
#include "move.h"

#define MAX_GAME_PLY		0x400	// max number of plies for a game

/* Castling flags: those are for White, use << 2 for Black */
enum {
	OO = 1,		// King side castle (OO = chess notation)
	OOO = 2		// Queen side castle (OOO = chess notation)
};

struct GameInfo {
	int capture;				// piece just captured
	int epsq;					// en passant square
	int crights;				// castling rights, 4 bits in FEN order KQkq
	move_t last_move;			// last move played (for undo)
	Key key, kpkey, mat_key;	// zobrist key, king+pawn key, material key
	Bitboard pinned, dcheckers;	// pinned and discovery checkers for turn
	Bitboard attacked;			// squares attacked by opp_color(turn)
	Bitboard checkers;			// pieces checking turn's King
	Bitboard occ;				// occupancy
	int rule50;					// counter for the 50 move rule
	Eval psq[NB_COLOR];			// PSQ Eval by color
	int piece_psq[NB_COLOR];	// PSQ Eval.op for pieces only
	Bitboard attacks[NB_COLOR][NB_PIECE + 1];

	Bitboard epsq_bb() const {
		return epsq < NO_SQUARE ? (1ULL << epsq) : 0;
	}
};

struct Board {
	const GameInfo& st() const;

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

	void play(move_t m);
	void undo();

	void set_unwind();
	void unwind();

	bool is_check() const;
	bool is_draw() const;

	Key get_key() const;	// full zobrist key of the position (including ep and crights)
	Key get_dm_key() const;	// hash key of the last two moves

private:
	Bitboard b[NB_PIECE], all[NB_COLOR];
	int piece_on[NB_SQUARE];
	GameInfo game_stack[MAX_GAME_PLY], *sp, *sp0;
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
extern std::ostream& operator<< (std::ostream& ostrm, const Board& B);

extern Bitboard hanging_pieces(const Board& B, int us);
extern Bitboard calc_attackers(const Board& B, int sq, Bitboard occ);

inline int Board::get_color_on(int sq) const
{
	assert(initialized && square_ok(sq));
	return BB::test_bit(all[WHITE], sq) ? WHITE : (BB::test_bit(all[BLACK], sq) ? BLACK : NO_COLOR);
}

inline int Board::get_piece_on(int sq) const
{
	assert(initialized && square_ok(sq));
	return piece_on[sq];
}

inline Bitboard Board::get_pieces(int color) const
{
	assert(initialized && color_ok(color));
	return all[color];
}

inline Bitboard Board::get_pieces(int color, int piece) const
{
	assert(initialized && color_ok(color) && piece_ok(piece));
	return b[piece] & all[color];
}

inline int Board::get_turn() const
{
	assert(initialized);
	return turn;
}

inline int Board::get_king_pos(int c) const
{
	assert(initialized);
	return king_pos[c];
}

inline const GameInfo& Board::st() const
{
	assert(initialized);
	return *sp;
}

inline int Board::get_move_count() const
{
	assert(initialized);
	return move_count;
}

inline Key Board::get_dm_key() const
{
	const GameInfo *p = std::max(sp - 2, sp0);
	return p->key ^ sp->key;
}

inline Bitboard Board::get_N() const
{
	return b[KNIGHT];
}

inline Bitboard Board::get_K() const
{
	return b[KING];
}

inline Bitboard Board::get_RQ(int color) const
{
	return (b[ROOK] | b[QUEEN]) & all[color];
}

inline Bitboard Board::get_BQ(int color) const
{
	return (b[BISHOP] | b[QUEEN]) & all[color];
}

inline Bitboard Board::get_NB(int color) const
{
	return (b[KNIGHT] | b[BISHOP]) & all[color];
}

inline Bitboard Board::get_RQ() const
{
	return b[ROOK] | b[QUEEN];
}

inline Bitboard Board::get_BQ() const
{
	return b[BISHOP] | b[QUEEN];
}

inline Key Board::get_key() const
{
	assert(initialized);
	return st().key
		   ^ (st().epsq == NO_SQUARE ? 0 : BB::zob_ep[st().epsq])
		   ^ BB::zob_castle[st().crights];
}

inline void Board::set_unwind()
{
	sp0 = sp;
}

inline void Board::unwind()
{
	while (sp > sp0) undo();
}

inline bool Board::is_check() const
{
	return st().checkers;
}

