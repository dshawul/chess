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

typedef struct
{
	Square fsq, tsq;
	Piece promotion;
	bool ep:1;
} move_t;

typedef struct
{
	Piece piece, capture;		// undo info
	Square epsq;				// en passant square
	unsigned crights;			// castling rights, 4 bits in FEN order KQkq
	move_t last_move;			// last move played (for undo)
	uint64_t key;				// base zobrist key
	uint64_t pinned, dcheckers;	// pinned and discovery check candidates from turn
	uint64_t attacked;			// squares attacked by opp_color(turn)
	uint64_t checkers;			// pieces checking turn's King
	uint64_t occ;				// occupancy
	int rule50;					// counter for the 50 move rule
} game_info;

typedef struct
{
	uint64_t b[NB_COLOR][NB_PIECE];
	uint64_t all[NB_COLOR];
	Color turn;
	Piece piece_on[NB_SQUARE];
	Square king_pos[NB_COLOR];
	game_info game_stack[MAX_PLY], *st;
	bool initialized;
} Board;

extern const char *PieceLabel[NB_COLOR];

/* board.c */

extern void clear_board(Board *B);			// empty the board
extern void print_board(const Board *B);	// print board ou stdout

extern void set_fen(Board *B, const char *fen);	// setup Board from FEN string
extern void get_fen(const Board *B, char *fen);	// make FEN string from Board

extern void play(Board *B, move_t m);	// play a move
extern void undo(Board *B);				// undo (the last move played)

extern uint64_t calc_attacks(const Board *B, Color color);	// squares attacked by color
extern bool board_is_check(const Board *B);
extern bool board_is_double_check(const Board *B);

extern uint64_t get_RQ(const Board *B, Color color);	// Rooks and Queens of color
extern uint64_t get_BQ(const Board *B, Color color);	// Bishops and Queens of color
extern uint64_t get_epsq_bb(const Board *B);			// ep-square, bitboard version
extern Color color_on(const Board *B, Square sq);		// color of piece on square (NoColor if empty)

extern Result game_over(const Board *B);	// returns enum Result

/* move.c */

extern bool move_equal(move_t m1, move_t m2);
extern bool move_is_legal(Board *B, move_t m);
extern bool move_is_castling(const Board *B, move_t m);
extern unsigned move_is_check(const Board *B, move_t m);

extern move_t string_to_move(const Board *B, const char *s);
extern void move_to_string(const Board *B, move_t m, char *s);
extern void move_to_san(const Board *B, move_t m, char *s);

/* movegen.c */

extern uint64_t perft(Board *B, unsigned depth, unsigned ply);

extern move_t *gen_piece_moves(const Board *B, uint64_t targets, move_t *mlist, bool king_moves);
extern move_t *gen_castling(const Board *B, move_t *mlist);
extern move_t *gen_pawn_moves(const Board *B, uint64_t targets, move_t *mlist, bool sub_promotions);
extern move_t *gen_evasion(const Board *B, move_t *mlist);
extern move_t *gen_moves(const Board *B, move_t *mlist);

extern bool has_piece_moves(const Board *B, uint64_t targets);
extern bool has_moves(const Board *B);
