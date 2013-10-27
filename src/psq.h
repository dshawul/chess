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
#pragma once
#include "types.h"

// game phase
enum { OPENING, ENDGAME, NB_PHASE };

// material values
enum {
	vOP = 80, vEP = 100,
	vN = 330, vB = 330,
	vR = 545, vQ = 1000,
	vK = 20000 // only for SEE
};

// Bind opening and endgame scores together
struct Eval {
	int op, eg;

	bool operator== (const Eval& e) {
		return op == e.op && eg == e.eg;
	}

	bool operator!= (const Eval& e) {
		return !(*this == e);
	}

	const Eval& operator+= (const Eval& e) {
		op += e.op;
		eg += e.eg;
		return *this;
	}

	const Eval& operator-= (const Eval& e) {
		op -= e.op;
		eg -= e.eg;
		return *this;
	}
};

extern const Eval Material[NB_PIECE+1];

// PSQ table for WHITE
extern Eval PsqTable[NB_PIECE][NB_SQUARE];

extern void init_psq();

inline const Eval& get_psq(int color, int piece, int sq)
{
	return PsqTable[piece][color ? rank_mirror(sq) : sq];
}
