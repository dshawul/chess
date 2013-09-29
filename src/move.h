/*
 * DiscoCheck, an UCI chess engine. Copyright (C) 2011-2013 Lucas Braesch.
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
#include "types.h"

namespace board {
struct Position;
}

namespace move {

enum {
	NORMAL,
	EN_PASSANT,
	PROMOTION,
	CASTLING
};

struct move_t {
	move_t(): b(0) {}	// silence compiler warnings
	explicit move_t(uint16_t _b): b(_b) {}

	operator bool() const;
	bool operator== (move_t m) const;
	bool operator!= (move_t m) const;

	int fsq() const;
	int tsq() const;
	int flag() const;
	int prom() const;

	void fsq(int new_fsq);
	void tsq(int new_tsq);
	void flag(int new_flag);
	void prom(int piece);

private:
	/* a move is incoded in 16 bits, as follows:
	 * 0..5: fsq (from square)
	 * 6..11: tsq (to square)
	 * 12,13: prom (promotion). Uses unusual numbering for optimal compactness: Knight=0 ... Queen=3
	 * 14,15: flag. Flags are: NORMAL=0, EN_PASSANT=1, PROMOTION=2, CASTLING=3 */
	uint16_t b;
};

enum { NO_CHECK, NORMAL_CHECK, DISCO_CHECK };

extern int is_check(const board::Position& B, move_t m);
extern bool is_cop(const board::Position& B, move_t m);	// capture or promotion
extern bool is_pawn_threat(const board::Position& B, move_t m);
extern bool refute(const board::Position& B, move_t m1, move_t m2);

extern move_t string_to_move(const board::Position& B, const std::string& s);
extern std::string move_to_string(move_t m);

extern int see(const board::Position& B, move_t m);
extern int mvv_lva(const board::Position& B, move_t m);

/* move_t member function */

inline move_t::operator bool() const
{
	return b;
}

inline bool move_t::operator== (move_t m) const
{
	return b == m.b;
}

inline bool move_t::operator!= (move_t m) const
{
	return b != m.b;
}

inline int move_t::fsq() const
{
	return b & 0x3f;
}

inline int move_t::tsq() const
{
	return (b >> 6) & 0x3f;
}

inline int move_t::flag() const
{
	return (b >> 14) & 3;
}

inline int move_t::prom() const
{
	assert(flag() == PROMOTION);
	return ((b >> 12) & 3) + KNIGHT;
}

inline void move_t::fsq(int new_fsq)
{
	assert(square_ok(new_fsq));
	b &= 0xffc0;
	b ^= new_fsq;
}

inline void move_t::tsq(int new_tsq)
{
	assert(square_ok(new_tsq));
	b &= 0xf03f;
	b ^= (new_tsq << 6);
}

inline void move_t::flag(int new_flag)
{
	assert(new_flag < 4);
	b &= 0x3fff;
	b ^= (new_flag << 14);
}

inline void move_t::prom(int piece)
{
	assert(KNIGHT <= piece && piece <= QUEEN);
	b &= 0xcfff;
	b ^= (piece - KNIGHT) << 12;
}

}	// namespace move

