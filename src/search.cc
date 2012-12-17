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
#include "search.h"
#include "eval.h"
#include "movesort.h"
#include "prng.h"

TTable TT;

using namespace std::chrono;

namespace
{
	struct SearchInfo
	{
		move_t m, best, killer[2];
		int ply;
	};
	const int MAX_PLY = 0x80;
	const int MATE = 32000;
	const int QS_LIMIT = -8;

	struct AbortSearch {};
	struct ForcedMove {};

	uint64_t node_count, node_limit;
	int time_limit;
	time_point<high_resolution_clock> start;
	void node_poll(Board& B);

	int time_alloc(const SearchLimits& sl);

	History H;

	int adjust_tt_score(int score, int ply);
	bool can_return_tt(bool is_pv, const TTable::Entry *tte, int depth, int beta, int ply);

	bool is_mate_score(int score) { return std::abs(score) >= MATE-MAX_PLY; }
	int mated_in(int ply) { return ply-MATE; }
	int mate_in(int ply) { return MATE-ply; }

	int null_reduction(int depth) { return 3 + depth/4; }

	const int IIDDepth[2] = {7, 4};
	const int IIDMargin = vOP;

	int razor_margin(int depth)
	{
		assert(1 <= depth && depth <= 3);
		return 2*vEP + (depth-1)*(vEP/4);
	}

	int eval_margin(int depth)
	{
		assert(1 <= depth && depth <= 3);
		return depth * vEP;
	}

	Bitboard threats(const Board& B)
	{
		assert(!B.is_check());
		const int us = B.get_turn(), them = opp_color(us);

		return (B.st().attacks[them][PAWN] & B.get_pieces(us) & ~B.get_pieces(us, PAWN))
		       | (B.get_RQ(us) & B.st().attacks[them][KNIGHT]);
	}
	
	void print_pv(Board& B);

	int search(Board& B, int alpha, int beta, int depth, bool is_pv, SearchInfo *ss);
	int qsearch(Board& B, int alpha, int beta, int depth, bool is_pv, SearchInfo *ss);
}

move_t bestmove(Board& B, const SearchLimits& sl)
{
	start = high_resolution_clock::now();

	SearchInfo ss[MAX_PLY + 1-QS_LIMIT];
	for (int ply = 0; ply < MAX_PLY + 1-QS_LIMIT; ++ply)
		ss[ply].ply = ply;

	node_count = 0;
	node_limit = sl.nodes;
	time_limit = time_alloc(sl);
	move_t best = 0;

	H.clear();
	TT.new_search();
	B.set_unwind();

	const int max_depth = sl.depth ? std::min(MAX_PLY-1, sl.depth) : MAX_PLY-1;
	for (int depth = 1, alpha = -INF, beta = +INF; depth <= max_depth; depth++)
	{
		int score, delta = 16;

		// Aspiration loop
		for (;;)
		{
			try
			{
				score = search(B, alpha, beta, depth, true, ss);
			}
			catch (AbortSearch e)
			{
				return best;
			}
			catch (ForcedMove e)
			{
				return best = ss->best;
			}

			std::cout << "info score cp " << score << " depth " << depth << " nodes " << node_count
				<< " time " << duration_cast<milliseconds>(high_resolution_clock::now()-start).count();

			if (alpha < score && score < beta)
			{
				// score is within bounds
				if (depth >= 4 && !is_mate_score(score))
				{
					// set aspiration window for the next depth (so aspiration starts at depth 5)
					alpha = score - delta;
					beta = score + delta;
				}
				// stop the aspiration loop
				break;
			}
			else
			{
				// score is outside bounds: resize window and double delta
				if (score <= alpha)
				{
					alpha -= delta;
					std::cout << " upperbound" << std::endl;
				}
				else if (score >= beta)
				{
					beta += delta;
					std::cout << " lowerbound" << std::endl;
				}
				delta *= 2;
			}
		}

		best = ss->best;
		print_pv(B);
	}

	return best;
}

namespace
{
	int search(Board& B, int alpha, int beta, int depth, bool is_pv, SearchInfo *ss)
	{
		assert(alpha < beta && (is_pv || alpha+1 == beta));

		if (depth <= 0 || ss->ply >= MAX_PLY)
			return qsearch(B, alpha, beta, depth, is_pv, ss);

		assert(depth > 0);
		node_poll(B);

		const bool root = !ss->ply, in_check = B.is_check();
		int best_score = -INF, old_alpha = alpha;
		ss->best = 0;

		if (!root && B.is_draw())
			return 0;

		// mate distance pruning
		alpha = std::max(alpha, mated_in(ss->ply));
		beta = std::min(beta, mate_in(ss->ply+1));
		if (alpha >= beta)
		{
			assert(!root);
			return alpha;
		}

		// Eval cache
		const Key key = B.get_key();
		int current_eval = in_check ? -INF : eval(B);

		// TT lookup
		const TTable::Entry *tte = TT.probe(key);
		if (tte)
		{
			if (!root && can_return_tt(is_pv, tte, depth, beta, ss->ply))
			{
				TT.refresh(tte);
				return adjust_tt_score(tte->score, ss->ply);
			}
			if (tte->depth > 0)		// do not use qsearch results
				ss->best = tte->move;
		}

		// Razoring
		if (depth <= 3 && !is_pv
		        && !in_check && !is_mate_score(beta))
		{
			const int threshold = beta - razor_margin(depth);
			if (current_eval < threshold)
			{
				const int score = qsearch(B, threshold-1, threshold, 0, is_pv, ss+1);
				if (score < threshold)
					return score;
			}
		}

		// Eval pruning
		if ( depth <= 3 && !is_pv
		        && !in_check && !is_mate_score(beta)
		        && current_eval - eval_margin(depth) >= beta
		        && B.st().piece_psq[B.get_turn()]
		        && !several_bits(threats(B)) )
			return current_eval - eval_margin(depth);

		// Null move pruning
		if (!is_pv && !in_check && !is_mate_score(beta)
		        && current_eval >= beta
		        && B.st().piece_psq[B.get_turn()])
		{
			const int reduction = null_reduction(depth) + (current_eval - vOP >= beta);

			B.play(0);
			int score = -search(B, -beta, -alpha, depth - reduction, false, ss+1);
			B.undo();

			if (score >= beta)		// null search fails high
				return score < mate_in(MAX_PLY)
				       ? score		// fail soft
				       : beta;		// *but* do not return an unproven mate
		}

		// Internal Iterative Deepening
		if ( depth >= IIDDepth[is_pv] && !ss->best
		        && (is_pv || (!in_check && current_eval + IIDMargin >= beta)) )
			search(B, alpha, beta, is_pv ? depth-2 : depth/2, is_pv, ss);

		MoveSort MS(&B, MoveSort::ALL, ss->killer, ss->best, &H);
		int cnt = 0, LMR = 0, see;

		while ( alpha < beta && (ss->m = MS.next(&see)) )
		{
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

			// SEE pruning near the leaves
			if (new_depth <= 1 && see < 0 && !capture && !dangerous && !in_check)
			{
				best_score = std::max(best_score, std::min(alpha, current_eval + see));
				continue;
			}

			// reduction decision
			int reduction = !first && (bad_capture || bad_quiet) && !dangerous;
			if (reduction)
			{
				LMR += !capture;
				reduction += (bad_quiet && LMR >= 3+8/depth);
			}

			B.play(ss->m);

			// PVS
			int score;
			if (is_pv && first)
				// search full window
				score = -search(B, -beta, -alpha, new_depth, is_pv, ss+1);
			else
			{
				// zero window search (reduced)
				score = -search(B, -alpha-1, -alpha, new_depth-reduction, false, ss+1);

				// doesn't fail low: re-search full window (reduced)
				if (is_pv && score > alpha)
					score = -search(B, -beta, -alpha, new_depth-reduction, is_pv, ss+1);

				// fails high: verify the beta cutoff with a non reduced search
				if (score > alpha && reduction)
					score = -search(B, -beta, -alpha, new_depth, is_pv, ss+1);
			}

			B.undo();

			if (score > best_score)
			{
				best_score = score;
				alpha = std::max(alpha, score);
				ss->best = ss->m;
			}
		}

		if (!MS.get_count())
		{
			// mated or stalemated
			assert(!root);
			return in_check ? mated_in(ss->ply) : 0;
		}
		else if (root && MS.get_count() == 1)
			// forced move at the root node, play instantly and prevent further iterative deepening
			throw ForcedMove();

		// update TT
		uint8_t bound = best_score <= old_alpha ? BOUND_UPPER : (best_score >= beta ? BOUND_LOWER : BOUND_EXACT);
		TT.store(key, bound, depth, best_score, ss->best);

		// best move is quiet: update killers and history
		if (ss->best && !move_is_cop(B, ss->best))
		{
			// update killers on a LIFO basis
			if (ss->killer[0] != ss->best)
			{
				ss->killer[1] = ss->killer[0];
				ss->killer[0] = ss->best;
			}

			// mark ss->best as good, and all other moves searched as bad
			move_t m;
			while ( (m = MS.previous()) )
				if (!move_is_cop(B, m))
				{
					int bonus = m == ss->best ? depth*depth : -depth*depth;
					H.add(B, m, bonus);
				}
		}

		return best_score;
	}

	int qsearch(Board& B, int alpha, int beta, int depth, bool is_pv, SearchInfo *ss)
	{
		assert(depth <= 0);
		assert(alpha < beta && (is_pv || alpha+1 == beta));
		node_poll(B);

		const bool in_check = B.is_check();
		int best_score = -INF, old_alpha = alpha;
		ss->best = 0;

		if (B.is_draw())
			return 0;

		// Eval cache
		const Key key = B.get_key();
		int current_eval = in_check ? -INF : eval(B);

		// TT lookup
		const TTable::Entry *tte = TT.probe(key);
		if (tte)
		{
			if (can_return_tt(is_pv, tte, depth, beta, ss->ply))
			{
				TT.refresh(tte);
				return adjust_tt_score(tte->score, ss->ply);
			}
			ss->best = tte->move;
		}

		// stand pat
		if (!in_check)
		{
			best_score = current_eval;
			alpha = std::max(alpha, best_score);
			if (alpha >= beta)
				return alpha;
		}

		MoveSort MS(&B, depth < 0 ? MoveSort::CAPTURES : MoveSort::CAPTURES_CHECKS, NULL, ss->best, &H);
		int see;
		const int fut_base = current_eval + vEP/2;

		while ( alpha < beta && (ss->m = MS.next(&see)) )
		{
			int check = move_is_check(B, ss->m);

			// Futility pruning
			if (!check && !in_check && !is_pv)
			{
				// opt_score = current eval + some margin + max material gain of the move
				const int opt_score = fut_base
				                      + Material[B.get_piece_on(ss->m.tsq())].eg
				                      + (ss->m.flag() == EN_PASSANT ? vEP : 0)
				                      + (ss->m.flag() == PROMOTION ? Material[ss->m.prom()].eg - vOP : 0);

				// still can't raise alpha, skip
				if (opt_score <= alpha)
				{
					best_score = std::max(best_score, opt_score);	// beware of fail soft side effect
					continue;
				}

				// the "SEE proxy" tells us we are unlikely to raise alpha, skip if depth < 0
				if (fut_base <= alpha && depth < 0 && see <= 0)
				{
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
				score = current_eval + see;
			else
			{
				B.play(ss->m);
				score = -qsearch(B, -beta, -alpha, depth-1, is_pv, ss+1);
				B.undo();
			}

			if (score > best_score)
			{
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
		TT.store(key, bound, depth, best_score, ss->best);

		return best_score;
	}

	void node_poll(Board &B)
	{
		++node_count;
		if (0 == (node_count % 1024))
		{
			if (node_limit && node_count >= node_limit)
			{
				B.unwind();
				throw AbortSearch();
			}
			if (time_limit && duration_cast<milliseconds>(high_resolution_clock::now()-start).count() > time_limit)
			{
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
			return depth_ok && tte->bound == BOUND_EXACT;
		else
		{
			const int tt_score = adjust_tt_score(tte->score, ply);
			return (depth_ok
			        ||	tt_score >= std::max(mate_in(MAX_PLY), beta)
			        ||	tt_score < std::min(mated_in(MAX_PLY), beta))
			       &&	((tte->bound == BOUND_LOWER && tt_score >= beta)
			            ||(tte->bound == BOUND_UPPER && tt_score < beta));
		}
	}

	int time_alloc(const SearchLimits& sl)
	{
		if (sl.movetime > 0)
			return sl.movetime;
		else if (sl.time > 0 || sl.inc > 0)
		{
			static const int time_buffer = 100;
			int movestogo = sl.movestogo > 0 ? sl.movestogo : 30;
			return std::max(std::min(sl.time / movestogo + sl.inc, sl.time-time_buffer), 1);
		}
		else
			return 0;
	}

	void print_pv(Board& B)
	{
		std::cout << " pv";

		for (int i = 0; i < MAX_PLY; i++)
		{
			const TTable::Entry *tte = TT.probe(B.get_key());

			if (tte && tte->bound == BOUND_EXACT && tte->move && !B.is_draw())
			{
				std::cout << ' ' << move_to_string(tte->move);
				B.play(tte->move);
			}
		}
		
		std::cout << std::endl;		
		B.unwind();		
	}

}

void bench()
{
	struct TestPosition
	{
		const char *fen;
		int depth;
	};

	static const TestPosition test[] =
	{
		{"r1bqkbnr/pp1ppppp/2n5/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq -", 12},
		{"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 12},
		{"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 12},
		{"4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19", 12},
		{"rq3rk1/ppp2ppp/1bnpb3/3N2B1/3NP3/7P/PPPQ1PP1/2KR3R w - - 7 14", 12},
		{"r1bq1r1k/1pp1n1pp/1p1p4/4p2Q/4Pp2/1BNP4/PPP2PPP/3R1RK1 w - - 2 14", 12},
		{"r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15", 12},
		{"r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13", 12},
		{"r1bq1rk1/ppp1nppp/4n3/3p3Q/3P4/1BP1B3/PP1N2PP/R4RK1 w - - 1 16", 12},
		{"4r1k1/r1q2ppp/ppp2n2/4P3/5Rb1/1N1BQ3/PPP3PP/R5K1 w - - 1 17", 12},
		{"2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQ - 0 11", 12},
		{"r1bq1r1k/b1p1npp1/p2p3p/1p6/3PP3/1B2NN2/PP3PPP/R2Q1RK1 w - - 1 16", 12},
		{"3r1rk1/p5pp/bpp1pp2/8/q1PP1P2/b3P3/P2NQRPP/1R2B1K1 b - - 6 22", 12},
		{"r1q2rk1/2p1bppp/2Pp4/p6b/Q1PNp3/4B3/PP1R1PPP/2K4R w - - 2 18", 12},
		{"4k2r/1pb2ppp/1p2p3/1R1p4/3P4/2r1PN2/P4PPP/1R4K1 b - - 3 22", 12},
		{"3q2k1/pb3p1p/4pbp1/2r5/PpN2N2/1P2P2P/5PP1/Q2R2K1 b - - 4 26", 12},
		{NULL, 0}
	};

	Board B;
	SearchLimits sl;
	uint64_t signature = 0;

	TT.alloc(32 << 20);

	time_point<high_resolution_clock> start, end;
	start = high_resolution_clock::now();

	for (int i = 0; test[i].fen; ++i)
	{
		B.set_fen(test[i].fen);
		std::cout << B.get_fen() << std::endl;
		sl.depth = test[i].depth;
		bestmove(B, sl);
		std::cout << std::endl;
		signature += node_count;
	}

	end = high_resolution_clock::now();
	int64_t elapsed_usec = duration_cast<microseconds>(end-start).count();

	std::cout << "signature = " << signature << std::endl;
	std::cout << "time = " << (float)elapsed_usec / 1e6 << std::endl;
}
