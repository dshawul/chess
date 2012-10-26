/*
 * Zinc, an UCI chess interface. Copyright (C) 2012 Lucas Braesch.
 *
 * Zinc is free software: you can redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zinc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not,
 * see <http://www.gnu.org/licenses/>.
*/
#include "prng.h"

namespace
{
	uint64_t rol(uint64_t x, unsigned k)
	{
		return (x << k) | (x >> (64 - k));
	}
}

PRNG::PRNG(uint64_t seed)
	: a(seed)
{}

void PRNG::seed(uint64_t seed)
{
	a = seed;
}

uint64_t PRNG::draw()
{
	const uint64_t e = a - rol(b,  7);
	a = b ^ rol(c, 13);
	b = c + rol(d, 37);
	c = d + e;
	return d = e + a;
}
