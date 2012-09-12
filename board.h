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
#pragma once
#include "bitboard.h"

/* ANY modification to board.h or bitboard.h implementation must be submitted to this test */
bool test_perft();

#define MAX_PLY		0x400	// max number of plies for a game: #(elements) in game stack
#define MAX_FEN		0x80	// max size for FEN string
#define MAX_MOVES	0x80	// max number of legal moves

/* Castling flags: those are for White, use << 2 for Black */
enum
{
    OO = 1,		// King side castle (OO = chess notation)
    OOO = 2		// Queen side castle (OOO = chess notation)
};

/* Possible results of a game */
enum Result
{
    ResultNone,			// game is not over
    ResultThreefold,	// draw by 3-fold repetition
    Result50Move,		// draw by 50 move rule
    ResultMaterial,		// draw by insufficient material
    ResultStalemate,	// stalemate
    ResultMate,			// check mate
    ResultIllegalMove
};

struct move_t
{
	Square fsq, tsq;
	Piece promotion;
	bool ep:1;

	bool operator== (const move_t& m) const;
};

struct game_info
{
	Piece capture;				// piece just captured
	Square epsq;				// en passant square
	unsigned crights;			// castling rights, 4 bits in FEN order KQkq
	move_t last_move;			// last move played (for undo)
	uint64_t key;				// base zobrist key
	uint64_t pinned, dcheckers;	// pinned and discovery checkers for turn
	uint64_t attacked;			// squares attacked by opp_color(turn)
	uint64_t checkers;			// pieces checking turn's King
	uint64_t occ;				// occupancy
	int rule50;					// counter for the 50 move rule
};

class Board
{
	uint64_t b[NB_COLOR][NB_PIECE];
	uint64_t all[NB_COLOR];
	Piece piece_on[NB_SQUARE];
	game_info game_stack[MAX_PLY];
	Color turn;
	Square king_pos[NB_COLOR];
	bool initialized;

	void clear();
	void set_square(Color color, Piece piece, Square sq, bool play);
	void clear_square(Color color, Piece piece, Square sq, bool play);

	uint64_t calc_key() const;
	uint64_t calc_attacks(Color color) const;
	uint64_t calc_checkers(Color kcolor) const;
	uint64_t hidden_checkers(bool find_pins, Color color) const;

public:
	game_info *st;

	Color get_turn() const;
	Square get_king_pos(Color c) const;
	Color color_on(Square sq) const;
	Piece get_piece_on(Square sq) const;
	uint64_t get_epsq_bb() const;
	uint64_t get_checkers() const;

	uint64_t get_pieces(Color color) const;
	uint64_t get_pieces(Color color, Piece piece) const;
	uint64_t get_RQ(Color color) const;
	uint64_t get_BQ(Color color) const;

	void set_fen(const char *fen);
	void get_fen(char *fen) const;

	void play(const move_t& m);
	void undo();

	Result game_over() const;
};

extern const char *PieceLabel[NB_COLOR];

/* board.cc */

bool is_stalemate(const Board& B);
bool is_mate(const Board& B);

void print(const Board& B);

/* move.cc */

extern bool move_is_legal(Board& B, const move_t& m);
extern bool move_is_castling(const Board& B, const move_t& m);
extern unsigned move_is_check(const Board& B, const move_t& m);

extern move_t string_to_move(const Board& B, const char *s);
extern void move_to_string(const Board& B, const move_t& m, char *s);
void move_to_san(const Board& B, const move_t& m, char *s);

/* movegen.cc */

extern uint64_t perft(Board& B, unsigned depth, unsigned ply);

extern move_t *gen_piece_moves(const Board& B, uint64_t targets, move_t *mlist, bool king_moves);
extern move_t *gen_castling(const Board& B, move_t *mlist);
extern move_t *gen_pawn_moves(const Board& B, uint64_t targets, move_t *mlist, bool sub_promotions);
extern move_t *gen_evasion(const Board& B, move_t *mlist);
extern move_t *gen_moves(const Board& B, move_t *mlist);

extern bool has_piece_moves(const Board& B, uint64_t targets);
extern bool has_moves(const Board& B);
