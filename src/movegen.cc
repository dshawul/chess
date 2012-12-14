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
#include <chrono>
#include "movegen.h"

namespace
{
	move_t *make_pawn_moves(const Board& B, int fsq, int tsq, move_t *mlist,
	                        bool sub_promotions)
	/* Centralise the pawnm moves generation: given (fsq,tsq) the rest follows. We filter here all the
	 * indirect self checks (through fsq, or through the ep captured square) */
	{
		assert(square_ok(fsq) && square_ok(tsq));
		const int us = B.get_turn(), them = opp_color(us);
		int kpos = B.get_king_pos(us);

		// filter self check through fsq
		if (test_bit(B.st().pinned, fsq) && !test_bit(Direction[kpos][fsq], tsq))
			return mlist;

		move_t m;
		m.fsq(fsq);
		m.tsq(tsq);

		if (tsq == B.st().epsq) {
			m.flag(EN_PASSANT);
			Bitboard occ = B.st().occ;
			// play the ep capture on occ
			clear_bit(&occ, m.fsq());
			clear_bit(&occ, pawn_push(them, m.tsq()));	// remove the ep captured enemy pawn
			set_bit(&occ, m.tsq());
			// test for check by a sliding enemy piece
			if ((B.get_RQ(them) & RPseudoAttacks[kpos] & rook_attack(kpos, occ))
			        || (B.get_BQ(them) & BPseudoAttacks[kpos] & bishop_attack(kpos, occ)))
				return mlist;	// illegal move by indirect self check (through the ep captured pawn)
		} else
			m.flag(NORMAL);

		if (test_bit(PPromotionRank[us], tsq)) {
			// promotion(s)
			m.flag(PROMOTION);
			m.prom(QUEEN); *mlist++ = m;
			if (sub_promotions) {
				m.prom(KNIGHT);	*mlist++ = m;
				m.prom(ROOK);	*mlist++ = m;
				m.prom(BISHOP);	*mlist++ = m;
			}
		} else
			// non promotions: normal or en-passant
			*mlist++ = m;

		return mlist;
	}

	move_t *make_piece_moves(const Board& B, int fsq, Bitboard tss, move_t *mlist)
	/* Centralise the generation of a piece move: given (fsq,tsq) the rest follows. We filter indirect
	 * self checks here. Note that direct self-checks aren't generated, so we don't check them here. In
	 * other words, we never put our King in check before calling this function */
	{
		assert(square_ok(fsq));
		const int kpos = B.get_king_pos(B.get_turn());

		move_t m;
		m.fsq(fsq);
		m.flag(NORMAL);

		if (test_bit(B.st().pinned, fsq))
			tss &= Direction[kpos][fsq];

		while (tss) {
			m.tsq(pop_lsb(&tss));
			*mlist++ = m;
		}

		return mlist;
	}
}

move_t *gen_piece_moves(const Board& B, Bitboard targets, move_t *mlist, bool king_moves)
/* Generates piece moves, when the board is not in check. Uses targets to filter the tss, eg.
 * targets = ~friends (all moves), empty (quiet moves only), enemies (captures only). */
{
	assert(!king_moves || !B.is_check());	// do not use when in check (use gen_evasion)
	const int us = B.get_turn();
	assert(!(targets & B.get_pieces(us)));
	Bitboard fss;

	// Knight Moves
	fss = B.get_pieces(us, KNIGHT);
	while (fss) {
		int fsq = pop_lsb(&fss);
		Bitboard tss = NAttacks[fsq] & targets;
		mlist = make_piece_moves(B, fsq, tss, mlist);
	}

	// Rook Queen moves
	fss = B.get_RQ(us);
	while (fss) {
		int fsq = pop_lsb(&fss);
		Bitboard tss = targets & rook_attack(fsq, B.st().occ);
		mlist = make_piece_moves(B, fsq, tss, mlist);
	}

	// Bishop Queen moves
	fss = B.get_BQ(us);
	while (fss) {
		int fsq = pop_lsb(&fss);
		Bitboard tss = targets & bishop_attack(fsq, B.st().occ);
		mlist = make_piece_moves(B, fsq, tss, mlist);
	}

	// King moves (king_moves == false is only used for check escapes)
	if (king_moves) {
		int fsq = B.get_king_pos(us);
		// here we also filter direct self checks, which shouldn't be sent to serialize_moves
		Bitboard tss = KAttacks[fsq] & targets & ~B.st().attacked;
		mlist = make_piece_moves(B, fsq, tss, mlist);
	}

	return mlist;
}

move_t *gen_castling(const Board& B, move_t *mlist)
/* Generates castling moves, when the board is not in check. The only function that doesn't go
 * through serialize_moves, as castling moves are very peculiar, and we don't want to pollute
 * serialize_moves with this over-specific code */
{
	assert(!B.is_check());
	const int us = B.get_turn();

	move_t m;
	m.fsq(B.get_king_pos(us));
	m.flag(CASTLING);

	if (B.st().crights & (OO << (2 * us))) {
		Bitboard safe = 3ULL << (m.fsq() + 1);	// must not be attacked
		Bitboard empty = safe;					// must be empty

		if (!(B.st().attacked & safe) && !(B.st().occ & empty)) {
			m.tsq(m.fsq() + 2);
			*mlist++ = m;
		}
	}
	if (B.st().crights & (OOO << (2 * us))) {
		Bitboard safe = 3ULL << (m.fsq() - 2);	// must not be attacked
		Bitboard empty = safe | (1ULL << (m.fsq() - 3));	// must be empty

		if (!(B.st().attacked & safe) && !(B.st().occ & empty)) {
			m.tsq(m.fsq() - 2);
			*mlist++ = m;
		}
	}

	return mlist;
}

move_t *gen_pawn_moves(const Board& B, Bitboard targets, move_t *mlist, bool sub_promotions)
/* Generates pawn moves, when the board is not in check. These are of course: double and single
 * pushes, normal captures, en passant captures. Promotions are considered in serialize_moves (so
 * for under-promotion pruning, modify only serialize_moves) */
{
	const int us = B.get_turn(), them = opp_color(us);
	const int
	lc_inc = us ? -9 : 7,	// left capture increment
	rc_inc = us ? -7 : 9,	// right capture increment
	sp_inc = us ? -8 : 8,	// single push increment
	dp_inc = 2 * sp_inc;	// double push increment
	const Bitboard
	fss = B.get_pieces(us, PAWN),
	enemies = B.get_pieces(them) | B.st().epsq_bb();

	/* First we calculate the to squares (tss) */

	Bitboard tss, tss_sp, tss_dp, tss_lc, tss_rc, fssd;

	// single pushes
	tss_sp = shift_bit(fss, sp_inc) & ~B.st().occ;

	// double pushes
	fssd = fss & PInitialRank[us]				// pawns on their initial rank
	       & ~shift_bit(B.st().occ, -sp_inc)	// can push once
	       & ~shift_bit(B.st().occ, -dp_inc);	// can push twice
	tss_dp = shift_bit(fssd, dp_inc);			// double push fssd

	// captures (including en passant if epsq != NO_SQUARE)
	tss_lc = shift_bit(fss & ~FileA_bb, lc_inc) & enemies;	// right captures
	tss_rc = shift_bit(fss & ~FileH_bb, rc_inc) & enemies;	// right captures

	// total
	tss = (tss_sp | tss_dp | tss_lc | tss_rc) & targets;

	/* Then we loop on the tss and find the possible from square(s) */

	while (tss) {
		const int tsq = pop_lsb(&tss);

		if (test_bit(tss_sp, tsq))		// can we single push to tsq ?
			mlist = make_pawn_moves(B, tsq - sp_inc, tsq, mlist, sub_promotions);
		else if (test_bit(tss_dp, tsq))	// can we double push to tsq ?
			mlist = make_pawn_moves(B, tsq - dp_inc, tsq, mlist, sub_promotions);
		else {	// can we capture tsq ?
			if (test_bit(tss_lc, tsq))	// can we left capture tsq ?
				mlist = make_pawn_moves(B, tsq - lc_inc, tsq, mlist, sub_promotions);
			if (test_bit(tss_rc, tsq))	// can we right capture tsq ?
				mlist = make_pawn_moves(B, tsq - rc_inc, tsq, mlist, sub_promotions);
		}
	}

	return mlist;
}

move_t *gen_evasion(const Board& B, move_t *mlist)
/* Generates evasions, when the board is in check. These are of 2 kinds: the king moves, or a piece
 * covers the check. Note that cover means covering the ]ksq,checker_sq], so it includes capturing
 * the checking piece. */
{
	assert(B.is_check());
	const int us = B.get_turn();
	const int kpos = B.get_king_pos(us);
	const Bitboard checkers = B.st().checkers;
	const int csq = lsb(checkers);	// checker square
	const int cpiece = B.get_piece_on(csq);		// checker piece
	Bitboard tss;

	// normal king escapes
	tss = KAttacks[kpos] & ~B.get_pieces(us) & ~B.st().attacked;

	// The king must also get out of all sliding checkers' firing lines
	Bitboard _checkers = checkers;
	while (_checkers) {
		const int _csq = pop_lsb(&_checkers);
		const int _cpiece = B.get_piece_on(_csq);
		if (is_slider(_cpiece))
			tss &= ~Direction[_csq][kpos];
	}

	// generate King escapes
	mlist = make_piece_moves(B, kpos, tss, mlist);

	if (!several_bits(B.st().checkers)) {
		// piece moves (only if we're not in double check)
		tss = is_slider(cpiece)
		      ? Between[kpos][csq]	// cover the check (inc capture the sliding checker)
		      : checkers;				// capture the checker

		/* if checked by a Pawn and epsq is available, then the check must result from a pawn double
		 * push, and we also need to consider capturing it en passant to solve the check */
		Bitboard ep_tss = cpiece == PAWN ? B.st().epsq_bb() : 0ULL;

		mlist = gen_piece_moves(B, tss, mlist, 0);
		mlist = gen_pawn_moves(B, tss | ep_tss, mlist, true);
	}

	return mlist;
}

move_t *gen_quiet_checks(const Board& B, move_t *mlist)
/* Generates non capturing checks by pieces (not pawns nor the king) */
{
	assert(!B.is_check());
	const int us = B.get_turn(), them = opp_color(us);
	const int ksq = B.get_king_pos(them);
	const Bitboard occ = B.st().occ;
	Bitboard fss, tss;

	// Pawn push checks (single push only)
	if (B.get_pieces(us, PAWN) & NAttacks[ksq] & PawnSpan[them][ksq]) {
		tss = Shield[them][ksq] & AdjacentFiles[file(ksq)] & ~occ;
		if (tss)
			mlist = gen_pawn_moves(B, tss, mlist, false);
	}

	// Piece quiet checks (direct + discovered)
	for (int piece = KNIGHT; piece <= QUEEN; piece++) {
		const Bitboard check_squares = piece_attack(piece, ksq, occ);
		fss = B.get_pieces(us, piece);

		while (fss) {
			const int fsq = pop_lsb(&fss);
			// possible destinations of piece on fsq
			Bitboard attacks = piece_attack(piece, fsq, occ);
			// direct checks
			tss = attacks & check_squares;
			// revealed checks
			if (test_bit(B.st().dcheckers, fsq))
				tss |= attacks & ~Direction[ksq][fsq];
			// exclude captures
			tss &= ~occ;

			mlist = make_piece_moves(B, fsq, tss, mlist);
		}
	}

	return mlist;
}

move_t *gen_moves(const Board& B, move_t *mlist)
/* Generates all moves in the position, using all the other specific move generators. This function
 * is quite fast but not flexible, and only used for debugging (eg. computing perft values) */
{
	if (B.is_check())
		return gen_evasion(B, mlist);
	else {
		// legal castling moves
		mlist = gen_castling(B, mlist);

		const Bitboard targets = ~B.get_pieces(B.get_turn());

		// generate moves
		mlist = gen_piece_moves(B, targets, mlist, true);
		mlist = gen_pawn_moves(B, targets, mlist, true);

		return mlist;
	}
}

uint64_t perft(Board& B, int depth, int ply)
/* Calculates perft(depth), and displays all perft(depth-1) in the initial position. This
 * decomposition is useful to debug an incorrect perft recursively, against a correct perft
 * generator */
{
	move_t mlist[MAX_MOVES];
	move_t *begin = mlist, *m;
	move_t *end = gen_moves(B, mlist);
	uint64_t count;

	if (depth > 1) {
		for (m = begin, count = 0ULL; m < end; m++) {
			uint64_t count_subtree;

			B.play(*m);
			count += count_subtree = perft(B, depth-1, ply+1);
			B.undo();

			if (!ply)
				std::cout << move_to_string(*m) << '\t' << count_subtree << std::endl;
		}
	} else {
		count = end - begin;
		if (!ply)
			for (m = begin; m < end; m++)
				std::cout << move_to_string(*m) << std::endl;
	}

	return count;
}

bool test_perft()
{
	Board B;

	struct TestPerft {
		const char *s;
		int depth;
		uint64_t value;
	};

	TestPerft Test[] = {
		{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", 6, 119060324ULL},
		{"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 5, 193690690ULL},
		{"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 7, 178633661ULL},
		{"r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ -", 6, 706045033ULL},
		{NULL, 0, 0}
	};

	uint64_t total = 0;
	using namespace std::chrono;
	auto start = high_resolution_clock::now();

	for (int i = 0; Test[i].s; i++) {
		std::cout << Test[i].s << std::endl;
		B.set_fen(Test[i].s);
		if (perft(B, Test[i].depth, 0) != Test[i].value) {
			std::cerr << "Incorrect perft" << std::endl;
			return false;
		}
		total += Test[i].value;
	}

	auto stop = high_resolution_clock::now();
	int elapsed = duration_cast<microseconds>(stop - start).count();
	std::cout << "speed: " << (int)(total / (float)elapsed * 1e6) << " leaf/sec" << std::endl;

	return true;
}
