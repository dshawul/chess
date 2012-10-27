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
#include <cstring>
#include <fstream>
#include "epd.h"

EPD::EPD(const std::string& epd_file, Mode _mode)
	: mode(_mode), idx(0)
{
	std::ifstream f(epd_file);

	while (!f.eof()) {
		char s[0x100];
		f.getline(s, sizeof(s));
		strtok(s, ";");
		fen_list.push_back(s);
	}

	count = fen_list.size();
}

std::string EPD::next()
{	
	if (mode == Random)
		idx = prng.draw();

	const size_t idx_result = idx % count;
	
	if (mode == Sequential)
		++idx;
	
	return fen_list[idx_result];
}
