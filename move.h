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

enum { NO_CHECK, NORMAL_CHECK, DISCO_CHECK };

extern int move_is_check(const Board& B, move_t m);
extern int move_is_cop(const Board& B, move_t m);	// capture or promotion

extern move_t string_to_move(const Board& B, const std::string& s);
extern std::string move_to_string(move_t m);
extern std::string move_to_san(const Board& B, move_t m);

extern int calc_see(const Board& B, move_t m);
extern bool test_see();

extern int mvv_lva(const Board& B, move_t m);
