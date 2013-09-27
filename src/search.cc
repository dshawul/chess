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
*/
#include <chrono>
#include <vector>
#include "search.h"
#include "uci.h"
#include "eval.h"
#include "movesort.h"
#include "prng.h"

using namespace std::chrono;

namespace search {

TTable TT;
Refutation R;

std::uint64_t node_count;
std::uint64_t PollingFrequency;

}	// namespace search

namespace {

const int MAX_DEPTH = 127;	// pvs() depth go from MAX_DEPTH down to 1
const int MIN_DEPTH = -8;	// qsearch() depth go from 0 downto -MIN_DEPTH-1
const int MAX_PLY = MAX_DEPTH - MIN_DEPTH + 1;	// plies go from 0 to MAX_PLY

const int MATE = 16000;

bool can_abort;
struct AbortSearch {};
struct ForcedMove {};

std::uint64_t node_limit;
int time_limit[2], time_allowed;
time_point<high_resolution_clock> start;

History H;

const int RazorMargin[4] = {0, 2 * vEP, 2 * vEP + vEP / 2, 3 * vEP};
const int EvalMargin[4]	 = {0, vEP, vN, vQ};

int DrawScore[NB_COLOR];	// Contempt draw score by color

move::move_t best_move, ponder_move;
bool best_move_changed;

void node_poll(board::Position &B)
{
	if (!(++search::node_count & (search::PollingFrequency - 1)) && can_abort) {
		bool abort = false;

		// abort search because node limit exceeded
		if (node_limit && search::node_count >= node_limit)
			abort = true;
		// abort search because time limit exceeded
		else if (time_allowed && duration_cast<milliseconds>
				 (high_resolution_clock::now() - start).count() > time_allowed)
			abort = true;
		// abort when UCI "stop" command is received (involves some non standard I/O)
		else if (uci::stop())
			abort = true;

		if (abort)
			throw AbortSearch();
	}
}

bool is_mate_score(int score)
{
	return std::abs(score) >= MATE - MAX_PLY;
}

int mated_in(int ply)
{
	return ply - MATE;
}

int mate_in(int ply)
{
	return MATE - ply;
}

int null_reduction(int depth)
{
	return 3 + depth / 4;
}

int score_to_tt(int score, int ply)
/* mate scores from the search, must be adjusted to be written in the TT. For example, if we find a
 * mate in 10 plies from the current position, it will be scored mate_in(15) by the search and must
 * be entered mate_in(10) in the TT */
{
	return score >= mate_in(MAX_PLY) ? score + ply :
		   score <= mated_in(MAX_PLY) ? score - ply : score;
}

int score_from_tt(int tt_score, int ply)
/* mate scores from the TT need to be adjusted. For example, if we find a mate in 10 in the TT at
 * ply 5, then we effectively have a mate in 15 plies (from the root) */
{
	return tt_score >= mate_in(MAX_PLY) ? tt_score - ply :
		   tt_score <= mated_in(MAX_PLY) ? tt_score + ply : tt_score;
}

bool can_return_tt(bool is_pv, const TTable::Entry *tte, int depth, int beta, int ply)
/* PV nodes: return only exact scores
 * non PV nodes: return fail high/low scores. Mate scores are also trusted, regardless of the
 * depth. This idea is from StockFish, and although it's not totally sound, it seems to work. */
{
	const bool depth_ok = tte->depth >= depth;

	if (is_pv)
		return depth_ok && tte->node_type() == PV
			   && ply >= 2;	// to have at least a ponder move in the PV
	else {
		const int tt_score = score_from_tt(tte->score, ply);
		return (depth_ok
				|| tt_score >= std::max(mate_in(MAX_PLY), beta)
				|| tt_score < std::min(mated_in(MAX_PLY), beta))
			   && ((tte->node_type() == Cut && tt_score >= beta)
				   || (tte->node_type() == All && tt_score < beta));
	}
}

void time_alloc(const search::Limits& sl, int result[2])
{
	if (sl.movetime > 0)
		result[0] = result[1] = sl.movetime;
	else if (sl.time > 0 || sl.inc > 0) {
		static const int time_buffer = 100;
		int movestogo = sl.movestogo > 0 ? sl.movestogo : 30;
		result[0] = std::max(std::min(sl.time / movestogo + sl.inc, sl.time - time_buffer), 1);
		result[1] = std::max(std::min(sl.time / (1 + movestogo / 2) + sl.inc, sl.time - time_buffer), 1);
	}
}

int qsearch(board::Position& B, int alpha, int beta, int depth, int node_type, SearchInfo *ss, move::move_t *pv)
{
	assert(depth <= 0);
	assert(alpha < beta && (node_type == PV || alpha + 1 == beta));

	const Key key = B.get_key();
	search::TT.prefetch(key);
	node_poll(B);

	const bool in_check = B.is_check();
	int best_score = -INF, old_alpha = alpha;
	ss->best = move::move_t(0);

	move::move_t subtree_pv[MAX_PLY - ss->ply];
	if (node_type == PV)
		pv[0] = move::move_t(0);

	if (B.is_draw())
		return DrawScore[B.get_turn()];

	const Bitboard hanging = hanging_pieces(B);

	// TT lookup
	const TTable::Entry *tte = search::TT.probe(key);
	if (tte) {
		if (can_return_tt(node_type == PV, tte, depth, beta, ss->ply)) {
			search::TT.refresh(tte);
			return score_from_tt(tte->score, ss->ply);
		}
		ss->eval = tte->eval;
		ss->best = tte->move;
	} else
		ss->eval = in_check ? -INF : (ss->null_child ? -(ss - 1)->eval : eval::symmetric_eval(B));

	// stand pat
	if (!in_check) {
		best_score = ss->eval + eval::asymmetric_eval(B, hanging);
		alpha = std::max(alpha, best_score);
		if (alpha >= beta)
			return alpha;
	}

	MoveSort MS(&B, depth, ss, &H, nullptr);
	int see;
	const int fut_base = ss->eval + vEP / 2;

	while ( alpha < beta && (ss->m = MS.next(&see)) ) {
		int check = move::is_check(B, ss->m);

		// Futility pruning
		if (!check && !in_check && node_type != PV) {
			// opt_score = current eval + some margin + max material gain of the move
			const int opt_score = fut_base
								  + Material[B.get_piece_on(ss->m.tsq())].eg
								  + (ss->m.flag() == move::EN_PASSANT ? vEP : 0)
								  + (ss->m.flag() == move::PROMOTION ? Material[ss->m.prom()].eg - vOP : 0);

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
		if (!in_check && check != move::DISCO_CHECK && see < 0)
			continue;

		// recursion
		int score;
		if (depth <= MIN_DEPTH && !in_check)		// prevent qsearch explosion
			score = ss->eval + see;
		else {
			B.play(ss->m);
			score = -qsearch(B, -beta, -alpha, depth - 1, -node_type, ss + 1, subtree_pv);
			B.undo();
		}

		if (score > best_score) {
			best_score = score;

			if (score > alpha) {
				alpha = score;

				if (node_type == PV) {
					// update the PV
					pv[0] = ss->m;
					for (int i = 0; i < MAX_PLY - ss->ply; i++)
						if (!(pv[i + 1] = subtree_pv[i]))
							break;
				}
			}

			ss->best = ss->m;
		}
	}

	if (B.is_check() && !MS.get_count())
		return mated_in(ss->ply);

	// update TT
	node_type = best_score <= old_alpha ? All : best_score >= beta ? Cut : PV;
	search::TT.store(key, node_type, depth, score_to_tt(best_score, ss->ply), ss->eval, ss->best);

	return best_score;
}

int pvs(board::Position& B, int alpha, int beta, int depth, int node_type, SearchInfo *ss, move::move_t *pv)
{
	assert(alpha < beta && (node_type == PV || alpha + 1 == beta));

	if (depth <= 0 || ss->ply >= MAX_DEPTH)
		return qsearch(B, alpha, beta, depth, node_type, ss, pv);

	const Key key = B.get_key();
	search::TT.prefetch(key);

	move::move_t subtree_pv[MAX_PLY - ss->ply];
	if (node_type == PV)
		pv[0] = move::move_t(0);

	node_poll(B);

	const bool root = !ss->ply, in_check = B.is_check();
	const int old_alpha = alpha, static_node_type = node_type;
	int best_score = -INF;
	ss->best = move::move_t(0);

	if (B.is_draw())
		return DrawScore[B.get_turn()];

	// mate distance pruning
	alpha = std::max(alpha, mated_in(ss->ply));
	beta = std::min(beta, mate_in(ss->ply + 1));
	if (alpha >= beta) {
		assert(!root);
		return alpha;
	}

	const Bitboard hanging = hanging_pieces(B);

	// TT lookup
	const TTable::Entry *tte = search::TT.probe(key);
	if (tte) {
		if (!root && can_return_tt(node_type == PV, tte, depth, beta, ss->ply)) {
			search::TT.refresh(tte);
			return score_from_tt(tte->score, ss->ply);
		}
		ss->eval = tte->eval;
		ss->best = tte->move;
	} else
		ss->eval = in_check ? -INF : (ss->null_child ? -(ss - 1)->eval : eval::symmetric_eval(B));

	// Stand pat score (adjusted for tempo and hanging pieces)
	const int stand_pat = ss->eval + eval::asymmetric_eval(B, hanging);

	// Eval pruning
	if ( depth <= 3 && node_type != PV
		 && !in_check && !is_mate_score(beta)
		 && stand_pat >= beta + EvalMargin[depth]
		 && B.st().piece_psq[B.get_turn()] )
		return stand_pat;

	// Razoring
	if ( depth <= 3 && node_type != PV
		 && !in_check && !is_mate_score(beta) ) {
		const int threshold = beta - RazorMargin[depth];
		if (ss->eval < threshold) {
			const int score = qsearch(B, threshold - 1, threshold, 0, All, ss + 1, subtree_pv);
			if (score < threshold)
				return score;
		}
	}

	// Null move pruning
	move::move_t threat_move = move::move_t(0);
	if ( ss->eval >= beta	// eval symmetry prevents double null moves
		 && !ss->skip_null && node_type != PV
		 && !in_check && !is_mate_score(beta)
		 && B.st().piece_psq[B.get_turn()] ) {
		const int reduction = null_reduction(depth) + (ss->eval - vOP >= beta);

		// if the TT entry tells us that no move can beat alpha at the null search depth or deeper,
		// we safely skip the null search.
		if ( tte && tte->depth >= depth - reduction
			 && tte->node_type() != Cut	// ie. upper bound or exact score
			 && tte->score <= alpha )
			goto tt_skip_null;

		B.play(move::move_t(0));
		(ss + 1)->null_child = (ss + 1)->skip_null = true;
		const int score = -pvs(B, -beta, -alpha, depth - reduction, All, ss + 1, subtree_pv);
		(ss + 1)->null_child = (ss + 1)->skip_null = false;
		B.undo();

		if (score >= beta)	// null search fails high
			return score < mate_in(MAX_PLY)
				   ? score	// fail soft
				   : beta;	// but do not return an unproven mate
		else {
			threat_move = (ss + 1)->best;
			if (score <= mated_in(MAX_PLY) && (ss - 1)->reduction) {
				++depth;
				--(ss - 1)->reduction;
			}
		}
	}

tt_skip_null:

	// Internal Iterative Deepening
	if ( (!tte || !tte->move || tte->depth <= 0)
		 && depth >= (node_type == PV ? 4 : 7) ) {
		ss->skip_null = true;
		pvs(B, alpha, beta, node_type == PV ? depth - 2 : depth / 2, node_type, ss, subtree_pv);
		ss->skip_null = false;
	}

	MoveSort MS(&B, depth, ss, &H, &search::R);
	const move::move_t refutation = search::R.get_refutation(B.get_dm_key());

	int cnt = 0, LMR = 0, see;
	while ( alpha < beta && (ss->m = MS.next(&see)) ) {
		++cnt;
		const int check = move::is_check(B, ss->m);

		// check extension
		int new_depth;
		if (check && (check == move::DISCO_CHECK || see >= 0) )
			// extend relevant checks
			new_depth = depth;
		else if (MS.get_count() == 1)
			// extend forced replies
			new_depth = depth;
		else
			new_depth = depth - 1;

		// move properties
		const bool first = cnt == 1;
		const bool capture = move::is_cop(B, ss->m);
		const int hscore = capture ? 0 : H.get(B, ss->m);
		const bool bad_quiet = !capture && (hscore < 0 || (hscore == 0 && see < 0));
		const bool bad_capture = capture && see < 0;
		// dangerous movea are not reduced
		const bool dangerous = check
							   || ss->m == ss->killer[0]
							   || ss->m == ss->killer[1]
							   || ss->m == refutation
							   || (move::is_pawn_threat(B, ss->m) && see >= 0)
							   || (ss->m.flag() == move::CASTLING);

		if (!capture && !dangerous && !in_check && !root) {
			// Move count pruning
			if ( depth <= 6 && node_type != PV
				 && LMR >= 3 + depth * (2 * depth - 1) / 2
				 && alpha > mated_in(MAX_PLY)
				 && (see < 0 || !refute(B, ss->m, threat_move)) ) {
				best_score = std::max(best_score, std::min(alpha, stand_pat + see));
				continue;
			}

			// SEE pruning near the leaves
			if (new_depth <= 1 && see < 0) {
				best_score = std::max(best_score, std::min(alpha, stand_pat + see));
				continue;
			}
		}

		// reduction decision
		ss->reduction = !first && (bad_capture || bad_quiet) && !dangerous;
		if (ss->reduction && !capture)
			ss->reduction += ++LMR >= (static_node_type == Cut ? 2 : 3) + 8 / depth;

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
			score = -pvs(B, -beta, -alpha, new_depth, -node_type, ss + 1, subtree_pv);
		else {
			// Cut node: If the first move didn't produce the expected cutoff, then we are
			// unlikely to get a cutoff at this node, which becomes an All node, so that its
			// children are Cut nodes
			if (node_type == Cut)
				node_type = All;

			// zero window search (reduced)
			score = -pvs(B, -alpha - 1, -alpha, new_depth - ss->reduction,
						 node_type == PV ? Cut : -node_type, ss + 1, subtree_pv);

			// doesn't fail low: verify at full depth, with zero window
			if (score > alpha && ss->reduction)
				score = -pvs(B, -alpha - 1, -alpha, new_depth, All, ss + 1, subtree_pv);

			// still doesn't fail low at PV node: full depth and full window
			if (node_type == PV && score > alpha)
				score = -pvs(B, -beta, -alpha, new_depth , PV, ss + 1, subtree_pv);
		}

		B.undo();

		if (score > best_score) {
			best_score = score;
			ss->best = ss->m;

			if (score > alpha) {
				alpha = score;

				if (node_type == PV) {
					// update the PV
					pv[0] = ss->m;
					for (int i = 0; i < MAX_PLY - ss->ply; i++)
						if (!(pv[i + 1] = subtree_pv[i]))
							break;
				}
			}

			if (root) {
				if (best_move != ss->m) {
					best_move_changed = true;
					best_move = ss->m;
				}
				ponder_move = pv[1];
			}
		}
	}

	if (!MS.get_count()) {
		// mated or stalemated
		assert(!root);
		return in_check ? mated_in(ss->ply) : DrawScore[B.get_turn()];
	} else if (root && MS.get_count() == 1 && depth >= 2)
		// forced move at the root node, play instantly and prevent further iterative deepening
		// depth >= 2 is to make sure we have a ponder move pv[1]
		throw ForcedMove();

	// update TT
	node_type = best_score <= old_alpha ? All : best_score >= beta ? Cut : PV;
	search::TT.store(key, node_type, depth, score_to_tt(best_score, ss->ply), ss->eval, ss->best);

	// best move is quiet: update killers and history
	if (ss->best && !move::is_cop(B, ss->best)) {
		// update killers on a LIFO basis
		if (ss->killer[0] != ss->best) {
			ss->killer[1] = ss->killer[0];
			ss->killer[0] = ss->best;
		}

		// update history table
		// mark ss->best as good, and all other moves searched as bad
		move::move_t m;
		int bonus = std::min(depth * depth, (int)History::Max);
		if (hanging) bonus /= 2;
		while ( (m = MS.previous()) )
			if (!move::is_cop(B, m))
				H.add(B, m, m == ss->best ? bonus : -bonus);

		// update double move refutation hash table
		search::R.set_refutation(B.get_dm_key(), ss->best);
	}

	return best_score;
}

}	// namespace

namespace search {

std::pair<move::move_t, move::move_t> bestmove(board::Position& B, const Limits& sl)
// returns a pair (best move, ponder move)
{
	start = high_resolution_clock::now();

	SearchInfo ss[MAX_PLY + 1];
	for (int ply = 0; ply <= MAX_PLY; ++ply)
		ss[ply].clear(ply);

	node_count = 0;
	node_limit = sl.nodes;
	time_alloc(sl, time_limit);

	best_move = ponder_move = move::move_t(0);
	best_move_changed = false;

	H.clear();
	TT.new_search();
	B.set_root();	// remember root node, for correct 2/3-fold in is_draw()

	// Calculate the value of a draw by chess rules, for both colors (contempt option)
	const int us = B.get_turn(), them = opp_color(us);
	DrawScore[us] = -uci::Contempt;
	DrawScore[them] = +uci::Contempt;

	move::move_t pv[MAX_PLY + 1];
	const int max_depth = sl.depth ? std::min(MAX_DEPTH, sl.depth) : MAX_DEPTH;

	for (int depth = 1, alpha = -INF, beta = +INF; depth <= max_depth; depth++) {
		// iterative deepening loop

		// We can only abort the search once iteration 1 is finished. In extreme situations (eg.
		// fixed nodes), the SearchLimits sl could trigger a search abortion before that, which is
		// disastrous, as the best move could be illegal or completely stupid.
		can_abort = depth >= 2;

		int score, delta = 16;

		// Time allowance
		time_allowed = time_limit[best_move_changed];
		if (best_move && move::see(B, best_move) > 0)
			time_allowed /= 2;

		best_move_changed = false;
		for (;;) {
			// Aspiration loop

			can_abort = depth >= 2;
			try {
				score = pvs(B, alpha, beta, depth, PV, ss, pv);
			} catch (AbortSearch e) {
				goto return_pair;
			} catch (ForcedMove e) {
				best_move = ss->best;
				goto return_pair;
			}

			if (is_mate_score(score))
				std::cout << "info score mate " << (score > 0 ? (MATE - score + 1) / 2 : -(score + MATE + 1) / 2);
			else
				std::cout << "info score cp " << score;
			std::cout << " depth " << depth << " nodes " << node_count << " time "
					  << duration_cast<milliseconds>(high_resolution_clock::now() - start).count();

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

		// display the PV
		std::cout << " pv";
		for (int i = 0; i <= MAX_PLY && pv[i]; i++)
			std::cout << ' ' << move_to_string(pv[i]);
		std::cout << std::endl;
	}

return_pair:
	return std::make_pair(best_move, ponder_move);
}

extern void clear_state()
{
	TT.clear();
	R.clear();
}

}	// namespace search
