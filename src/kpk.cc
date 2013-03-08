#include "bitboard.h"

namespace {
	const unsigned IndexMax = 64*64*2*24;	// wk * bk * stm * wp	
	uint64_t bitbase[IndexMax/64];
	enum {ILLEGAL=0, UNKNOWN=1, DRAW=2, WIN=4};

	unsigned encode(int wk, int bk, int stm, int wp)
	{
		assert(square_ok(wk) && square_ok(bk) && color_ok(stm));
		assert(file(wp) <= FILE_D && RANK_2 <= rank(wp) && rank(wp) <= RANK_7);
		
		const unsigned wp24 = 4*(RANK_7-rank(wp)) + file(wp);
		assert(wp24 < 24);
		
		return wk ^ (bk << 6) ^ (stm << 12) ^ (wp24 << 13);
	}

	void decode(unsigned idx, int *wk, int *bk, int *stm, int *wp)
	{
		assert(idx < IndexMax);
		*wk = idx & 63; idx >>= 6;
		*bk = idx & 63; idx >>= 6;
		*stm = idx & 1; idx >>= 1;
		
		assert(idx < 24);
		*wp = square(RANK_7 - idx/4, idx & 3);
	}

	uint8_t rules(unsigned idx)
	{
		assert(idx < IndexMax);
		int wk, bk, stm, wp;
		decode(idx, &wk, &bk, &stm, &wp);
		
		// pieces overlapping or kings checking each other
		if (kdist(wk, bk) <= 1 || wp == wk || wp == bk)
			return ILLEGAL;	
		// cannot be white's turn if black is in check
		if (stm == WHITE && test_bit(PAttacks[WHITE][wp], bk))
			return ILLEGAL;
		
		// win if pawn can be promoted without getting captured
		if (stm == WHITE) {
			if (rank(wp) == RANK_7 && bk != wp+8 && wk != wp+8
				&& !test_bit(KAttacks[bk] & ~KAttacks[wk], wp+8))
				return WIN;
		} else if ( !(KAttacks[bk] & ~(KAttacks[wk] | PAttacks[WHITE][wp]))
			|| test_bit(KAttacks[bk] & ~KAttacks[wk], wp) )
			return DRAW;
		
		return UNKNOWN;
	}

	uint8_t classify(uint8_t res[], unsigned idx)
	{
		assert(idx < IndexMax && res[idx] == UNKNOWN);
		int wk, bk, stm, wp;
		decode(idx, &wk, &bk, &stm, &wp);

		uint8_t r = ILLEGAL;
		Bitboard b = KAttacks[stm ? bk : wk];
		
		// king moves
		while (b) {
			const int sq = pop_lsb(&b);
			r |= res[stm ? encode(wk, sq, WHITE, wp) : encode(sq, bk, BLACK, wp)];
		}
		
		// pawn moves
		if (stm == WHITE && rank(wp) < RANK_7) {
			// single push
			const int sq = wp+8;
			r |= res[encode(wk, bk, BLACK, sq)];
			
			// double push
			if (rank(wp) == RANK_2 && sq != wk && sq != bk)
				r |= res[encode(wk, bk, BLACK, sq+8)];
		}
		
		if (stm == WHITE)
			return res[idx] = r & WIN ? WIN : (r & UNKNOWN ? UNKNOWN : DRAW);
		else
			return res[idx] = r & DRAW ? DRAW : (r & UNKNOWN ? UNKNOWN : WIN);
	}

	bool kpk_ok(uint8_t res[])
	{
		unsigned illegal = 0, win = 0;
		
		for (unsigned idx = 0; idx < IndexMax; ++idx) {
			if (res[idx] == ILLEGAL)
				++illegal;
			else if (res[idx] == WIN)
				++win;
		}	
		
		return illegal == 30932 && win == 111282;
	}
}

void init_kpk()
{
	// 192 KB on the stack
	uint8_t res[IndexMax];
	
	// first pass: apply static rules
	for (unsigned idx = 0; idx < IndexMax; ++idx)
		res[idx] = rules(idx);
	
	// iterate until all positions are computed
	bool repeat = true;
	while (repeat) {
		repeat = false;
		for (unsigned idx = 0; idx < IndexMax; ++idx)
			repeat |= (res[idx] == UNKNOWN && classify(res, idx) != UNKNOWN);
	}
	
	// it's so easy to screw up the KPK that I don't even trust myself with it...
	assert(kpk_ok(res));
	
	// Pack into 64-bit entries
	for (unsigned idx = 0; idx < IndexMax; ++idx)
		if (res[idx] == WIN)
			set_bit(&bitbase[idx/64], idx % 64);
}

bool probe_kpk(int wk, int bk, int stm, int wp)
{
	const unsigned idx = encode(wk, bk, stm, wp);
	return test_bit(bitbase[idx/64], idx % 64);
}