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
#pragma once
#include <string>
#include "bitboard.h"

bool test_perft();

#define MAX_PLY		0x400	// max number of plies for a game
#define MAX_MOVES	0x80	// max number of legal moves

/* Castling flags: those are for White, use << 2 for Black */
enum {
    OO = 1,		// King side castle (OO = chess notation)
    OOO = 2		// Queen side castle (OOO = chess notation)
};

/* Possible results of a game */
enum Result {
    ResultNone,			// game is not over
    ResultThreefold,	// draw by 3-fold repetition
    Result50Move,		// draw by 50 move rule
    ResultMaterial,		// draw by insufficient material
    ResultStalemate,	// stalemate
    ResultMate,			// check mate
    ResultIllegalMove
};

struct move_t {
	uint16_t fsq:8, tsq:8;
	uint16_t promotion:8;
	uint16_t ep:1;

	bool operator== (move_t m) const {
		return fsq == m.fsq && tsq == m.tsq && promotion == m.promotion && ep == m.ep;
	}
};

struct game_info {
	int capture;				// piece just captured
	unsigned epsq;				// en passant square
	unsigned crights;			// castling rights, 4 bits in FEN order KQkq
	move_t last_move;			// last move played (for undo)
	Key key;					// base zobrist key
	uint64_t pinned, dcheckers;	// pinned and discovery checkers for turn
	uint64_t attacked;			// squares attacked by opp_color(turn)
	uint64_t checkers;			// pieces checking turn's King
	uint64_t occ;				// occupancy
	int rule50;					// counter for the 50 move rule

	uint64_t epsq_bb() const { return epsq ? (1ULL << epsq) : 0; }
};

class Board
{
	uint64_t b[NB_COLOR][NB_PIECE];
	uint64_t all[NB_COLOR];
	int piece_on[NB_SQUARE];
	game_info game_stack[MAX_PLY], *_st;
	int turn;
	unsigned king_pos[NB_COLOR];
	int move_count;				// full move count, as per FEN standard
	bool initialized;

	void clear();
	void set_square(int color, int piece, unsigned sq, bool play = true);
	void clear_square(int color, int piece, unsigned sq, bool play = true);

	Key calc_key() const;
	uint64_t calc_attacks(int color) const;
	uint64_t calc_checkers(int kcolor) const;
	uint64_t hidden_checkers(bool find_pins, int color) const;

public:
	const game_info& st() const;

	int get_turn() const;
	int get_move_count() const;
	unsigned get_king_pos(int c) const;
	int get_color_on(unsigned sq) const;
	int get_piece_on(unsigned sq) const;

	uint64_t get_pieces(int color) const;
	uint64_t get_pieces(int color, int piece) const;
	uint64_t get_RQ(int color) const;
	uint64_t get_BQ(int color) const;

	void set_fen(const std::string& fen);
	std::string get_fen() const;

	void play(move_t m);
	void undo();

	bool is_castling(move_t m) const;
	unsigned is_check(move_t m) const;

	move_t string_to_move(const std::string& s);
	std::string move_to_string(move_t m);
	std::string move_to_san(move_t m);
};

extern const std::string PieceLabel[NB_COLOR];
std::ostream& operator<< (std::ostream& ostrm, const Board& B);

/* movegen.cc */

extern uint64_t perft(Board& B, unsigned depth, unsigned ply);

extern move_t *gen_piece_moves(const Board& B, uint64_t targets, move_t *mlist, bool king_moves);
extern move_t *gen_castling(const Board& B, move_t *mlist);
extern move_t *gen_pawn_moves(const Board& B, uint64_t targets, move_t *mlist, bool sub_promotions);
extern move_t *gen_evasion(const Board& B, move_t *mlist);
extern move_t *gen_moves(const Board& B, move_t *mlist);

extern bool has_piece_moves(const Board& B, uint64_t targets);
extern bool has_moves(const Board& B);

inline unsigned pawn_push(int color, unsigned sq)
{
	assert(color_ok(color) && rank(sq) >= RANK_2 && rank(sq) <= RANK_7);
	return color ? sq - 8 : sq + 8;
}
