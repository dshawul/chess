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
 *
 * Credits:
 * - This KPK bitbase code is completely copied from Stockfish (modified and adapted, obviously)
*/
#include <vector>
#include "bitboard.h"

namespace
{
	// The possible pawns squares are 24, the first 4 files and ranks from 2 to 7
	const unsigned IndexMax = 2*24*64*64; // stm * psq * wksq * bksq = 196608

	// Each uint32_t stores results of 32 positions, one per bit
	uint32_t KPKBitbase[IndexMax / 32];

	// A KPK bitbase index is an integer in [0, IndexMax] range
	//
	// Information is mapped in a way that minimizes number of iterations:
	//
	// bit  0- 5: white king square (from SQ_A1 to SQ_H8)
	// bit  6-11: black king square (from SQ_A1 to SQ_H8)
	// bit    12: side to move (WHITE or BLACK)
	// bit 13-14: white pawn file (from FILE_A to FILE_D)
	// bit 15-17: white pawn 6 - rank (from 6 - RANK_7 to 6 - RANK_2)
	unsigned index(int us, int bksq, int wksq, int psq)
	{
		assert(color_ok(us) && square_ok(wksq) && square_ok(bksq) && square_ok(psq));
		return wksq + (bksq << 6) + (us << 12) + (file(psq) << 13) + ((6 - rank(psq)) << 15);
	}

	enum Result { INVALID = 0, UNKNOWN = 1, DRAW = 2, WIN = 4 };
	inline Result& operator|=(Result& r, const Result v) { return r = Result(r | v); }

	struct KPKPosition {
		operator Result() const { return res; }
		Result classify_leaf(unsigned idx);
		Result classify(const std::vector<KPKPosition>& db);

	private:
		int us, bksq, wksq, psq;
		Result res;
	};
} // namespace

bool probe_kpk(int wksq, int wpsq, int bksq, int us)
{
	assert(file(wpsq) <= FILE_D);
	unsigned idx = index(us, bksq, wksq, wpsq);
	return KPKBitbase[idx / 32] & (1 << (idx & 0x1F));
}

void init_kpk()
{
	unsigned idx, repeat = 1;
	std::vector<KPKPosition> db(IndexMax);

	// Initialize db with known win / draw positions
	for (idx = 0; idx < IndexMax; idx++)
		db[idx].classify_leaf(idx);

	// Iterate until all positions are classified (15 cycles needed)
	while (repeat)
		for (repeat = idx = 0; idx < IndexMax; idx++)
			if (db[idx] == UNKNOWN && db[idx].classify(db) != UNKNOWN)
				repeat = 1;

	// Map 32 results into one KPKBitbase[] entry
	for (idx = 0; idx < IndexMax; idx++)
		if (db[idx] == WIN)
			KPKBitbase[idx / 32] |= 1 << (idx & 0x1F);
}

namespace
{
	Result KPKPosition::classify_leaf(unsigned idx)
	{
		wksq = (idx >> 0) & 0x3F;
		bksq = (idx >> 6) & 0x3F;
		us   = (idx >> 12) & 0x01;
		psq  =  square(6 - (idx >> 15), (idx >> 13) & 3);

		// Check if two pieces are on the same square or if a king can be captured
		if ( wksq == psq || wksq == bksq || bksq == psq
			|| test_bit(KAttacks[wksq], bksq)
			|| (us == WHITE && test_bit(PAttacks[WHITE][psq], bksq)) )
			return res = INVALID;

		if (us == WHITE) {
			// Immediate win if pawn can be promoted without getting captured
			if ( rank(psq) == RANK_7
				&& wksq != psq + 8
				&& (kdist(bksq, psq + 8) > 1 || test_bit(KAttacks[wksq], psq + 8)) )
				return res = WIN;
		}
		// Immediate draw if is stalemate or king captures undefended pawn
		else if ( !(KAttacks[bksq] & ~(KAttacks[wksq] | KAttacks[psq]))
			|| test_bit(KAttacks[bksq] & ~KAttacks[wksq], psq) )
			return res = DRAW;

		return res = UNKNOWN;
	}

	Result KPKPosition::classify(const std::vector<KPKPosition>& db)
	{
		// White to Move: If one move leads to a position classified as WIN, the result
		// of the current position is WIN. If all moves lead to positions classified
		// as DRAW, the current position is classified DRAW otherwise the current
		// position is classified as UNKNOWN.
		//
		// Black to Move: If one move leads to a position classified as DRAW, the result
		// of the current position is DRAW. If all moves lead to positions classified
		// as WIN, the position is classified WIN otherwise the current position is
		// classified UNKNOWN.

		Result r = INVALID;
		Bitboard b = KAttacks[us == WHITE ? wksq : bksq];

		while (b)
			r |= us == WHITE ? db[index(opp_color(us), bksq, pop_lsb(&b), psq)]
				: db[index(opp_color(us), pop_lsb(&b), wksq, psq)];

		if (us == WHITE && rank(psq) < RANK_7) {
			int s = psq + 8;
			r |= db[index(BLACK, bksq, wksq, s)]; // Single push

			if (rank(s) == RANK_3 && s != wksq && s != bksq)
			r |= db[index(BLACK, bksq, wksq, s + 8)]; // Double push
		}

		if (us == WHITE)
			return res = r & WIN  ? WIN  : r & UNKNOWN ? UNKNOWN : DRAW;
		else
			return res = r & DRAW ? DRAW : r & UNKNOWN ? UNKNOWN : WIN;
	}
} // namespace