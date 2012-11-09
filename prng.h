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
#include <inttypes.h>

class PRNG
{
	// Keep variables always together
	struct S { uint64_t a, b, c, d; } s;

	uint64_t rotate(uint64_t x, uint64_t k) const {
		return (x << k) | (x >> (64 - k));
	}

	// Return 64 bit unsigned integer in between [0, 2^64 - 1]
	uint64_t rand64() {
		const uint64_t
		e = s.a - rotate(s.b,  7);
		s.a = s.b ^ rotate(s.c, 13);
		s.b = s.c + rotate(s.d, 37);
		s.c = s.d + e;
		return s.d = e + s.a;
	}

	// Init seed and scramble a few rounds
	void raninit() {
		s.a = 0xf1ea5eed;
		s.b = s.c = s.d = 0xd4e12c77;
		for (int i = 0; i < 73; i++)
			rand64();
	}

public:
	PRNG() { raninit(); }
	template<typename T> T rand() { return T(rand64()); }
};
