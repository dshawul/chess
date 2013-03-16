/*
 * DiscoCheck, an UCI chess interface. Copyright (C) 2011-2013 Lucas Braesch.
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
 * Credits: Bob Jenkins' 64-bit PRNG from the Marsaglia "KISS family". Seeds from Heinz Van Saanen.
*/
#pragma once
#include <cinttypes>

struct PRNG
{
	PRNG() { init(); }

	// Return 64 bit unsigned integer in between [0, 2^64 - 1]
	uint64_t rand() {
		const uint64_t
		e = a - rotate(b,  7);
		a = b ^ rotate(c, 13);
		b = c + rotate(d, 37);
		c = d + e;
		return d = e + a;
	}

private:
	// generator state
	uint64_t a, b, c, d;

	uint64_t rotate(uint64_t x, uint64_t k) const {
		return (x << k) | (x >> (64 - k));
	}

	// Init seed and scramble a few rounds
	void init() {
		a = 0x46dd577ff603b540;
		b = 0xc4077bddfacf987b;
		c = 0xbbf4d93b7200e858;
		d = 0xd3e075cfd449bb1e;
	}
};
