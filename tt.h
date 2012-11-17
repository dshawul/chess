#pragma once
#include "board.h"

enum { SCORE_EXACT, SCORE_UBOUND, SCORE_LBOUND };

class TT
{
public:
	struct Entry {
		Key key;
		int16_t eval, score;
		int8_t depth, type;
		move_t move;
	};

	TT(): count(0), buf(NULL) {}
	~TT();
	
	void alloc(uint64_t size);
	void write(const Entry *e);
	Entry *find(Key key) const;
	
private:
	int count;
	Entry *buf;
};