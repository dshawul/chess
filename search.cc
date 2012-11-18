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

namespace
{
	struct SearchInfo {
		move_t best, killer[2];
		int ply;
	};
	const int MAX_PLY = 0x80;
	const int MATE = 32000;
	const int QS_LIMIT = -8;

	bool abort_search;
	uint64_t node_count, node_limit;
	bool node_poll();

	History H;

	int adjust_tt_score(int score, int ply);
	bool can_return_tt(bool is_pv, const TTable::Entry *tte, int depth, int beta, int ply);

	bool is_mate_score(int score);

	int null_reduction(int depth) { return 3 + depth/4; }

	int search(Board& B, int alpha, int beta, int depth, bool is_pv, SearchInfo *ss);
	int qsearch(Board& B, int alpha, int beta, int depth, bool is_pv, SearchInfo *ss);
}

move_t bestmove(Board& B, const SearchLimits& sl)
{
	SearchInfo ss[MAX_PLY];
	for (int ply = 0; ply < MAX_PLY; ++ply)
		ss[ply].ply = ply;

	node_count = 0;
	node_limit = sl.nodes;
	abort_search = false;
	move_t best;
	H.clear();

	for (int depth = 1; !abort_search; depth++) {
		if ( (sl.depth && depth > sl.depth) || depth >= MAX_PLY )
			break;

		int score = search(B, -INF, +INF, depth, true, ss);

		if (!abort_search) {
			best = ss->best;

			std::cout << "info score cp " << score
			          << " depth " << depth
			          << " nodes " << node_count
			          << " pv " << move_to_string(ss->best)
			          << std::endl;
		}
	}

	return best;
}

namespace
{
	int mated_in(int ply)	{ return ply-MATE; }
	int mate_in(int ply)	{ return MATE-ply; }

	int search(Board& B, int alpha, int beta, int depth, bool is_pv, SearchInfo *ss)
	{
		assert(alpha < beta);

		if ((ss->ply > 0 && B.is_draw()) || ss->ply >= MAX_PLY-2)
			return 0;

		if (depth <= 0)
			return qsearch(B, alpha, beta, depth, is_pv, ss);

		assert(depth > 0);

		if ( (abort_search = node_poll()) )
			return 0;

		const bool in_check = B.is_check();
		int best_score = -INF, old_alpha = alpha;
		ss->best = 0;

		// mate distance pruning
		alpha = std::max(alpha, mated_in(ss->ply));
		beta = std::min(beta, mate_in(ss->ply+1));
		if (alpha >= beta) {
			assert(ss->ply > 0);
			return alpha;
		}

		// TT lookup
		int current_eval;
		move_t tt_move;
		Key key = B.get_key();
		const TTable::Entry *tte = TT.find(key);
		if (tte) {
			if (ss->ply > 0 && can_return_tt(is_pv, tte, depth, beta, ss->ply))
				return adjust_tt_score(tte->score, ss->ply);
			if (tte->depth > 0)		// do not use qsearch results
				tt_move = tte->move;
			current_eval = tte->eval;
		} else
			current_eval = in_check ? -INF : eval(B);

		// Null move pruning
		if (!is_pv && !in_check && !is_mate_score(beta)
		        && (current_eval >= beta || depth <= null_reduction(depth))
		        && B.st().piece_psq[B.get_turn()]) {
			int reduction = null_reduction(depth) + (current_eval - vOP >= beta);

			B.play(0);
			int score = -search(B, -beta, -alpha, depth - reduction, false, ss+1);
			B.undo();

			if (score >= beta)		// null search fails high
				return score < mate_in(MAX_PLY)
				       ? score		// fail soft
				       : beta;		// *but* do not return an unproven mate
		}

		MoveSort MS(&B, MoveSort::ALL, ss->killer, tt_move);
		move_t *m;
		int cnt = 0;

		while ( alpha < beta && (m = MS.next()) && !abort_search ) {
			++cnt;
			bool check = move_is_check(B, *m);

			// check extension
			int new_depth;
			if (check && (check == DISCO_CHECK || see(B, *m) >= 0) )
				new_depth = depth;
			else
				new_depth = depth-1;

			// recursion
			B.play(*m);

			int score;
			if (is_pv && cnt == 1)
				// search full window
				score = -search(B, -beta, -alpha, new_depth, is_pv, ss+1);
			else {
				// try zero window (which *is* the full window for non PV nodes)
				score = -search(B, -alpha-1, -alpha, new_depth, false, ss+1);

				// doesn't fail low at PV node: research full window
				if (is_pv && score > alpha)
					score = -search(B, -beta, -alpha, new_depth, is_pv, ss+1);
			}

			B.undo();

			if (score > best_score) {
				best_score = score;
				alpha = std::max(alpha, score);
				ss->best = *m;
			}
		}

		// mated or stalemated
		if (!MS.get_count()) {
			assert(ss->ply > 0);
			return in_check ? mated_in(ss->ply) : 0;
		}

		// update TT
		TTable::Entry e;
		e.depth = depth;
		e.eval = current_eval;
		e.key = key;
		e.move = ss->best;
		e.score = best_score;
		e.type = best_score <= old_alpha ? SCORE_UBOUND : (best_score >= beta ? SCORE_LBOUND : SCORE_EXACT);
		TT.write(&e);

		// best move is quiet: update killers and history
		if (ss->best && !move_is_cop(B, ss->best)) {
			// update killers on a LIFO basis
			if (ss->killer[0] != ss->best) {
				ss->killer[1] = ss->killer[0];
				ss->killer[0] = ss->best;
			}
			
			H.add(B, ss->best, depth * depth);
		}

		return best_score;
	}

	int qsearch(Board& B, int alpha, int beta, int depth, bool is_pv, SearchInfo *ss)
	{
		assert(depth <= 0);
		assert(alpha < beta);

		if ( (abort_search = node_poll()) )
			return 0;

		bool in_check = B.is_check();
		int best_score = -INF, old_alpha = alpha;

		// TT lookup
		int current_eval;
		move_t tt_move;
		Key key = B.get_key();
		const TTable::Entry *tte = TT.find(key);
		if (tte) {
			if (can_return_tt(is_pv, tte, depth, beta, ss->ply))
				return adjust_tt_score(tte->score, ss->ply);
			tt_move = tte->move;
			current_eval = tte->eval;
		} else
			current_eval = in_check ? -INF : eval(B);

		// stand pat
		if (!in_check) {
			best_score = current_eval = eval(B);
			ss->best = NO_MOVE;
			alpha = std::max(alpha, best_score);
			if (alpha >= beta) {
				return alpha;
			}
		}

		MoveSort MS(&B, depth < 0 ? MoveSort::CAPTURES : MoveSort::CAPTURES_CHECKS, NULL, tt_move);
		move_t *m;

		while ( alpha < beta && (m = MS.next()) && !abort_search ) {
			int check = move_is_check(B, *m);

			// SEE pruning
			if (!in_check && check != DISCO_CHECK && see(B, *m) < 0)
				continue;

			// recursion
			int score;
			if (depth <= QS_LIMIT && !in_check)		// prevent qsearch explosion
				score = current_eval + see(B,*m);
			else {
				B.play(*m);
				score = -qsearch(B, -beta, -alpha, depth-1, is_pv, ss+1);
				B.undo();
			}

			if (score > best_score) {
				best_score = score;
				alpha = std::max(alpha, score);
				ss->best = *m;
			}
		}

		if (B.is_check() && !MS.get_count())
			return mated_in(ss->ply);

		// update TT
		TTable::Entry e;
		e.depth = depth;
		e.eval = current_eval;
		e.key = key;
		e.move = ss->best;
		e.score = best_score;
		e.type = best_score <= old_alpha ? SCORE_UBOUND
		         : (best_score >= beta ? SCORE_LBOUND : SCORE_EXACT);
		TT.write(&e);

		return best_score;
	}

	bool node_poll()
	{
		++node_count;
		return node_limit && node_count >= node_limit;
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
			return depth_ok && tte->type == SCORE_EXACT;
		else {
			const int tt_score = adjust_tt_score(tte->score, ply);
			return (depth_ok
			        ||	tt_score >= std::max(mate_in(MAX_PLY), beta)
			        ||	tt_score < std::min(mated_in(MAX_PLY), beta))
			       &&	((tte->type == SCORE_LBOUND && tt_score >= beta)
			            ||(tte->type == SCORE_UBOUND && tt_score < beta));
		}
	}

	bool is_mate_score(int score)
	{
		assert(std::abs(score) <= INF);
		return score <= mated_in(MAX_PLY) || score >= mate_in(MAX_PLY);
	}
}

void bench()
{
	struct TestPosition {
		const char *fen;
		int depth;
	};

	static const TestPosition test[] = {
		{"r1bqkbnr/pp1ppppp/2n5/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq -", 8},
		{"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 8},
		{"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 8},
		{"4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19", 8},
		{"rq3rk1/ppp2ppp/1bnpb3/3N2B1/3NP3/7P/PPPQ1PP1/2KR3R w - - 7 14", 8},
		{"r1bq1r1k/1pp1n1pp/1p1p4/4p2Q/4Pp2/1BNP4/PPP2PPP/3R1RK1 w - - 2 14", 8},
		{"r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15", 8},
		{"r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13", 8},
		{"r1bq1rk1/ppp1nppp/4n3/3p3Q/3P4/1BP1B3/PP1N2PP/R4RK1 w - - 1 16", 8},
		{"4r1k1/r1q2ppp/ppp2n2/4P3/5Rb1/1N1BQ3/PPP3PP/R5K1 w - - 1 17", 8},
		{"2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQ - 0 11", 8},
		{"r1bq1r1k/b1p1npp1/p2p3p/1p6/3PP3/1B2NN2/PP3PPP/R2Q1RK1 w - - 1 16", 8},
		{"3r1rk1/p5pp/bpp1pp2/8/q1PP1P2/b3P3/P2NQRPP/1R2B1K1 b - - 6 22", 8},
		{"r1q2rk1/2p1bppp/2Pp4/p6b/Q1PNp3/4B3/PP1R1PPP/2K4R w - - 2 18", 8},
		{"4k2r/1pb2ppp/1p2p3/1R1p4/3P4/2r1PN2/P4PPP/1R4K1 b - - 3 22", 8},
		{"3q2k1/pb3p1p/4pbp1/2r5/PpN2N2/1P2P2P/5PP1/Q2R2K1 b - - 4 26", 8},
		{NULL, 0}
	};

	Board B;
	SearchLimits sl;
	uint64_t signature = 0;

	TT.alloc(32 << 20);

	using namespace std::chrono;
	time_point<high_resolution_clock> start, end;
	start = high_resolution_clock::now();

	for (int i = 0; test[i].fen; ++i) {
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
