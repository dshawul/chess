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
 *
 * Credits:
 * - Passed pawn scoring is inspired by Stockfish, by Marco Costalba.
*/
#include <cstring>
#include "eval.h"

int KingDistanceToSafety[NB_COLOR][NB_SQUARE];

void init_eval()
{
	for (int us = WHITE; us <= BLACK; ++us)
		for (int sq = A1; sq <= H8; ++sq)
			KingDistanceToSafety[us][sq] = std::min(kdist(sq, us ? E8 : E1), kdist(sq, us ? B8 : B1));
}

struct PawnCache
{
	struct Entry {
		Key key;
		Eval eval_white;
		Bitboard passers;
	};

	PawnCache() { memset(buf, 0, sizeof(buf)); }
	Entry &probe(Key key) { return buf[key & (count-1)]; }

private:
	static const int count = 0x10000;
	Entry buf[count];
};

PawnCache PC;

struct EvalInfo
{
	EvalInfo(const Board *_B): B(_B), eval_factor(16)
	{ e[WHITE] = e[BLACK] = {0,0}; }

	void select_side(int color);
	void eval_material();
	void eval_mobility();
	void eval_safety();
	void eval_pieces();
	
	void eval_pawns();
	void eval_drawish();
	int interpolate() const;

private:
	const Board *B;
	Eval e[NB_COLOR];
	int us, them, our_ksq, their_ksq;
	Bitboard our_pawns, their_pawns;
	int eval_factor;	// between 0 and 16
	
	void score_mobility(int us, int p0, int p, Bitboard tss);
	void score_attacks(int p0, int sq, Bitboard sq_attackers, Bitboard defended,
		int *total_count, int *total_weight);
	
	Bitboard do_eval_pawns();
	void eval_shelter_storm();
	void eval_passer(int sq, Eval* res);
	void eval_passer_interaction(int sq);

	int calc_phase() const;
	Eval eval_white() const { return e[WHITE] - e[BLACK]; }
};

void EvalInfo::select_side(int color)
{
	us = color;
	them = opp_color(color);
	our_ksq = B->get_king_pos(us);
	their_ksq = B->get_king_pos(them);
	our_pawns = B->get_pieces(us, PAWN);
	their_pawns = B->get_pieces(them, PAWN);
}
	
void EvalInfo::eval_material()
{
	// material (PSQ)
	e[us] += B->st().psq[us];

	// bishop pair
	if (several_bits(B->get_pieces(us, BISHOP)))
		e[us] += {40, 50};
}

void EvalInfo::eval_drawish()
{
	// If the stronger side has no pawns, half the material difference in the endgame
	const int strong_side = e[BLACK].eg > e[WHITE].eg;
	if (!B->get_pieces(strong_side, PAWN))
		eval_factor = 8;
	// Opposite color bishop ending
	else if ((B->st().mat_key & 0xFF0000ULL) == 0x110000ULL) {
		// Each side has exactly one bishop
		const Bitboard b = B->get_pieces(WHITE, BISHOP) | B->get_pieces(BLACK, BISHOP);
		static const Bitboard white_sqaures = 0x55AA55AA55AA55AAULL;
		static const Bitboard black_sqaures = 0xAA55AA55AA55AA55ULL;
		if ((b & white_sqaures) && (b & black_sqaures))
			// We have an opposite color bishop situation. If the stronger side has no other
			// pieces it's more drawish.
			eval_factor = (B->st().mat_key & (0x0F0F000F00ULL << (4*strong_side))) ? 12 : 8;
	}
}

void EvalInfo::score_mobility(int us, int p0, int p, Bitboard tss)
{
	static const int mob_count[ROOK+1][15] = {
		{},
		{-3, -2, -1, 0, 1, 2, 3, 4, 4},
		{-4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 5, 6, 6, 7},
		{-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 6, 7, 7}
	};
	static const unsigned mob_unit[NB_PHASE][NB_PIECE] = {
		{0, 4, 5, 2, 1, 0},		// Opening
		{0, 4, 5, 4, 2, 0}		// EndGame
	};

	const int count = mob_count[p0][count_bit(tss)];
	e[us].op += count * mob_unit[OPENING][p];
	e[us].eg += count * mob_unit[ENDGAME][p];
}

void EvalInfo::eval_mobility()
{
	const Bitboard mob_targets = ~(our_pawns | B->get_pieces(us, KING)
		| B->st().attacks[them][PAWN]);

	Bitboard fss, tss, occ;
	int fsq, piece;

	// Knight mobility
	fss = B->get_pieces(us, KNIGHT);
	while (fss) {
		tss = NAttacks[pop_lsb(&fss)] & mob_targets;
		score_mobility(us, KNIGHT, KNIGHT, tss);
	}

	// Lateral mobility
	fss = B->get_RQ(us);
	occ = B->st().occ & ~B->get_pieces(us, ROOK);		// see through rooks
	while (fss) {
		fsq = pop_lsb(&fss);
		piece = B->get_piece_on(fsq);
		tss = rook_attack(fsq, occ) & mob_targets;
		score_mobility(us, ROOK, piece, tss);
	}

	// Diagonal mobility
	fss = B->get_BQ(us);
	occ = B->st().occ & ~B->get_pieces(us, BISHOP);		// see through rooks
	while (fss) {
		fsq = pop_lsb(&fss);
		piece = B->get_piece_on(fsq);
		tss = bishop_attack(fsq, occ) & mob_targets;
		score_mobility(us, BISHOP, piece, tss);
	}
}

void EvalInfo::score_attacks(int p0, int sq, Bitboard sq_attackers, Bitboard defended,
	int *total_count, int *total_weight)
{
	static const int AttackWeight[NB_PIECE] = {0, 3, 3, 4, 0, 0};
	
	if (sq_attackers) {
		int count = count_bit(sq_attackers);
		*total_weight += AttackWeight[p0] * count;
		if (test_bit(defended, sq)) count--;
		*total_count += count;
	}
}

void EvalInfo::eval_safety()
{
	// Squares that defended by pawns or occupied by attacker pawns, are useless as far as piece
	// attacks are concerned
	const Bitboard solid = B->st().attacks[us][PAWN] | their_pawns;

	// Defended by our pieces
	const Bitboard defended = B->st().attacks[us][KNIGHT] | B->st().attacks[us][BISHOP]
		| B->st().attacks[us][ROOK];

	int total_weight = 0, total_count = 0, sq;
	Bitboard sq_attackers, attacked, occ, fss;

	// Knight attacks
	attacked = B->st().attacks[them][KNIGHT] & (KAttacks[our_ksq] | NAttacks[our_ksq]) & ~solid;
	if (attacked) {
		fss = B->get_pieces(them, KNIGHT);
		while (attacked) {
			sq = pop_lsb(&attacked);
			sq_attackers = NAttacks[sq] & fss;
			score_attacks(KNIGHT, sq, sq_attackers, defended, &total_count, &total_weight);
		}
	}

	// Lateral attacks
	attacked = B->st().attacks[them][ROOK] & KAttacks[our_ksq] & ~solid;
	if (attacked) {
		fss = B->get_RQ(them);
		occ = B->st().occ & ~fss;	// rooks and queens see through each other
		while (attacked) {
			sq = pop_lsb(&attacked);
			sq_attackers = fss & rook_attack(sq, occ);
			score_attacks(ROOK, sq, sq_attackers, defended, &total_count, &total_weight);
		}
	} else if ( (fss = RPseudoAttacks[our_ksq] & B->get_RQ(them)) )
		// hidden attackers: increment count when the attacking line contains at most one pawn
		while (fss) {
			sq = pop_lsb(&fss);
			total_count += !several_bits((our_pawns | their_pawns) & Between[our_ksq][sq]);
		}

	// Diagonal attacks
	attacked = B->st().attacks[them][BISHOP] & KAttacks[our_ksq] & ~solid;
	if (attacked) {
		fss = B->get_BQ(them);
		occ = B->st().occ & ~fss;	// bishops and queens see through each other
		while (attacked) {
			sq = pop_lsb(&attacked);
			sq_attackers = fss & bishop_attack(sq, occ);
			score_attacks(BISHOP, sq, sq_attackers, defended, &total_count, &total_weight);
		}
	} else if ( (fss = BPseudoAttacks[our_ksq] & B->get_BQ(them)) )
		// hidden attackers: increment count when the attacking diagonal contains at most one pawn
		while (fss) {
			sq = pop_lsb(&fss);
			total_count += !several_bits((our_pawns | their_pawns) & Between[our_ksq][sq]);
		}

	// Adjust for king's "distance to safety"
	total_count += KingDistanceToSafety[us][our_ksq];
	
	if (total_weight) {
		// if king cannot retreat increase penalty
		if ( Shield[them][our_ksq]
			&& (Shield[them][our_ksq] & ~B->st().attacks[them][NO_PIECE] & ~B->get_pieces(us)) )
			++total_count;
		
		e[us].op -= total_count * total_weight;
	}
}

void EvalInfo::eval_passer_interaction(int sq)
// Passed pawn eval step 2: interaction with pieces. This part cannot be done in do_eval_pawns() as
// it is piece dependant, and we need to separate what is KP dependant from the rest (or we cannot
// use the Pawn Cache with KP key)
{
	const int us = B->get_color_on(sq), them = opp_color(us);

	if (!B->st().piece_psq[them]) {
		// opponent has no pieces
		const int psq = square(us ? RANK_1 : RANK_8, file(sq));
		const int pd = kdist(sq, psq);
		const int kd = kdist(B->get_king_pos(them), psq) - (them == B->get_turn());

		if (kd > pd) {	// unstoppable passer
			e[us].eg += vR;	// on top of the bonus from do_eval_pawns()
			return;
		}
	}

	const int r = rank(sq);
	const int L = (us ? 7-r : r) - RANK_2;	// Linear part		0..5
	const int Q = L*(L-1);					// Quadratic part	0..20

	if (Q && !test_bit(B->st().occ, pawn_push(us, sq))) {
		const Bitboard path = SquaresInFront[us][sq];
		const Bitboard b = file_bb(file(sq)) & rook_attack(sq, B->st().occ);

		uint64_t defended, attacked;
		if (B->get_RQ(them) & b) {
			defended = path & B->st().attacks[us][NO_PIECE];
			attacked = path;
		} else {
			defended = (B->get_RQ(us) & b) ? path : path & B->st().attacks[us][NO_PIECE];
			attacked = path & (B->st().attacks[them][NO_PIECE] | B->get_pieces(them));
		}

		if (!attacked)
			// Promotion path is all defended
			e[us].eg += Q * (path == defended ? 7 : 6);
		else
			// Attacked squares on promotion path are defended
			e[us].eg += Q * (!(attacked & ~defended) ? 4 : 2);
	}
}

void EvalInfo::eval_pawns()
{
	const Key key = B->st().kpkey;
	PawnCache::Entry h = PC.probe(key);

	if (h.key == key)
		e[WHITE] += h.eval_white;
	else {
		const Eval ew0 = eval_white();
		h.key = key;
		select_side(WHITE); h.passers  = do_eval_pawns();
		select_side(BLACK); h.passers |= do_eval_pawns();		
		h.eval_white = eval_white() - ew0;
	}

	// piece-dependant passed pawn scoring
	Bitboard b = h.passers;
	while (b)
		eval_passer_interaction(pop_lsb(&b));
}

void EvalInfo::eval_shelter_storm()
{
	static const int ShelterPenalty[8] = {55, 0, 15, 40, 50, 55, 55, 0};
	static const int StormPenalty[8] = {10, 0, 40, 20, 10, 0, 0, 0};

	const int kf = file(our_ksq);

	for (int f = kf-1; f <= kf+1; ++f) {
		if (f < FILE_A || f > FILE_H)
			continue;

		Bitboard b;
		int r, sq;
		bool half;

		// Pawn shelter
		b = our_pawns & file_bb(f);
		r = b ? (us ? 7-rank(msb(b)) : rank(lsb(b))): 0;
		half = f != kf;
		e[us].op -= ShelterPenalty[r] >> half;

		// Pawn storm
		b = their_pawns & file_bb(f);
		if (b) {
			sq = us ? msb(b) : lsb(b);
			r = us ? 7-rank(sq) : rank(sq);
			half = test_bit(our_pawns, pawn_push(them, sq));
		} else {
			r = RANK_1;		// actually we penalize for the semi open file here
			half = false;
		}
		e[us].op -= StormPenalty[r] >> half;
	}
}

void EvalInfo::eval_passer(int sq, Eval *res)
{
	const int r = rank(sq), f = file(sq);
	const int next_sq = pawn_push(us, sq);
	const Bitboard besides = our_pawns & AdjacentFiles[f];
	
	const int L = (us ? RANK_8-r : r)-RANK_2;	// Linear part		0..5
	const int Q = L*(L-1);						// Quadratic part	0..20

	// score based on rank
	res->op += 6 * Q;
	res->eg += 3 * (Q + L + 1);

	if (Q) {
		// adjustment for king distance
		res->eg += kdist(next_sq, their_ksq) * 2 * Q;
		res->eg -= kdist(next_sq, our_ksq) * Q;
		if (rank(next_sq) != (us ? RANK_1 : RANK_8))
			res->eg -= kdist(pawn_push(us, next_sq), our_ksq) * Q / 2;
	}

	// support by friendly pawn
	if (besides & PawnSpan[them][next_sq]) {
		if (PAttacks[them][next_sq] & our_pawns)
			res->eg += 8 * L;	// besides is good, as it allows a further push
		else if (PAttacks[them][sq] & our_pawns)
			res->eg += 5 * L;	// behind is solid, but doesn't allow further push
		else if (!(PAttacks[them][sq] & (their_pawns | B->st().attacks[them][PAWN]))) {
			// pawns that are 1 push away
			Bitboard b = PAttacks[them][sq];
			// also pawns that are 1 double push away if on 5-th (relative) rank
			// for simplicity neglect en-passant refutation of the double push
			if (L == 3)
				b |= PAttacks[them][pawn_push(them, sq)];
			
			while (b) {
				const int tsq = pop_lsb(&b);
				if (test_bit(our_pawns, pawn_push(them, tsq)))
					res->eg += 2 * L;	// 1 push away from defendint the passer
			}
		}
	}
}

Bitboard EvalInfo::do_eval_pawns()
{
	static const int Chained = 5, Isolated = 20;
	static const Eval Hole = {16, 10};
	Bitboard passers = 0;
	
	eval_shelter_storm();
	
	Bitboard sqs = our_pawns;
	while (sqs) {
		const int sq = pop_lsb(&sqs), next_sq = pawn_push(us, sq);
		const int r = rank(sq), f = file(sq);
		const Bitboard besides = our_pawns & AdjacentFiles[f];

		const bool chained = besides & (rank_bb(r) | rank_bb(us ? r+1 : r-1));
		const bool hole = !chained && !(PawnSpan[them][next_sq] & our_pawns)
						  && test_bit(B->st().attacks[them][PAWN], next_sq);
		const bool isolated = !besides;

		const bool open = !(SquaresInFront[us][sq] & (our_pawns | their_pawns));
		const bool passed = open && !(PawnSpan[us][sq] & their_pawns);
		const bool candidate = chained && open && !passed
							   && !several_bits(PawnSpan[us][sq] & their_pawns);

		if (chained)
			e[us].op += Chained;
		else if (hole) {
			e[us].op -= open ? Hole.op : Hole.op/2;
			e[us].eg -= Hole.eg;
		} else if (isolated) {
			e[us].op -= open ? Isolated : Isolated/2;
			e[us].eg -= Isolated;
		}

		if (candidate) {
			Eval tmp = {0,0};
			eval_passer(sq, &tmp);
			e[us] += {tmp.op/2, tmp.eg/2};
		} else if (passed) {
			set_bit(&passers, sq);
			eval_passer(sq, &e[us]);
		}
	}

	return passers;
}

void EvalInfo::eval_pieces()
{
	static const int RookOpen = 8, RookTrapped = 40;
	static const uint64_t BishopTrap[NB_COLOR] = {
		(1ULL << A7) | (1ULL << H7) | (1ULL << A6) | (1ULL << H6),
		(1ULL << A2) | (1ULL << H2) | (1ULL << A3) | (1ULL << H3)
	};
	static const Bitboard KnightTrap[NB_COLOR] = {0xFFFF000000000000ULL, 0x000000000000FFFFULL};

	const bool can_castle = B->st().crights & (3 << (2*us));
	Bitboard fss, tss;

	// Rook on open file
	fss = B->get_pieces(us, ROOK);
	while (fss) {
		const int rsq = pop_lsb(&fss);
		const Bitboard ahead = SquaresInFront[us][rsq];
		if (!(our_pawns & ahead)) {
			int bonus = RookOpen;
			if (!(their_pawns & ahead))
				bonus += RookOpen/2;
			e[us] += {bonus, bonus/2};
		}
	}
	
	// Rook blocked by uncastled King
	fss = B->get_pieces(us, ROOK) & PPromotionRank[them];
	while (fss) {
		const int rsq = pop_lsb(&fss);
		if (test_bit(Between[rsq][us ? E8 : E1], our_ksq)) {
			if (our_pawns & SquaresInFront[us][rsq] & HalfBoard[us])
				e[us].op -= RookTrapped >> can_castle;
			else
				e[us].op -= (RookTrapped/2) >> can_castle;

			break;  // King can only trap one Rook
		}
	}

	// Knight trapped
	fss = B->get_pieces(us, KNIGHT) & KnightTrap[us];
	while (fss) {
		// escape squares = not defended by enemy pawns
		tss = NAttacks[pop_lsb(&fss)] & ~B->st().attacks[them][PAWN];
		// If escape square(s) are attacked and not defended by a pawn, then the knight is likely
		// to be trapped and we penalize it
		if (!(tss & ~(B->st().attacks[them][NO_PIECE] & ~B->st().attacks[us][PAWN])))
			e[us].op -= vOP;
		// in the endgame, we only look at king attacks, and incentivise the king to go and grab
		// the trapped knight
		if (!(tss & ~(B->st().attacks[them][KING] & ~B->st().attacks[us][PAWN])))
			e[us].eg -= vEP;
	}

	// Bishop trapped
	fss = B->get_pieces(us, BISHOP) & BishopTrap[us];
	while (fss) {
		const int fsq = pop_lsb(&fss);
		// See if the retreat path of the bishop is blocked by a defended pawn
		if (B->get_pieces(them, PAWN) & B->st().attacks[them][NO_PIECE] & PAttacks[them][fsq]) {
			e[us].op -= vOP;
			// in the endgame, we only penalize if there's no escape via the 8th rank
			if (PAttacks[us][fsq] & B->st().attacks[them][KING])
				e[us].eg -= vEP;
		}
	}

	// Hanging pieces
	Bitboard loose_pawns = our_pawns & ~B->st().attacks[us][NO_PIECE];
	Bitboard loose_pieces = (B->get_pieces(us) & ~our_pawns)
							& (B->st().attacks[them][PAWN] | ~B->st().attacks[us][PAWN]);
	Bitboard hanging = (loose_pawns | loose_pieces) & B->st().attacks[them][NO_PIECE];
	while (hanging) {
		const int victim = B->get_piece_on(pop_lsb(&hanging));
		e[us].op -= 4 + Material[victim].op/32;
		e[us].eg -= 8 + Material[victim].eg/32;
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
	const int eval = (phase*(e[us].op-e[them].op) + (1024-phase)*(e[us].eg-e[them].eg)) / 1024;
	return eval * eval_factor / 16;
}

bool kpk_draw(const Board& B)
{
	const int us = B.get_pieces(WHITE, PAWN) ? WHITE : BLACK;
	int wk = B.get_king_pos(us), bk = B.get_king_pos(opp_color(us));
	int wp = lsb(B.get_pieces(us, PAWN));
	int stm = B.get_turn();
	
	if (us == BLACK) {
		wk = rank_mirror(wk);
		bk = rank_mirror(bk);
		wp = rank_mirror(wp);
		stm = opp_color(stm);
	}
	if (file(wp) > FILE_D) {
		wk = file_mirror(wk);
		bk = file_mirror(bk);
		wp = file_mirror(wp);
	}
	
	return !probe_kpk(wk, bk, stm, wp);
}

bool kbpk_draw(const Board& B)
{
	const int us = B.get_pieces(WHITE, PAWN) ? WHITE : BLACK;
	int our_king = B.get_king_pos(us), their_king = B.get_king_pos(opp_color(us));
	int pawn = lsb(B.get_pieces(us, PAWN)), bishop = lsb(B.get_pieces(us, BISHOP));
	int prom_sq = square(us ? RANK_1 : RANK_8, file(pawn));
	int stm = B.get_turn();
	
	return (file(pawn) == FILE_A || file(pawn) == FILE_H)
		&& color_of(bishop) != color_of(prom_sq)
		&& kdist(their_king, prom_sq) < kdist(our_king, prom_sq) - (stm == us)
		&& kdist(their_king, prom_sq) - (stm != us) <= kdist(pawn, prom_sq);
}

#define KPK		0x110000000001
#define KKP		0x110000000010
#define KBPK	0x110000010001
#define KKBP	0x110000100010

int eval(const Board& B)
{
	assert(!B.is_check());

	// Recognize some known draws
	const Bitboard mk = B.st().mat_key;
	if ((mk == KPK || mk == KKP) && kpk_draw(B))
		return 0;
	else if ((mk == KBPK || mk == KKBP) && kbpk_draw(B))
		return 0;
	
	EvalInfo ei(&B);
	ei.eval_pawns();
	
	for (int color = WHITE; color <= BLACK; ++color) {
		ei.select_side(color);
		ei.eval_material();
		ei.eval_mobility();
		ei.eval_safety();
		ei.eval_pieces();
	}
	
	ei.eval_drawish();
	return ei.interpolate();
}

int stand_pat_penalty(const Board& B)
{
	Bitboard b = hanging_pieces(B, B.get_turn());
	
	if (several_bits(b)) {
		// Several pieces are hanging. Take the lowest one and return half its value.
		int piece = KING;
		while (b) {
			const int sq = pop_lsb(&b);
			const int p = B.get_piece_on(sq);
			piece = std::min(piece, p);
		}
		return Material[piece].op / 2;
	} else if (b & B.st().pinned) {
		// Only one piece hanging, but also pinned. Return half its value.
		assert(count_bit(b) == 1);
		const int sq = lsb(b), piece = B.get_piece_on(sq);
		return Material[piece].op / 2;
	}
	
	return 0;
}

int asymmetric_eval(const Board& B)
{
	static const int TEMPO = 4;
	return TEMPO - stand_pat_penalty(B);
}