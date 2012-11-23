#include "eval.h"

Bitboard InFront[NB_COLOR][NB_RANK_FILE];
Bitboard PawnsAttacking[NB_COLOR][NB_SQUARE];
Bitboard Shield[NB_COLOR][NB_SQUARE];
int KingDistanceToSafety[NB_COLOR][NB_SQUARE];

int kdist(int s1, int s2)
{
	return std::max(std::abs(file(s1)-file(s2)), std::abs(rank(s1)-rank(s2)));
}

void init_eval()
{
	InFront[WHITE][RANK_8] = InFront[BLACK][RANK_1] = 0;
	for (int rw = RANK_7, rb = RANK_2; rw >= RANK_1; rw--, rb++) {
		InFront[WHITE][rw] = InFront[WHITE][rw+1] | rank_bb(rw+1);
		InFront[BLACK][rb] = InFront[BLACK][rb-1] | rank_bb(rb-1);
	}

	for (int us = WHITE; us <= BLACK; ++us) {
		for (int sq = A1; sq <= H8; ++sq) {
			Shield[us][sq] = KAttacks[sq] & InFront[us][rank(sq)];
			PawnsAttacking[us][sq] = shift_bit(KAttacks[sq] & ~FileA_bb, us ? -9 : 7)
				| shift_bit(KAttacks[sq] & ~FileH_bb, us ? -7 : 9);
			KingDistanceToSafety[us][sq] = std::min(kdist(sq, us ? E8 : E1), kdist(sq, us ? B8 : B1));
		}
	}
}

class EvalInfo
{
public:
	EvalInfo(const Board *_B): B(_B) { e[WHITE].clear(); e[BLACK].clear(); }
	void eval_material();
	void eval_mobility();
	void eval_safety();
	int interpolate() const;
	
private:
	const Board *B;
	Eval e[NB_COLOR];
	Bitboard attacks[NB_COLOR][ROOK+1];
	
	int calc_phase() const;
};

void EvalInfo::eval_material()
{
	for (int color = WHITE; color <= BLACK; ++color) {
		// material (PSQ)
		e[color] += B->st().psq[color];

		// bishop pair
		if (several_bits(B->get_pieces(color, BISHOP))) {
			e[color].op += 40;
			e[color].eg += 50;
		}
	}
}

void EvalInfo::eval_mobility()
{
	static const int mob_zero[NB_PIECE] = {0, 3, 4, 5, 0, 0};
	static const unsigned mob_unit[NB_PHASE][NB_PIECE] = {
		{0, 4, 5, 2, 1, 0},		// Opening
		{0, 4, 5, 4, 2, 0}		// EndGame
	};

	for (int color = WHITE; color <= BLACK; color++) {
		const int us = color, them = opp_color(us);

		const Bitboard their_pawns = B->get_pieces(them, PAWN);
		const Bitboard defended = attacks[them][PAWN] =
		                              shift_bit(their_pawns & ~FileA_bb, them ? -9 : 7)
		                              | shift_bit(their_pawns & ~FileH_bb, them ? -7 : 9);
		const Bitboard mob_targets = ~(B->get_pieces(us, PAWN) | B->get_pieces(us, KING) | defended);

		Bitboard fss, tss, occ;
		int fsq, piece, count;

		/* Generic linear mobility */
		#define MOBILITY(p0, p)											\
			attacks[us][p0] |= tss;										\
			count = count_bit_max15(tss & mob_targets) - mob_zero[p0];	\
			e[us].op += count * mob_unit[OPENING][p];					\
			e[us].eg += count * mob_unit[ENDGAME][p]

		/* Knight mobility */
		attacks[us][KNIGHT] = 0;
		fss = B->get_pieces(us, KNIGHT);
		while (fss) {
			tss = NAttacks[pop_lsb(&fss)];
			MOBILITY(KNIGHT, KNIGHT);
		}

		/* Lateral mobility */
		attacks[us][ROOK] = 0;
		fss = B->get_RQ(us);
		occ = B->st().occ & ~B->get_pieces(us, ROOK);		// see through rooks
		while (fss) {
			fsq = pop_lsb(&fss); piece = B->get_piece_on(fsq);
			tss = rook_attack(fsq, occ);
			MOBILITY(ROOK, piece);
		}

		/* Diagonal mobility */
		attacks[us][BISHOP] = 0;
		fss = B->get_BQ(us);
		occ = B->st().occ & ~B->get_pieces(us, BISHOP);		// see through rooks
		while (fss) {
			fsq = pop_lsb(&fss); piece = B->get_piece_on(fsq);
			tss = bishop_attack(fsq, occ);
			MOBILITY(BISHOP, piece);
		}
	}
}

void EvalInfo::eval_safety()
{
	static const int AttackWeight[NB_PIECE] = {3, 3, 3, 4, 0, 0};
	static const int ShieldWeight = 3;

	for (int color = WHITE; color <= BLACK; color++) {
		const int us = color, them = opp_color(us), ksq = B->get_king_pos(us);
		const Bitboard our_pawns = B->get_pieces(us, PAWN), their_pawns = B->get_pieces(them, PAWN);

		// Squares that defended by pawns or occupied by attacker pawns, are useless as far as piece
		// attacks are concerned
		const Bitboard solid = attacks[us][PAWN] | their_pawns;
		// Defended by our pieces
		const Bitboard defended = attacks[us][KNIGHT] | attacks[us][BISHOP] | attacks[us][ROOK];

		int total_weight = 0, total_count = 0, sq, count;
		Bitboard sq_attackers, attacked, occ, fss;

		// Generic attack scoring
		#define ADD_ATTACK(p0)								\
			if (sq_attackers) {								\
				count = count_bit(sq_attackers);			\
				total_weight += AttackWeight[p0] * count;	\
				if (test_bit(defended, sq)) count--;		\
				total_count += count;						\
			}

		// Knight attacks
		attacked = attacks[them][KNIGHT] & (KAttacks[ksq] | NAttacks[ksq]) & ~solid;
		if (attacked) {
			fss = B->get_pieces(them, KNIGHT);
			while (attacked) {
				sq = pop_lsb(&attacked);
				sq_attackers = NAttacks[sq] & fss;
				ADD_ATTACK(KNIGHT);
			}
		}

		// Lateral attacks
		attacked = attacks[them][ROOK] & KAttacks[ksq] & ~solid;
		if (attacked) {
			fss = B->get_RQ(them);
			occ = B->st().occ & ~fss;	// rooks and queens see through each other
			while (attacked) {
				sq = pop_lsb(&attacked);
				sq_attackers = fss & rook_attack(sq, occ);
				ADD_ATTACK(ROOK);
			}
		}

		// Diagonal attacks
		attacked = attacks[them][BISHOP] & KAttacks[ksq] & ~solid;
		if (attacked) {
			fss = B->get_BQ(them);
			occ = B->st().occ & ~fss;	// bishops and queens see through each other
			while (attacked) {
				sq = pop_lsb(&attacked);
				sq_attackers = fss & bishop_attack(sq, occ);
				ADD_ATTACK(BISHOP);
			}
		}

		// Pawn attacks
		fss = PawnsAttacking[us][ksq] & their_pawns;            
		while (fss) {
			sq = pop_lsb(&fss);
			if (PAttacks[them][sq] & KAttacks[ksq] & ~their_pawns) {
				++total_count;
				total_weight += AttackWeight[PAWN];				
			}
		}

		// Adjust for king's "distance to safety"
		total_count += KingDistanceToSafety[us][ksq];

		// Adjust for lack of pawn shield
		total_count += count = 3 - count_bit(Shield[us][ksq] & our_pawns);
		total_weight += count * ShieldWeight;

		if (total_count)
			e[us].op -= total_weight * total_count;
	}
}

int EvalInfo::calc_phase() const
{
	static const int total = 4*(vN + vB + vR) + 2*vQ;
	return (B->st().piece_psq[WHITE] + B->st().piece_psq[BLACK]) * 1024 / total;
}

int EvalInfo::interpolate() const
{
	const int us = B->get_turn(), them = opp_color(us);
	const int phase = calc_phase();
	return (phase*(e[us].op-e[them].op) + (1024-phase)*(e[us].eg-e[them].eg)) / 1024;
}

int eval(const Board& B)
{
	assert(!B.is_check());

	EvalInfo ei(&B);
	ei.eval_material();
	ei.eval_mobility();
	ei.eval_safety();

	return ei.interpolate();
}
