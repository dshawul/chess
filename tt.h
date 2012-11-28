#pragma once
#include "board.h"

enum { SCORE_EXACT, SCORE_UBOUND, SCORE_LBOUND };

class TTable
{
public:
	struct Entry {
		Key key;
		int16_t eval, score;
		int8_t depth, type;
		move_t move;
	};

	TTable(): count(0), buf(NULL) {}
	~TTable();
	
	void alloc(uint64_t size);
	void clear();
	
	void write(const Entry& e);
	const Entry *find(Key key) const;
	
private:
	int count;
	Entry *buf;
};