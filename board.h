#pragma once
#include "bitboard.h"

typedef struct {
	uint16_t fsq:6, tsq:6;
	uint16_t promotion:3, ep:1;
} move_t;
static const move_t NoMove = {0,0,0,0};

typedef struct {
	unsigned piece, capture;	// undo info
	unsigned epsq;				// en passant square
	unsigned crights;			// castling rights, 4 bits in FEN order KQkq
	move_t last_move;			// last move played (for undo)
	uint64_t key;				// base zobrist key
	uint64_t pinned, dcheckers;	// pinned and discovery check candidates from turn
	uint64_t attacked;			// squares attacked by opp_color(turn)
	uint64_t checkers;			// pieces checking turn's King
	uint64_t occ;				// occupancy
	unsigned rule50;			// counter for the 50 move rule
} game_info;

#define MAX_PLY	0x400

typedef struct {
	uint64_t b[NB_COLOR][NB_PIECE];
	uint64_t all[NB_COLOR];
	unsigned turn;
	unsigned piece_on[NB_SQUARE];
	unsigned king_pos[NB_COLOR];
	game_info game_stack[MAX_PLY], *st;
	bool initialized;
} Board;

void reset_board(Board *B);			// empty the board
void print_board(const Board *B);	// print board ou stdout

#define MAX_FEN	80
void load_fen(Board *B, const char *fen);	// setup Board from FEN string
void write_fen(const Board *B, char *fen);	// make FEN string from Board

void play(Board *B, move_t m);		// play a move
void undo(Board *B);				// undo (the last move played)

/* move properties */
bool move_ok(Board *B, move_t m);
move_t string_to_move(const Board *B, const char *s);
void move_to_string(const Board *B, move_t m, char *s);
void move_to_san(const Board *B, move_t m, char *s);
unsigned move_is_check(const Board *B, move_t m);

/* move generators */
#define MAX_MOVES	0x80
uint64_t perft(Board *B, unsigned depth, unsigned ply);
move_t *gen_piece_moves(const Board *B, uint64_t targets, move_t *mlist, bool king_moves);
move_t *gen_castling(const Board *B, move_t *mlist);
move_t *gen_pawn_moves(const Board *B, uint64_t targets, move_t *mlist, bool sub_promotions);
move_t *gen_evasion(const Board *B, move_t *mlist);
move_t *gen_moves(const Board *B, move_t *mlist);

uint64_t calc_attacks(const Board *B, unsigned color);

enum {ResultNone, ResultThreefold, Result50Move, ResultMaterial, ResultStalemate, ResultMate};
int game_over(const Board *B);

uint64_t get_RQ(const Board *B, unsigned color);	// Rooks and Queens of color
uint64_t get_BQ(const Board *B, unsigned color);	// Bishops and Queens of color
uint64_t get_epsq_bb(const Board *B);				// ep-square, bitboard version
unsigned color_on(const Board *B, unsigned sq);		// color of piece on square (NoColor if empty)

bool board_is_check(const Board *B);
bool board_is_double_check(const Board *B);

bool move_equal(move_t m1, move_t m2);
bool move_is_castling(const Board *B, move_t m);

extern const char *PieceLabel[NB_COLOR];
enum {OO = 1, OOO = 2};		// castling flags