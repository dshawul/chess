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
#include "psq.h"

const Eval Material[NB_PIECE] = { {OP, EP}, {N, N}, {B, B}, {R, R}, {Q, Q}, {K, K} };

Eval Psq[NB_PIECE][NB_SQUARE];

namespace
{
	Eval psq_bonus(int piece, int sq)
	{
		// TODO
		return {0, 0};
	}
}

void init_psq()
{
	for (int piece = PAWN; piece <= KING; ++piece) {
		for (int sq = A1; sq <= H8; ++sq) {
			Eval& e = Psq[piece][sq];
			e = psq_bonus(piece, sq);

			if (piece < KING)
				e += Material[piece];
		}
	}
}
