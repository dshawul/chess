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
#include "pgn.h"
#include <sstream>

PGN::PGN(const Header& _header)
	: header(_header)
{}

void PGN::operator<< (const PGN::Token& token)
{
	tokens.push_back(token);
}

void PGN::Header::operator>> (std::ostream& ostrm) const
{
	ostrm << "[White \"" << white << "\"]\n";
	ostrm << "[Black \"" << black << "\"]\n";

	if (!fen.empty())
		ostrm << "[FEN \"" << fen << "\"]\n";

	ostrm << "[TimeControl \"" << time_control << "\"]\n";
}

void PGN::operator>> (std::ostream& ostrm) const
/* FIXME: simplistic, need to implement things like "5.. Nf6 6. c4 (...) 35. Qxg7# 1-0" */
{
	header >> ostrm;
	ostrm << "[Result \"" << result << "\"]\n";

	ostrm << std::endl;
	Color color = header.color;
	int move_count = header.move_count;

	for (auto it = tokens.begin(); it != tokens.end(); ++it, color = opp_color(color)) {
		if (color == Black) {
			if (it == tokens.begin())
				ostrm << move_count << ".. ";
			if (move_count++ % 2 == 0)
				ostrm << '\n';
		} else if (color == White)
			ostrm << move_count << ". ";

		ostrm << it->str();
	}

	ostrm << result << std::endl;
}

void PGN::set_result(const std::string& _result)
/*
 * A result string is typically "1-0", "0-1" or "1/2-1/2". It will be printed as is in the PGN (both
 * in the header section, and at the end of the move list), so you can make it more verbose. eg.
 * "1-0 {Black resigns}", "0-1 {White loses on time}" etc.
 * */
{
	result = _result;
}

std::string PGN::Token::str() const
{
	std::ostringstream s;
	s << san << " {" << float(score)/100 << '/' << depth << ' ' << float(time)/1000 << "s} ";
	return s.str();
}
