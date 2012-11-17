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
#include "search.h"
#include "eval.h"
#include "movesort.h"
#include "prng.h"

namespace
{
	struct SearchInfo {
		move_t best, killer[2];
		int ply;
	};
	const int MAX_PLY = 0x80;
	const int MATE = 32000;

	bool abort_search;
	uint64_t node_count, node_limit;
	bool node_poll();

	int search(Board& B, int alpha, int beta, int depth, SearchInfo *ss);
	int qsearch(Board& B, int alpha, int beta, int depth, SearchInfo *ss);
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

	for (int depth = 1; !abort_search && (!sl.depth || depth <= sl.depth); depth++) {
		int score = search(B, -INF, +INF, depth, ss);

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
	const int QS_LIMIT = -6;

	int mated_in(int ply)	{ return ply-MATE; }
	int mate_in(int ply)	{ return MATE-ply; }

	int search(Board& B, int alpha, int beta, int depth, SearchInfo *ss)
	{
		assert(alpha < beta);

		if ((ss->ply > 0 && B.is_draw()) || ss->ply >= MAX_PLY-2)
			return 0;
		
		if (depth <= 0)
			return qsearch(B, alpha, beta, depth, ss);

		assert(depth > 0);

		if ( (abort_search = node_poll()) )
			return 0;

		int best_score = -INF;

		// mate distance pruning
		alpha = std::max(alpha, mated_in(ss->ply));
		beta = std::min(beta, mate_in(ss->ply+1));
		if (alpha >= beta) {
			assert(ss->ply > 0);
			return alpha;
		}

		MoveSort MS(&B, MoveSort::ALL, ss->killer);
		move_t *m;

		while ( alpha < beta && (m = MS.next()) && !abort_search ) {
			bool check = move_is_check(B, *m);

			// check extension
			int new_depth;
			if (check && (check == DISCO_CHECK || see(B, *m) >= 0) )
				new_depth = depth;
			else
				new_depth = depth-1;

			// recursion
			B.play(*m);
			int score = -search(B, -beta, -alpha, new_depth, ss+1);
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
			return B.is_check() ? mated_in(ss->ply) : 0;
		}
		
		// update killers
		if (!move_is_cop(B, ss->best)) {
			if (ss->killer[0] != ss->best) {
				ss->killer[1] = ss->killer[0];
				ss->killer[0] = ss->best;
			}			
		}

		return best_score;
	}

	int qsearch(Board& B, int alpha, int beta, int depth, SearchInfo *ss)
	{
		assert(depth <= 0);
		assert(alpha < beta);

		if ( (abort_search = node_poll()) )
			return 0;

		bool in_check = B.is_check();
		int best_score = -INF, static_eval = -INF;

		// stand pat
		if (!in_check) {
			best_score = static_eval = eval(B);
			ss->best = NO_MOVE;
			alpha = std::max(alpha, best_score);
			if (alpha >= beta) {
				return alpha;
			}
		}

		MoveSort MS(&B, depth < 0 ? MoveSort::CAPTURES : MoveSort::CAPTURES_CHECKS, NULL);
		move_t *m;

		while ( alpha < beta && (m = MS.next()) && !abort_search ) {
			int check = move_is_check(B, *m);

			// SEE pruning
			if (!in_check && check != DISCO_CHECK && see(B, *m) < 0)
				continue;

			// recursion
			int score;
			if (depth <= QS_LIMIT && !in_check)
				score = static_eval + see(B,*m);	// prevent qsearch explosion
			else {
				B.play(*m);
				score = -qsearch(B, -beta, -alpha, depth-1, ss+1);
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

		return best_score;
	}

	bool node_poll()
	{
		++node_count;
		return node_limit && node_count >= node_limit;
	}
}

void bench()
{
	struct TestPosition {
		const char *fen;
		int depth;
	};
	
	static const TestPosition test[] = {
		{"r1bqkbnr/pp1ppppp/2n5/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq -", 6},
		{"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 6},
		{"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 6},
		{"4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19", 6},
		{"rq3rk1/ppp2ppp/1bnpb3/3N2B1/3NP3/7P/PPPQ1PP1/2KR3R w - - 7 14", 6},
		{"r1bq1r1k/1pp1n1pp/1p1p4/4p2Q/4Pp2/1BNP4/PPP2PPP/3R1RK1 w - - 2 14", 6},
		{"r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15", 6},
		{"r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13", 6},
		{"r1bq1rk1/ppp1nppp/4n3/3p3Q/3P4/1BP1B3/PP1N2PP/R4RK1 w - - 1 16", 6},
		{"4r1k1/r1q2ppp/ppp2n2/4P3/5Rb1/1N1BQ3/PPP3PP/R5K1 w - - 1 17", 6},
		{"2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQ - 0 11", 6},
		{"r1bq1r1k/b1p1npp1/p2p3p/1p6/3PP3/1B2NN2/PP3PPP/R2Q1RK1 w - - 1 16", 6},
		{"3r1rk1/p5pp/bpp1pp2/8/q1PP1P2/b3P3/P2NQRPP/1R2B1K1 b - - 6 22", 6},
		{"r1q2rk1/2p1bppp/2Pp4/p6b/Q1PNp3/4B3/PP1R1PPP/2K4R w - - 2 18", 6},
		{"4k2r/1pb2ppp/1p2p3/1R1p4/3P4/2r1PN2/P4PPP/1R4K1 b - - 3 22", 6},
		{"3q2k1/pb3p1p/4pbp1/2r5/PpN2N2/1P2P2P/5PP1/Q2R2K1 b - - 4 26", 6},
		{NULL, 0}
	};
	
	Board B;
	SearchLimits sl;
	uint64_t signature = 0;
	
	for (int i = 0; test[i].fen; ++i) {
		B.set_fen(test[i].fen);
		std::cout << B.get_fen() << std::endl;
		sl.depth = test[i].depth;
		bestmove(B, sl);
		std::cout << std::endl;
		signature += node_count;
	}
	
	std::cout << "signature = " << signature << std::endl;
}
