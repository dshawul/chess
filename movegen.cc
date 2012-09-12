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
#include "board.h"

namespace
{
	move_t *make_pawn_moves(const Board& B, Square fsq, Square tsq, move_t *mlist,
	                        bool sub_promotions)
	/* Centralise the pawnm moves generation: given (fsq,tsq) the rest follows. We filter here all the
	 * indirect self checks (through fsq, or through the ep captured square) */
	{
		assert(square_ok(fsq) && square_ok(tsq));
		const Color us = B.get_turn(), them = opp_color(us);
		Square kpos = B.get_king_pos(us);

		if ((test_bit(B.st->pinned, fsq))				// pinned piece
		        && (!test_bit(Direction[kpos][fsq], tsq)))	// moves out of pin ray
			return mlist;	// illegal move by indirect self check

		move_t m;
		m.fsq = fsq;
		m.tsq = tsq;
		m.ep = tsq == B.st->epsq;

		if (m.ep)
		{
			uint64_t occ = B.st->occ;
			// play the ep capture on occ
			clear_bit(&occ, m.fsq);
			clear_bit(&occ, pawn_push(them, m.tsq));	// remove the ep captured enemy pawn
			set_bit(&occ, m.tsq);
			// test for check by a sliding enemy piece
			if ((B.get_RQ(them) & RPseudoAttacks[kpos] & rook_attack(kpos, occ))
			        || (B.get_BQ(them) & BPseudoAttacks[kpos] & bishop_attack(kpos, occ)))
				return mlist;	// illegal move by indirect self check (through the ep captured pawn)
		}

		// promotions
		if (test_bit(PPromotionRank[us], tsq))
		{
			m.promotion = Queen;
			*mlist++ = m;
			if (sub_promotions)
			{
				m.promotion = Knight;
				*mlist++ = m;
				m.promotion = Rook;
				*mlist++ = m;
				m.promotion = Bishop;
				*mlist++ = m;
			}
		}
		// all other moves
		else
		{
			m.promotion = NoPiece;
			*mlist++ = m;
		}

		return mlist;
	}

	move_t *make_piece_move(const Board& B, Square fsq, Square tsq, move_t *mlist)
	/* Centralise the generation of a piece move: given (fsq,tsq) the rest follows. We filter indirect
	 * self checks here. Note that direct self-checks aren't generated, so we don't check them here. In
	 * other words, we never put our King in check before calling this function */
	{
		assert(square_ok(fsq) && square_ok(tsq));
		const unsigned kpos = B.get_king_pos(B.get_turn());

		if ((test_bit(B.st->pinned, fsq))				// pinned piece
		        && (!test_bit(Direction[kpos][fsq], tsq)))	// moves out of pin ray
			return mlist;	// illegal move by indirect self check

		move_t m;
		m.fsq = fsq;
		m.tsq = tsq;
		m.ep = false;
		m.promotion = NoPiece;
		
		*mlist++ = m;
		return mlist;
	}
}

move_t *gen_piece_moves(const Board& B, uint64_t targets, move_t *mlist, bool king_moves)
/* Generates piece moves, when the board is not in check. Uses targets to filter the tss, eg.
 * targets = ~friends (all moves), empty (quiet moves only), enemies (captures only). */
{
	assert(!king_moves || !B.get_checkers());	// do not use when in check (use gen_evasion)
	const Color us = B.get_turn();
	assert(!(targets & B.get_pieces(us)));
	uint64_t fss;

	// Knight Moves
	fss = B.get_pieces(us, Knight);
	while (fss)
	{
		Square fsq = next_bit(&fss);
		uint64_t tss = NAttacks[fsq] & targets;
		while (tss)
		{
			Square tsq = next_bit(&tss);
			mlist = make_piece_move(B, fsq, tsq, mlist);
		}
	}

	// Rook Queen moves
	fss = B.get_RQ(us);
	while (fss)
	{
		Square fsq = next_bit(&fss);
		uint64_t tss = targets & rook_attack(fsq, B.st->occ);
		while (tss)
		{
			Square tsq = next_bit(&tss);
			mlist = make_piece_move(B, fsq, tsq, mlist);
		}
	}

	// Bishop Queen moves
	fss = B.get_BQ(us);
	while (fss)
	{
		Square fsq = next_bit(&fss);
		uint64_t tss = targets & bishop_attack(fsq, B.st->occ);
		while (tss)
		{
			Square tsq = next_bit(&tss);
			mlist = make_piece_move(B, fsq, tsq, mlist);
		}
	}

	// King moves (king_moves == false is only used for check escapes)
	if (king_moves)
	{
		Square fsq = B.get_king_pos(us);
		// here we also filter direct self checks, which shouldn't be sent to serialize_moves
		uint64_t tss = KAttacks[fsq] & targets & ~B.st->attacked;
		while (tss)
		{
			Square tsq = next_bit(&tss);
			mlist = make_piece_move(B, fsq, tsq, mlist);
		}
	}

	return mlist;
}

move_t *gen_castling(const Board& B, move_t *mlist)
/* Generates castling moves, when the board is not in check. The only function that doesn't go
 * through serialize_moves, as castling moves are very peculiar, and we don't want to pollute
 * serialize_moves with this over-specific code */
{
	assert(!B.get_checkers());
	const Color us = B.get_turn();

	move_t m;
	m.fsq = B.get_king_pos(us);
	m.promotion = NoPiece;
	m.ep = 0;

	if (B.st->crights & (OO << (2 * us)))
	{
		uint64_t safe = 3ULL << (m.fsq + 1);	// must not be attacked
		uint64_t empty = safe;						// must be empty

		if (!(B.st->attacked & safe) && !(B.st->occ & empty))
		{
			m.tsq = m.fsq + 2;
			*mlist++ = m;
		}
	}
	if (B.st->crights & (OOO << (2 * us)))
	{
		uint64_t safe = 3ULL << (m.fsq - 2);	// must not be attacked
		uint64_t empty = safe | (1ULL << (m.fsq - 3));		// must be empty

		if (!(B.st->attacked & safe) && !(B.st->occ & empty))
		{
			m.tsq = m.fsq - 2;
			*mlist++ = m;
		}
	}

	return mlist;
}

move_t *gen_pawn_moves(const Board& B, uint64_t targets, move_t *mlist, bool sub_promotions)
/* Generates pawn moves, when the board is not in check. These are of course: double and single
 * pushes, normal captures, en passant captures. Promotions are considered in serialize_moves (so
 * for under-promotion pruning, modify only serialize_moves) */
{
	const Color us = B.get_turn(), them = opp_color(us);
	const int
	lc_inc = us ? -9 : 7,	// left capture increment
	rc_inc = us ? -7 : 9,	// right capture increment
	sp_inc = us ? -8 : 8,	// single push increment
	dp_inc = 2 * sp_inc;	// double push increment
	const uint64_t
	fss = B.get_pieces(us, Pawn),
	enemies = B.get_pieces(them) | B.get_epsq_bb();

	/* First we calculate the to squares (tss) */

	uint64_t tss, tss_sp, tss_dp, tss_lc, tss_rc, fssd;

	// single pushes
	tss_sp = shift_bit(fss, sp_inc) & ~B.st->occ;

	// double pushes
	fssd = fss & PInitialRank[us]				// pawns on their initial rank
	       & ~shift_bit(B.st->occ, -sp_inc)	// can push once
	       & ~shift_bit(B.st->occ, -dp_inc);	// can push twice
	tss_dp = shift_bit(fssd, dp_inc);			// double push fssd

	// captures (including en passant if epsq != NoSquare)
	tss_lc = shift_bit(fss & ~FileA_bb, lc_inc) & enemies;	// right captures
	tss_rc = shift_bit(fss & ~FileH_bb, rc_inc) & enemies;	// right captures

	// total
	tss = (tss_sp | tss_dp | tss_lc | tss_rc) & targets;

	/* Then we loop on the tss and find the possible from square(s) */

	while (tss)
	{
		const Square tsq = next_bit(&tss);

		if (test_bit(tss_sp, tsq))		// can we single push to tsq ?
			mlist = make_pawn_moves(B, tsq - sp_inc, tsq, mlist, sub_promotions);
		else if (test_bit(tss_dp, tsq))	// can we double push to tsq ?
			mlist = make_pawn_moves(B, tsq - dp_inc, tsq, mlist, sub_promotions);
		else  	// can we capture tsq ?
		{
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
	assert(B.get_checkers());
	const Color us = B.get_turn();
	const Square kpos = B.get_king_pos(us);
	const uint64_t checkers = B.st->checkers;
	const Square csq = first_bit(checkers);	// checker square
	const Piece cpiece = B.get_piece_on(csq);		// checker piece
	uint64_t tss;

	// normal king escapes
	tss = KAttacks[kpos] & ~B.get_pieces(us) & ~B.st->attacked;

	// The king must also get out of all sliding checkers' firing lines
	uint64_t _checkers = checkers;
	while (_checkers)
	{
		const Square _csq = next_bit(&_checkers);
		const Piece _cpiece = B.get_piece_on(_csq);
		if (is_slider(_cpiece))
			tss &= ~Direction[_csq][kpos];
	}

	// generate King escapes
	while (tss)
	{
		Square tsq = next_bit(&tss);
		mlist = make_piece_move(B, kpos, tsq, mlist);
	}

	if (!several_bits(B.get_checkers()))
	{
		// piece moves (only if we're not in double check)
		tss = is_slider(cpiece)
		      ? Between[kpos][csq]	// cover the check (inc capture the sliding checker)
		      : checkers;				// capture the checker

		/* if checked by a Pawn and epsq is available, then the check must result from a pawn double
		 * push, and we also need to consider capturing it en passant to solve the check */
		uint64_t ep_tss = cpiece == Pawn ? B.get_epsq_bb() : 0ULL;

		mlist = gen_piece_moves(B, tss, mlist, 0);
		mlist = gen_pawn_moves(B, tss | ep_tss, mlist, true);
	}

	return mlist;
}

move_t *gen_moves(const Board& B, move_t *mlist)
/* Generates all moves in the position, using all the other specific move generators. This function
 * is quite fast but not flexible, and only used for debugging (eg. computing perft values) */
{
	if (B.get_checkers())
		return gen_evasion(B, mlist);
	else
	{
		// legal castling moves
		mlist = gen_castling(B, mlist);

		const uint64_t targets = ~B.get_pieces(B.get_turn());

		// generate moves
		mlist = gen_piece_moves(B, targets, mlist, 1);
		mlist = gen_pawn_moves(B, targets, mlist, true);

		return mlist;
	}
}

bool has_piece_moves(const Board& B, uint64_t targets)
{
	assert(!B.get_checkers());			// do not use when in check (use gen_evasion)
	const Color us = B.get_turn();
	const Square kpos = B.get_king_pos(us);
	assert(!(targets & B.get_pieces(us)));	// do not overwrite our pieces
	uint64_t fss;

	// Knight Moves
	fss = B.get_pieces(us, Knight);
	while (fss)
	{
		Square fsq = next_bit(&fss);
		uint64_t tss = NAttacks[fsq] & targets;
		if (test_bit(B.st->pinned, fsq))	// pinned piece
			tss &= Direction[kpos][fsq];	// can only move on the pin-ray
		if (tss)
			return true;
	}

	// King moves
	Square fsq = B.get_king_pos(us);
	uint64_t tss = KAttacks[fsq] & targets & ~B.st->attacked;
	if (tss)
		return true;

	// Rook Queen moves
	fss = B.get_RQ(us);
	while (fss)
	{
		Square fsq = next_bit(&fss);
		uint64_t tss = targets & rook_attack(fsq, B.st->occ);
		if (test_bit(B.st->pinned, fsq))	// pinned piece
			tss &= Direction[kpos][fsq];	// can only move on the pin-ray
		if (tss)
			return true;
	}

	// Bishop Queen moves
	fss = B.get_BQ(us);
	while (fss)
	{
		Square fsq = next_bit(&fss);
		uint64_t tss = targets & bishop_attack(fsq, B.st->occ);
		if (test_bit(B.st->pinned, fsq))	// pinned piece
			tss &= Direction[kpos][fsq];	// can only move on the pin-ray
		if (tss)
			return true;
	}

	return false;
}

bool has_moves(const Board& B)
{
	move_t mlist[MAX_MOVES];

	if (B.get_checkers())
		return gen_evasion(B, mlist) != mlist;
	else
	{
		const uint64_t targets = ~B.get_pieces(B.get_turn());
		if (has_piece_moves(B, targets))
			return true;
		if (gen_pawn_moves(B, targets, mlist, true) != mlist)
			return true;
	}

	return false;
}

uint64_t perft(Board& B, unsigned depth, unsigned ply)
/* Calculates perft(depth), and displays all perft(depth-1) in the initial position. This
 * decomposition is useful to debug an incorrect perft recursively, against a correct perft
 * generator */
{
	move_t mlist[MAX_MOVES];
	move_t *begin = mlist, *m;
	move_t *end = gen_moves(B, mlist);
	uint64_t count;
	char s[8];

	if (has_moves(B) != (end > begin))
	{
		print(B);
		exit(EXIT_FAILURE);
	}

	if (depth > 1)
	{
		for (m = begin, count = 0ULL; m < end; m++)
		{
			uint64_t count_subtree;

			B.play(*m);
			count += count_subtree = perft(B, depth-1, ply+1);
			B.undo();

			if (!ply)
			{
				move_to_string(B, *m, s);
				printf("%s\t%u\n", s, (uint32_t)count_subtree);
			}
		}
	}
	else
	{
		count = end - begin;
		if (!ply)
		{
			for (m = begin; m < end; m++)
			{
				move_to_string(B, *m, s);
				printf("%s\n", s);
			}
		}
	}

	return count;
}
