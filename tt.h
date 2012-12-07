#pragma once
#include "board.h"

enum { SCORE_EXACT, SCORE_UBOUND, SCORE_LBOUND };

class TTable
{
public:
	struct Entry {
		Key key;
		uint8_t generation, bound;
		int8_t depth;
		int16_t score;
		move_t move;
		
		void save(Key k, uint8_t g, uint8_t b, int8_t d, int16_t s, move_t m)
		{
			key = k;
			generation = g;
			bound = b;
			depth = d;
			score = s;
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
	
	void store(Key key, uint8_t bound, int8_t depth, int16_t score, move_t move);
	const Entry *probe(Key key) const;
	
private:
	size_t count;
	uint8_t generation;
	Cluster *cluster;
};