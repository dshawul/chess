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
	free(buf);
	buf = NULL;
	count = 0;
}

void TTable::alloc(uint64_t size)
{
	const uint64_t old_size = count * sizeof(Entry);
	
	// calculate the number of hash slots to allocate
	for (count = 1; count * sizeof(Entry) <= size; count *= 2);
	count /= 2;

	// adjust size to the correct power of two
	size = count * sizeof(Entry);

	// buf already allocated to the correct size: nothing to do
	if (size == old_size)
		return;

	// use realloc to preserve the first min(size,old_size) bytes
	buf = (Entry *)realloc(buf, size);

	// when growing the buffer, the exceeding portion must be cleared
	if (size > old_size)
		memset((char *)buf + old_size, 0, size - old_size);
}

void TTable::clear()
{
	memset(buf, 0, count * sizeof(Entry));
}

void TTable::write(const TTable::Entry& e)
{
	assert(count && buf);
	Entry& slot = buf[e.key & (count - 1)];

	if (e.key != slot.key || e.depth >= slot.depth)
		slot = e;
}

const TTable::Entry *TTable::find(Key key) const
{
	Entry *slot = &buf[key & (count - 1)];
	return slot->key == key ? slot : NULL;
}