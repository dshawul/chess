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
#include <vector>
#include <string>
#include <ostream>
#include "engine.h"

class PGN
{
public:
	struct Header
	{
		std::string white, black;
		std::string fen;
		Color color;
		int move_count;
		std::string time_control;

		void operator>> (std::ostream& ostrm) const;
	};

	struct Token
	{
		std::string san;
		int depth, score;
		std::string str() const;
	};

	PGN(const Header& _header);
	void set_result(const std::string& _result);

	void operator<< (const Token& token);
	void operator>> (std::ostream& ostrm) const;

private:
	Header header;
	std::vector<Token> tokens;
	std::string result;
};
