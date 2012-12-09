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
#include <cstring>
#include "tt.h"

TTable::~TTable()
{
	delete[] cluster;
	cluster = NULL;
	generation = 0;
	count = 0;
}

void TTable::alloc(uint64_t size)
{
	if (cluster)
		delete[] cluster;
	
	// calculate the number of clusters allocate
	count = 1ULL << msb(size / sizeof(Cluster));
	
	// Allocate the cluster array. On failure, std::bad_alloc is thrown and not caught, which
	// terminates the program. It's not a bug, it's a "feature" ;-)
	cluster = new Cluster[count];
	
	clear();
}

void TTable::clear()
{
	memset(cluster, 0, count * sizeof(Cluster));
	generation = 0;
}

void TTable::new_search()
{
	++generation;
}

const TTable::Entry *TTable::probe(Key key) const
{
	const Entry *e = cluster[key & (count-1)].entry;
	
	for (size_t i = 0; i < 4; ++i, ++e)
		if (e->key == key)
			return e;
	
	return NULL;
}

void TTable::store(Key key, uint8_t bound, int8_t depth, int16_t score, move_t move)
{
	Entry *e = cluster[key & (count-1)].entry, *replace = e;
	
	for (size_t i = 0; i < 4; ++i, ++e)
	{
		// overwrite empty or old
		if (!e->key || e->key == key) {
			replace = e;
			if (!move)
				move = e->move;
			break;				
		}
		
		// Stockfish replacement strategy
		int c1 = generation == replace->generation ? 2 : 0;
		int c2 = e->generation == generation || e->bound == SCORE_EXACT ? -2 : 0;
		int c3 = e->depth < replace->depth ? 1 : 0;		
		if (c1 + c2 + c3 > 0)
			replace = e;
	}
	
	replace->save(key, generation, bound, depth, score, move);
}