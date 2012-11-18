#include "eval.h"

void eval_material(const Board& B, Eval e[NB_COLOR])
{
	for (int color = WHITE; color <= BLACK; ++color) {
		// material (PSQ)
		e[color] += B.st().psq[color];

		// bishop pair
		if (several_bits(B.get_pieces(color, BISHOP))) {
			e[color].op += 40;
			e[color].eg += 50;
		}
	}
}

void eval_mobility(const Board& B, Eval e[NB_COLOR])
{
	static const int mob_zero[NB_PIECE] = {0, 3, 4, 5, 0, 0};
	static const unsigned mob_unit[NB_PHASE][NB_PIECE] = {
		{0, 4, 5, 2, 1, 0},		// Opening
		{0, 4, 5, 4, 2, 0}		// EndGame
	};

	for (int color = WHITE; color <= BLACK; color++) {
		const int us = color, them = opp_color(us);

		const Bitboard their_pawns = B.get_pieces(them, PAWN);
		const Bitboard defended = shift_bit(their_pawns & ~FileA_bb, them ? -9 : 7)
		                          | shift_bit(their_pawns & ~FileH_bb, them ? -7 : 9);
		const Bitboard mob_targets = ~(B.get_pieces(us, PAWN) | B.get_pieces(us, KING) | defended);

		Bitboard fss, tss, occ;
		int fsq, piece, count;

		/* Generic linear mobility */
		#define MOBILITY(p0, p)								\
			count = count_bit_max15(tss) - mob_zero[p0];	\
			e[us].op += count * mob_unit[OPENING][p];		\
			e[us].eg += count * mob_unit[ENDGAME][p]

		/* Knight mobility */
		fss = B.get_pieces(us, KNIGHT);
		while (fss) {
			fsq = pop_lsb(&fss);
			tss = NAttacks[fsq] & mob_targets;
			MOBILITY(KNIGHT, KNIGHT);
		}

		/* Lateral mobility */
		fss = B.get_RQ(us);
		occ = B.st().occ & ~B.get_pieces(us, ROOK);		// see through rooks
		while (fss) {
			fsq = pop_lsb(&fss); piece = B.get_piece_on(fsq);
			tss = rook_attack(fsq, occ) & mob_targets;
			MOBILITY(ROOK, piece);
		}

		/* Diagonal mobility */
		fss = B.get_BQ(us);
		occ = B.st().occ & ~B.get_pieces(us, BISHOP);		// see through rooks
		while (fss) {
			fsq = pop_lsb(&fss); piece = B.get_piece_on(fsq);
			tss = bishop_attack(fsq, occ) & mob_targets;
			MOBILITY(BISHOP, piece);
		}
	}
}

int calc_phase(const Board& B)
{
	static const int total = 4*(vN + vB + vR) + 2*vQ;
	return (B.st().piece_psq[WHITE] + B.st().piece_psq[BLACK]) * 1024 / total;
}

int eval(const Board& B)
{
	assert(!B.is_check());
	const int us = B.get_turn(), them = opp_color(us);
	const int phase = calc_phase(B);

	Eval e[NB_COLOR];
	e[WHITE].clear(); e[BLACK].clear();

	eval_mobility(B, e);
	eval_material(B, e);

	return (phase*(e[us].op-e[them].op) + (1024-phase)*(e[us].eg-e[them].eg)) / 1024;
}
