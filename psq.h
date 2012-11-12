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
#pragma once
#include "types.h"

// game phase
enum {
    OPENING,
    ENDGAME,
    NB_PHASE
};

// material values
#define OP	85
#define EP	100
#define N	325
#define B	325
#define R	550
#define Q	1000
#define K	20000	// only for SEE

// Bind opening and endgame scores together, for coding simplicity, and performance (cache)
struct Eval {
	int op, eg;

	void clear() {
		op = eg = 0;
	}

	const Eval& operator+= (const Eval& e) {
		op += e.op; eg += e.eg;
		return *this;
	}
};

extern const Eval Material[NB_PIECE];

// PSQ table for WHITE
extern Eval Psq[NB_PIECE][NB_SQUARE];

extern void init_psq();
