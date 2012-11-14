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
#include "movegen.h"

class MoveSort
{
public:
	enum GenType {
	    ALL,				// all legal moves
	    CAPTURES_CHECKS,	// captures and quiet checks
	    CAPTURES			// captures only
	};

	struct Token {
		move_t m;
		int score;
		bool operator< (const Token& t) const {return score < t.score; }
	};

	MoveSort(const Board* _B, GenType _type);
	move_t *next();

private:
	const Board *B;
	GenType type;

	Token list[MAX_MOVES];
	int idx, count;

	move_t *generate(GenType type, move_t *mlist);
	void annotate(const move_t *mlist);
	int score(move_t m);
};
