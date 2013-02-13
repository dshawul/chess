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
#include "board.h"

enum { BOUND_EXACT, BOUND_UPPER, BOUND_LOWER };

class TTable
{
public:
	struct Entry {
		Key key_bound;	// bit 0..1 for bounr2 bits for bound, and 2..63 for key's 62 MSB
		mutable uint8_t generation;
		int8_t depth;
		int16_t score, eval;
		move_t move;

		int bound() const {
			return key_bound & 3;
		}

		bool key_match(Key k) const {
			return (key_bound & ~3ULL) == (k & ~3ULL);
		}

		void save(Key k, uint8_t g, uint8_t b, int8_t d, int16_t s, int16_t e, move_t m) {
			key_bound = (k & ~3ULL) ^ b;
			generation = g;
			depth = d;
			score = s;
			eval = e;
			move = m;
		}
	};

	struct Cluster {
		Entry entry[4];
	};

	TTable(): count(0), cluster(NULL) {}
	~TTable();

	void alloc(uint64_t size);
	void clear();

	void new_search();
	void refresh(const Entry *e) const { e->generation = generation; }

	void store(Key key, uint8_t bound, int8_t depth, int16_t score, int16_t eval, move_t move);
	const Entry *probe(Key key) const;

private:
	size_t count;
	uint8_t generation;
	Cluster *cluster;
};
