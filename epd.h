/*
 * Zinc, an UCI chess interface. Copyright (C) 2012 Lucas Braesch.
 *
 * Zinc is free software: you can redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zinc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not,
 * see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include <string>
#include <vector>
#include "prng.h"

class EPD
{
public:
	enum Mode { Random, Sequential };

	EPD(const std::string& epd_file, Mode _mode);
	std::string next() const;

private:
	std::vector<std::string> fen_list;
	Mode mode;
	mutable PRNG prng;
	mutable size_t idx;
	size_t count;
};
