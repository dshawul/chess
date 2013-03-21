/*
 * DiscoCheck, an UCI chess interface. Copyright (C) 2011-2013 Lucas Braesch.
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
 * - Fruit source code, by Fabien Letouzey.
 * - Stockfish source code, by Tord Romstad and Marco Costalba.
*/
#include <chrono>
#include "search.h"
#include "eval.h"
#include "movesort.h"
#include "prng.h"

TTable TT;

using namespace std::chrono;

namespace
{
	struct SearchInfo {
		move_t m, best, killer[2];
		int ply, reduction, eval;
		bool skip_null, null_child;
	};
	const int MAX_PLY = 0x80;
	const int MATE = 32000;
	const int QS_LIMIT = -8;
	const int TEMPO = 4;

	struct AbortSearch {};
	struct ForcedMove {};

	uint64_t node_count, node_limit;
	int time_limit[2], time_allowed;
	time_point<high_resolution_clock> start;
	void node_poll(Board& B);

	void time_alloc(const SearchLimits& sl, int result[2]);

	History H;

	int adjust_tt_score(int score, int ply);
	bool can_return_tt(bool is_pv, const TTable::Entry *tte, int depth, int beta, int ply);

	bool is_mate_score(int score) { return std::abs(score) >= MATE-MAX_PLY; }
	int mated_in(int ply) { return ply-MATE; }
	int mate_in(int ply) { return MATE-ply; }

	int null_reduction(int depth) { return 3 + depth/4; }

	const int RazorMargin[4] = {0, 2*vEP, 2*vEP+vEP/2, 3*vEP};
	const int EvalMargin[4]	 = {0, vEP, vN, vQ};

	Bitboard threats(const Board& B)
	{
		assert(!B.is_check());
		const int us = B.get_turn(), them = opp_color(us);

		const Bitboard our_pawns = B.get_pieces(us, PAWN);
		const Bitboard our_pieces = B.get_pieces(us) & ~our_pawns;

		const Bitboard attacked = B.st().attacks[them][NO_PIECE];
		const Bitboard defended = B.st().attacks[us][NO_PIECE];

		return ((our_pawns ^ our_pieces) & attacked & ~defended)
		       | (our_pieces & B.st().attacks[them][PAWN]);
	}

	void print_pv(Board& B);

	int search(Board& B, int alpha, int beta, int depth, int node_type, SearchInfo *ss);
	int qsearch(Board& B, int alpha, int beta, int depth, int node_type, SearchInfo *ss);
	
	move_t best;
}

move_t bestmove(Board& B, const SearchLimits& sl)
{
	start = high_resolution_clock::now();

	SearchInfo ss[MAX_PLY + 1-QS_LIMIT], *sp = ss;
	for (int ply = 0; ply < MAX_PLY + 1-QS_LIMIT; ++ply, ++sp) {
		sp->ply = ply;
		sp->skip_null = sp->null_child = false;
	}

	node_count = 0;
	node_limit = sl.nodes;
	time_alloc(sl, time_limit);
	best = 0;

	H.clear();
	TT.new_search();
	B.set_unwind();

	const int max_depth = sl.depth ? std::min(MAX_PLY-1, sl.depth) : MAX_PLY-1;

	for (int depth = 1, alpha = -INF, beta = +INF; depth <= max_depth; depth++) {
		// iterative deepening loop

		int score, delta = 16;
		// set time allowance to normal, and divide by two if we're in an "easy" recapture situation
		time_allowed = time_limit[0] >> (best && calc_see(B, best) > 0);

		for (;;) {
			// Aspiration loop

			try {
				score = search(B, alpha, beta, depth, PV, ss);
			} catch (AbortSearch e) {
				return best;
			} catch (ForcedMove e) {
				return best = ss->best;
			}

			std::cout << "info score cp " << score << " depth " << depth << " nodes " << node_count
			          << " time " << duration_cast<milliseconds>(high_resolution_clock::now()-start).count();

			if (alpha < score && score < beta) {
				// score is within bounds
				if (depth >= 4 && !is_mate_score(score)) {
					// set aspiration window for the next depth (so aspiration starts at depth 5)
					alpha = score - delta;
					beta = score + delta;
				}
				// stop the aspiration loop
				break;
			} else {
				// score is outside bounds: resize window and double delta
				if (score <= alpha) {
					alpha -= delta;
					std::cout << " upperbound" << std::endl;
				} else if (score >= beta) {
					beta += delta;
					std::cout << " lowerbound" << std::endl;
				}
				delta *= 2;

				// increase time_allowed, to try to finish the current depth iteration
				time_allowed = time_limit[1];
			}
		}

		print_pv(B);
	}

	return best;
}

namespace
{
	int search(Board& B, int alpha, int beta, int depth, int node_type, SearchInfo *ss)
	{
		assert(alpha < beta && (node_type == PV || alpha+1 == beta));

		if (depth <= 0 || ss->ply >= MAX_PLY)
			return qsearch(B, alpha, beta, depth, node_type, ss);

		const Key key = B.get_key();
		TT.prefetch(key);
		
		node_poll(B);

		const bool root = !ss->ply, in_check = B.is_check();
		int best_score = -INF, old_alpha = alpha;
		ss->best = 0;

		if (!root && B.is_draw())
			return 0;

		// mate distance pruning
		alpha = std::max(alpha, mated_in(ss->ply));
		beta = std::min(beta, mate_in(ss->ply+1));
		if (alpha >= beta) {
			assert(!root);
			return alpha;
		}

		// TT lookup
		const TTable::Entry *tte = TT.probe(key);
		if (tte) {
			if (!root && can_return_tt(node_type == PV, tte, depth, beta, ss->ply)) {
				TT.refresh(tte);
				return adjust_tt_score(tte->score, ss->ply);
			}
			ss->eval = tte->eval;
			ss->best = tte->move;
		} else
			ss->eval = in_check ? -INF : (ss->null_child ? -(ss-1)->eval : eval(B));

		// Eval pruning
		if ( node_type != PV
		        && depth <= 3
		        && !in_check
		        && !is_mate_score(beta)
		        && ss->eval + TEMPO >= beta + EvalMargin[depth]
		        && B.st().piece_psq[B.get_turn()]
		        && !several_bits(threats(B)) )
			return ss->eval + TEMPO - EvalMargin[depth];

		// Razoring
		if ( node_type != PV
		        && depth <= 3
		        && !in_check
		        && !is_mate_score(beta) ) {
			const int threshold = beta - RazorMargin[depth];
			if (ss->eval < threshold) {
				const int score = qsearch(B, threshold-1, threshold, 0, All, ss+1);
				if (score < threshold)
					return score + TEMPO;
			}
		}

		// Null move pruning
		move_t threat_move = 0;
		if ( node_type != PV
		        && !ss->skip_null
		        && !in_check
		        && !is_mate_score(beta)
		        && ss->eval >= beta
		        && B.st().piece_psq[B.get_turn()] ) {
			const int reduction = null_reduction(depth) + (ss->eval - vOP >= beta);

			B.play(0);
			(ss+1)->null_child = true;
			int score = -search(B, -beta, -alpha, depth - reduction, All, ss+1);
			(ss+1)->null_child = false;
			B.undo();

			if (score >= beta)		// null search fails high
				return score < mate_in(MAX_PLY)
				       ? score		// fail soft
				       : beta;		// *but* do not return an unproven mate
			else {
				threat_move = (ss+1)->best;
				if (score <= mated_in(MAX_PLY) && (ss-1)->reduction)
					++depth;
			}
		}

		// Internal Iterative Deepening
		if ( (!tte || !tte->move || tte->depth <= 0)
			&& depth >= (node_type == PV ? 4 : 7)
			&& (node_type != All || ss->eval+vOP >= beta) ) {
			ss->skip_null = true;
			search(B, alpha, beta, node_type == PV ? depth-2 : depth/2, node_type, ss);
			ss->skip_null = false;
		}

		MoveSort MS(&B, MoveSort::ALL, ss->killer, ss->best, node_type, &H);
		int cnt = 0, LMR = 0, see;

		while ( alpha < beta && (ss->m = MS.next(&see)) ) {
			++cnt;
			bool check = move_is_check(B, ss->m);

			// check extension
			int new_depth;
			if (check && (check == DISCO_CHECK || see >= 0) )
				// extend relevant checks
				new_depth = depth;
			else if (MS.get_count() == 1)
				// extend forced replies
				new_depth = depth;
			else
				new_depth = depth-1;

			// move properties
			const bool first = cnt == 1;
			const bool capture = move_is_cop(B, ss->m);
			const int hscore = capture ? 0 : H.get(B, ss->m);
			const bool bad_quiet = !capture && (hscore < 0 || (hscore == 0 && see < 0));
			const bool bad_capture = capture && see < 0;
			// dangerous movea are not reduced
			const bool dangerous = check
				|| new_depth == depth
				|| ss->m == ss->killer[0]
				|| ss->m == ss->killer[1]
				|| (move_is_pawn_threat(B, ss->m) && see >= 0)
				|| (ss->m.flag() == CASTLING);

			if (!capture && !dangerous && !in_check) {
				// Move count pruning
				if ( depth <= 8 && node_type != PV
				        && LMR >= 3 + depth*depth
				        && alpha > mated_in(MAX_PLY)
				        && (see < 0 || !refute(B, ss->m, threat_move)) ) {
					best_score = std::max(best_score, std::min(alpha, ss->eval + see));
					continue;
				}

				// SEE pruning near the leaves
				if (new_depth <= 1 && see < 0) {
					best_score = std::max(best_score, std::min(alpha, ss->eval + see));
					continue;
				}
			}

			// reduction decision
			ss->reduction = !first && (bad_capture || bad_quiet) && !dangerous;
			if (ss->reduction) {
				LMR += !capture;
				ss->reduction += (bad_quiet && LMR >= 3+8/depth);
			}
			// do not LMR into the QS
			if (new_depth - ss->reduction <= 0)
				ss->reduction = 0;

			B.play(ss->m);

			// PVS
			int score;
			if (first)
				// search full window full depth
				// Note that the full window is a zero window at non PV nodes
				// "-node_type" effectively does PV->PV Cut<->All
				score = -search(B, -beta, -alpha, new_depth, -node_type, ss+1);
			else {
				// Cut node: If the first move didn't produce the expected cutoff, then we are
				// unlikely to get a cutoff at this node, which becomes an All node, so that its
				// children are Cut nodes
				if (node_type == Cut)
					node_type = All;

				// zero window search (reduced)
				score = -search(B, -alpha-1, -alpha, new_depth - ss->reduction,
				                node_type == PV ? Cut : -node_type, ss+1);

				// doesn't fail low: verify at full depth, with zero window
				if (score > alpha && ss->reduction)
					score = -search(B, -alpha-1, -alpha, new_depth, All, ss+1);

				// still doesn't fail low at PV node: full depth and full window
				if (node_type == PV && score > alpha)
					score = -search(B, -beta, -alpha, new_depth , PV, ss+1);
			}

			B.undo();

			if (score > best_score) {
				best_score = score;
				alpha = std::max(alpha, score);
				ss->best = ss->m;
				if (root)
					best = ss->m;
			}
		}

		if (!MS.get_count()) {
			// mated or stalemated
			assert(!root);
			return in_check ? mated_in(ss->ply) : 0;
		} else if (root && MS.get_count() == 1)
			// forced move at the root node, play instantly and prevent further iterative deepening
			throw ForcedMove();

		// update TT
		uint8_t bound = best_score <= old_alpha ? BOUND_UPPER : (best_score >= beta ? BOUND_LOWER : BOUND_EXACT);
		TT.store(key, bound, depth, best_score, ss->eval, ss->best);

		// best move is quiet: update killers and history
		if (ss->best && !move_is_cop(B, ss->best)) {
			// update killers on a LIFO basis
			if (ss->killer[0] != ss->best) {
				ss->killer[1] = ss->killer[0];
				ss->killer[0] = ss->best;
			}

			// mark ss->best as good, and all other moves searched as bad
			move_t m;
			while ( (m = MS.previous()) )
				if (!move_is_cop(B, m)) {
					int bonus = m == ss->best ? depth*depth : -depth*depth;
					H.add(B, m, bonus);
				}
		}

		return best_score;
	}

	int qsearch(Board& B, int alpha, int beta, int depth, int node_type, SearchInfo *ss)
	{
		assert(depth <= 0);
		assert(alpha < beta && (node_type == PV || alpha+1 == beta));
		
		const Key key = B.get_key();
		TT.prefetch(key);
		node_poll(B);

		const bool in_check = B.is_check();
		int best_score = -INF, old_alpha = alpha;
		ss->best = 0;

		if (B.is_draw())
			return 0;

		// TT lookup
		const TTable::Entry *tte = TT.probe(key);
		if (tte) {
			if (can_return_tt(node_type == PV, tte, depth, beta, ss->ply)) {
				TT.refresh(tte);
				return adjust_tt_score(tte->score, ss->ply);
			}
			ss->eval = tte->eval;
			ss->best = tte->move;
		} else
			ss->eval = in_check ? -INF : (ss->null_child ? -(ss-1)->eval : eval(B));

		// stand pat
		if (!in_check) {
			best_score = ss->eval + TEMPO;
			alpha = std::max(alpha, best_score);
			if (alpha >= beta)
				return alpha;
		}

		MoveSort MS(&B, depth < 0 ? MoveSort::CAPTURES : MoveSort::CAPTURES_CHECKS, NULL, ss->best, node_type, &H);
		int see;
		const int fut_base = ss->eval + vEP/2;

		while ( alpha < beta && (ss->m = MS.next(&see)) ) {
			int check = move_is_check(B, ss->m);

			// Futility pruning
			if (!check && !in_check && node_type != PV) {
				// opt_score = current eval + some margin + max material gain of the move
				const int opt_score = fut_base
				                      + Material[B.get_piece_on(ss->m.tsq())].eg
				                      + (ss->m.flag() == EN_PASSANT ? vEP : 0)
				                      + (ss->m.flag() == PROMOTION ? Material[ss->m.prom()].eg - vOP : 0);

				// still can't raise alpha, skip
				if (opt_score <= alpha) {
					best_score = std::max(best_score, opt_score);	// beware of fail soft side effect
					continue;
				}

				// the "SEE proxy" tells us we are unlikely to raise alpha, skip if depth < 0
				if (fut_base <= alpha && depth < 0 && see <= 0) {
					best_score = std::max(best_score, fut_base);	// beware of fail soft side effect
					continue;
				}
			}

			// SEE pruning
			if (!in_check && check != DISCO_CHECK && see < 0)
				continue;

			// recursion
			int score;
			if (depth <= QS_LIMIT && !in_check)		// prevent qsearch explosion
				score = ss->eval + see;
			else {
				B.play(ss->m);
				score = -qsearch(B, -beta, -alpha, depth-1, -node_type, ss+1);
				B.undo();
			}

			if (score > best_score) {
				best_score = score;
				alpha = std::max(alpha, score);
				ss->best = ss->m;
			}
		}

		if (B.is_check() && !MS.get_count())
			return mated_in(ss->ply);

		// update TT
		int8_t bound = best_score <= old_alpha ? BOUND_UPPER
		               : (best_score >= beta ? BOUND_LOWER : BOUND_EXACT);
		TT.store(key, bound, depth, best_score, ss->eval, ss->best);

		return best_score;
	}

	void node_poll(Board &B)
	{
		++node_count;
		if (0 == (node_count % 1024)) {
			bool abort = false;

			// abort search because node limit exceeded
			if (node_limit && node_count >= node_limit)
				abort = true;
			// abort search because time limit exceeded
			else if (time_allowed && duration_cast<milliseconds>
			         (high_resolution_clock::now()-start).count() > time_allowed)
				abort = true;

			// abort search by throwing an exception, caught in bestmove()
			// B.unwind() restores the board state from its initial state
			if (abort) {
				B.unwind();
				throw AbortSearch();
			}
		}
	}

	int adjust_tt_score(int score, int ply)
	/* mate scores from the hash_table_t need to be adjusted. For example, if we find a mate in 10 from the
	 * hash_table_t at ply 5, then we effectively have a mate in 15 plies. */
	{
		if (score >= mate_in(MAX_PLY))
			return score - ply;
		else if (score <= mated_in(MAX_PLY))
			return score + ply;
		else
			return score;
	}

	bool can_return_tt(bool is_pv, const TTable::Entry *tte, int depth, int beta, int ply)
	/* PV nodes: return only exact scores
	 * non PV nodes: return fail high/low scores. Mate scores are also trusted, regardless of the
	 * depth. This idea is from StockFish, and although it's not totally sound, it seems to work. */
	{
		const bool depth_ok = tte->depth >= depth;

		if (is_pv)
			return depth_ok && tte->bound() == BOUND_EXACT;
		else {
			const int tt_score = adjust_tt_score(tte->score, ply);
			return (depth_ok
			        || tt_score >= std::max(mate_in(MAX_PLY), beta)
			        || tt_score < std::min(mated_in(MAX_PLY), beta))
			       && ((tte->bound() == BOUND_LOWER && tt_score >= beta)
			           ||(tte->bound() == BOUND_UPPER && tt_score < beta));
		}
	}

	void time_alloc(const SearchLimits& sl, int result[2])
	{
		if (sl.movetime > 0)
			result[0] = result[1] = sl.movetime;
		else if (sl.time > 0 || sl.inc > 0) {
			static const int time_buffer = 100;
			int movestogo = sl.movestogo > 0 ? sl.movestogo : 30;
			result[0] = std::max(std::min(sl.time / movestogo + sl.inc, sl.time-time_buffer), 1);
			result[1] = std::max(std::min(sl.time / (1+movestogo/2) + sl.inc, sl.time-time_buffer), 1);
		}
	}

	void print_pv(Board& B)
	{
		std::cout << " pv";

		for (int i = 0; i < MAX_PLY; i++) {
			const TTable::Entry *tte = TT.probe(B.get_key());

			if (tte && tte->move && !B.is_draw()) {
				std::cout << ' ' << move_to_string(tte->move);
				B.play(tte->move);
			}
		}

		std::cout << std::endl;
		B.unwind();
	}

}

void bench(int depth)
{
	static const char *test[] = {
		"r1bqkbnr/pp1ppppp/2n5/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq -",
		"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",
		"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",
		"4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19",
		"rq3rk1/ppp2ppp/1bnpb3/3N2B1/3NP3/7P/PPPQ1PP1/2KR3R w - - 7 14",
		"r1bq1r1k/1pp1n1pp/1p1p4/4p2Q/4Pp2/1BNP4/PPP2PPP/3R1RK1 w - - 2 14",
		"r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15",
		"r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13",
		"r1bq1rk1/ppp1nppp/4n3/3p3Q/3P4/1BP1B3/PP1N2PP/R4RK1 w - - 1 16",
		"4r1k1/r1q2ppp/ppp2n2/4P3/5Rb1/1N1BQ3/PPP3PP/R5K1 w - - 1 17",
		"2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQ - 0 11",
		"r1bq1r1k/b1p1npp1/p2p3p/1p6/3PP3/1B2NN2/PP3PPP/R2Q1RK1 w - - 1 16",
		"3r1rk1/p5pp/bpp1pp2/8/q1PP1P2/b3P3/P2NQRPP/1R2B1K1 b - - 6 22",
		"r1q2rk1/2p1bppp/2Pp4/p6b/Q1PNp3/4B3/PP1R1PPP/2K4R w - - 2 18",
		"4k2r/1pb2ppp/1p2p3/1R1p4/3P4/2r1PN2/P4PPP/1R4K1 b - - 3 22",
		"3q2k1/pb3p1p/4pbp1/2r5/PpN2N2/1P2P2P/5PP1/Q2R2K1 b - - 4 26",
		"2r5/8/1n6/1P1p1pkp/p2P4/R1P1PKP1/8/1R6 w - - 0 1",
		"r2q1rk1/1b1nbppp/4p3/3pP3/p1pP4/PpP2N1P/1P3PP1/R1BQRNK1 b 0 1",
		"1r1r4/3q2k1/3p1p2/p1pNpPp1/PpP1B1Pp/1P5P/2Q5/6K1 b 0 1",
		"r4rk1/1pp1q1pp/p2p4/3Pn3/1PP1Pp2/P7/3QB1PP/2R2RK1 b 0 1",
		"r2q1rk1/1ppbbpp1/2n2n1p/p3pP2/3pP3/3P3N/PPP1B1PP/RNBQ1RK1 w 0 1",
		NULL
	};

	Board B;
	SearchLimits sl;
	sl.depth = depth;
	uint64_t signature = 0;

	TT.alloc(32 << 20);

	time_point<high_resolution_clock> start, end;
	start = high_resolution_clock::now();

	for (int i = 0; test[i]; ++i) {
		B.set_fen(test[i]);
		std::cout << B.get_fen() << std::endl;
		bestmove(B, sl);
		std::cout << std::endl;
		signature += node_count;
	}

	end = high_resolution_clock::now();
	int64_t elapsed_usec = duration_cast<microseconds>(end-start).count();

	std::cout << "signature = " << signature << std::endl;
	std::cout << "time = " << (float)elapsed_usec / 1e6 << std::endl;
}
