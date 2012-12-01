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
#include <cstring>
#include "eval.h"

Bitboard PawnsAttacking[NB_COLOR][NB_SQUARE];
int KingDistanceToSafety[NB_COLOR][NB_SQUARE];

int kdist(int s1, int s2)
{
	return std::max(std::abs(file(s1)-file(s2)), std::abs(rank(s1)-rank(s2)));
}

void init_eval()
{
	for (int us = WHITE; us <= BLACK; ++us)
	{
		for (int sq = A1; sq <= H8; ++sq)
		{
			PawnsAttacking[us][sq] = shift_bit(KAttacks[sq] & ~FileA_bb, us ? -9 : 7)
			                         | shift_bit(KAttacks[sq] & ~FileH_bb, us ? -7 : 9);
			KingDistanceToSafety[us][sq] = std::min(kdist(sq, us ? E8 : E1), kdist(sq, us ? B8 : B1));
		}
	}
}

class PHTable
{
public:
	struct Entry
	{
		Key key;
		Eval eval_white;
		Bitboard passers;
	};

	PHTable()
	{
		memset(buf, 0, sizeof(buf));
	}
	Entry &find(Key key)
	{
		return buf[key & (count-1)];
	}

private:
	static const int count = 0x10000;
	Entry buf[count];
};

PHTable PHT;

class EvalInfo
{
public:
	EvalInfo(const Board *_B): B(_B)
	{
		e[WHITE].clear();
		e[BLACK].clear();
	}

	void eval_material();
	void eval_mobility();
	void eval_safety();
	void eval_pawns();	
	int interpolate() const;

private:
	const Board *B;
	Eval e[NB_COLOR];
	Bitboard attacks[NB_COLOR][ROOK+1];

	Bitboard do_eval_pawns();
	void eval_passer(int sq);
	
	int calc_phase() const;
	Eval eval_white() const
	{
		Eval tmp = e[WHITE];
		return tmp -= e[BLACK];
	}
};

void EvalInfo::eval_material()
{
	for (int color = WHITE; color <= BLACK; ++color)
	{
		// material (PSQ)
		e[color] += B->st().psq[color];

		// bishop pair
		if (several_bits(B->get_pieces(color, BISHOP)))
		{
			e[color].op += 40;
			e[color].eg += 50;
		}
	}
}

void EvalInfo::eval_mobility()
{
	static const int mob_zero[NB_PIECE] = {0, 3, 4, 5, 0, 0};
	static const unsigned mob_unit[NB_PHASE][NB_PIECE] =
	{
		{0, 4, 5, 2, 1, 0},		// Opening
		{0, 4, 5, 4, 2, 0}		// EndGame
	};

	for (int color = WHITE; color <= BLACK; color++)
	{
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
		while (fss)
		{
			tss = NAttacks[pop_lsb(&fss)];
			MOBILITY(KNIGHT, KNIGHT);
		}

		/* Lateral mobility */
		attacks[us][ROOK] = 0;
		fss = B->get_RQ(us);
		occ = B->st().occ & ~B->get_pieces(us, ROOK);		// see through rooks
		while (fss)
		{
			fsq = pop_lsb(&fss);
			piece = B->get_piece_on(fsq);
			tss = rook_attack(fsq, occ);
			MOBILITY(ROOK, piece);
		}

		/* Diagonal mobility */
		attacks[us][BISHOP] = 0;
		fss = B->get_BQ(us);
		occ = B->st().occ & ~B->get_pieces(us, BISHOP);		// see through rooks
		while (fss)
		{
			fsq = pop_lsb(&fss);
			piece = B->get_piece_on(fsq);
			tss = bishop_attack(fsq, occ);
			MOBILITY(BISHOP, piece);
		}
	}
}

void EvalInfo::eval_safety()
{
	static const int AttackWeight[NB_PIECE] = {2, 3, 3, 4, 0, 0};
	static const int ShieldWeight = 3;

	for (int color = WHITE; color <= BLACK; color++)
	{
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
		if (attacked)
		{
			fss = B->get_pieces(them, KNIGHT);
			while (attacked)
			{
				sq = pop_lsb(&attacked);
				sq_attackers = NAttacks[sq] & fss;
				ADD_ATTACK(KNIGHT);
			}
		}

		// Lateral attacks
		attacked = attacks[them][ROOK] & KAttacks[ksq] & ~solid;
		if (attacked)
		{
			fss = B->get_RQ(them);
			occ = B->st().occ & ~fss;	// rooks and queens see through each other
			while (attacked)
			{
				sq = pop_lsb(&attacked);
				sq_attackers = fss & rook_attack(sq, occ);
				ADD_ATTACK(ROOK);
			}
		}

		// Diagonal attacks
		attacked = attacks[them][BISHOP] & KAttacks[ksq] & ~solid;
		if (attacked)
		{
			fss = B->get_BQ(them);
			occ = B->st().occ & ~fss;	// bishops and queens see through each other
			while (attacked)
			{
				sq = pop_lsb(&attacked);
				sq_attackers = fss & bishop_attack(sq, occ);
				ADD_ATTACK(BISHOP);
			}
		}

		// Pawn attacks
		fss = PawnsAttacking[us][ksq] & their_pawns;
		while (fss)
		{
			sq = pop_lsb(&fss);
			total_count += count = count_bit(PAttacks[them][sq] & KAttacks[ksq] & ~their_pawns);
			total_weight += AttackWeight[PAWN] * count;
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

void EvalInfo::eval_passer(int sq)
{
	const int us = B->get_color_on(sq), them = opp_color(us);

	if (!B->st().piece_psq[them])
	{
		// opponent has no pieces
		const int psq = square(us ? RANK_1 : RANK_8, file(sq));
		const int pd = kdist(sq, psq);
		const int kd = kdist(B->get_king_pos(them), psq) - (them == B->get_turn());

		if (kd > pd)  	// unstoppable passer
		{
			e[us].eg += vR;	// on top of the bonus from do_eval_pawns()
			return;
		}
	}

	const int r = rank(sq);
	const int L = (us ? 7-r : r) - RANK_2;	// Linear part		0..5
	const int Q = L*(L-1);					// Quadratic part	0..20

	if (Q && !test_bit(B->st().occ, pawn_push(us, sq)))
	{
		const Bitboard path = SquaresInFront[us][sq];
		const Bitboard b = file_bb(file(sq)) & rook_attack(sq, B->st().occ);

		uint64_t defended, attacked;
		if (B->get_RQ(them) & b)
		{
			defended = path & B->st().attacks[us][NO_PIECE];
			attacked = path;
		}
		else
		{
			defended = (B->get_RQ(us) & b) ? path : path & B->st().attacks[us][NO_PIECE];
			attacked = path & (B->st().attacks[them][NO_PIECE] | B->get_pieces(them));
		}

		if (!attacked)
			e[us].eg += Q * (path == defended ? 7 : 6);
		else
			e[us].eg += Q * ((attacked & defended) == attacked ? 5 : 3);
	}
}

void EvalInfo::eval_pawns()
{
	const Key key = B->st().kpkey;
	PHTable::Entry h = PHT.find(key);

	if (h.key == key)
		e[WHITE] += h.eval_white;
	else
	{
		const Eval ew0 = eval_white();
		h.key = key;
		h.passers = do_eval_pawns();
		h.eval_white = eval_white();
		h.eval_white -= ew0;
	}

	// piece-dependant passed pawn scoring
	Bitboard b = h.passers;
	while (b)
		eval_passer(pop_lsb(&b));
}

Bitboard EvalInfo::do_eval_pawns()
{
	static const int Chained = 5, Isolated = 20;
	static const Eval Hole = {16, 10};
	Bitboard passers = 0;

	for (int color = WHITE; color <= BLACK; color++)
	{
		const int us = color, them = opp_color(us);
		const int our_ksq = B->get_king_pos(us), their_ksq = B->get_king_pos(them);
		const Bitboard our_pawns = B->get_pieces(us, PAWN), their_pawns = B->get_pieces(them, PAWN);
		Bitboard sqs = our_pawns;

		while (sqs)
		{
			const int sq = pop_lsb(&sqs), next_sq = pawn_push(us, sq);
			const int r = rank(sq), f = file(sq);
			const Bitboard besides = our_pawns & AdjacentFiles[f];

			const bool chained = besides & (rank_bb(r) | rank_bb(us ? r+1 : r-1));
			const bool hole = !chained && !(PawnSpan[them][next_sq] & our_pawns)
			                  && test_bit(attacks[them][PAWN], next_sq);
			const bool isolated = !besides;

			const bool open = !(SquaresInFront[us][sq] & (our_pawns | their_pawns));
			const bool passed = open && !(PawnSpan[us][sq] & their_pawns);

			if (chained)
				e[us].op += Chained;
			else if (hole)
			{
				e[us].op -= open ? Hole.op : Hole.op/2;
				e[us].eg -= Hole.eg;
			}
			else if (isolated)
			{
				e[us].op -= open ? Isolated : Isolated/2;
				e[us].eg -= Isolated;
			}

			if (passed)
			{
				set_bit(&passers, sq);

				const int L = (us ? RANK_8-r : r)-RANK_2;	// Linear part		0..5
				const int Q = L*(L-1);						// Quadratic part	0..20

				// score based on rank
				e[us].op += 8 * Q;
				e[us].eg += 4 * (Q + L + 1);

				if (Q)
				{
					//  adjustment for king distance
					e[us].eg += kdist(next_sq, their_ksq) * 2 * Q;
					e[us].eg -= kdist(next_sq, our_ksq) * Q;
					if (rank(next_sq) != (us ? RANK_1 : RANK_8))
						e[us].eg -= kdist(pawn_push(us, next_sq), our_ksq) * Q / 2;
				}

				// support by friendly pawn
				if (chained)
				{
					if (PAttacks[them][next_sq] & our_pawns)
						e[us].eg += 8 * L;	// besides is good, as it allows a further push
					else
						e[us].eg += 5 * L;	// behind is solid, but doesn't allow further push
				}
			}
		}
	}

	return passers;
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
	ei.eval_pawns();
	ei.eval_safety();

	return ei.interpolate();
}
