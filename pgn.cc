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

PGN::PGN(const Header& _hdeader)
	: header(_hdeader)
{}

void PGN::operator<< (const std::string& san)
{
	san_list.push_back(san);
}

void PGN::Header::operator>> (std::ostream& ostrm) const
{
	ostrm << "[White \"" << white << "\"]\n";
	ostrm << "[Black \"" << black << "\"]\n";

	std::string result = (winner == White) ? "1-0" : (winner == Black ? "0-1" : "1/2-1/2");
	ostrm << "[Result \"" << result << "\"]\n";

	if (!fen.empty())
		ostrm << "[FEN \"" << fen << "\"]\n";
	
	/* TODO print the TimeControl tag. based on sp. Many cases to consider, including asymetric time
	 * control, or control based on nodes or depth, instead of time. */
}

void PGN::operator>> (std::ostream& ostrm) const
/* FIXME: simplistic, need to implement things like "5.. Nf6 6. c4 (...) 35. Qxg7# 1-0" */
{
	header >> ostrm;
	ostrm << std::endl;

	for (auto it = san_list.begin(); it != san_list.end(); ++it)
	{
		ostrm << *it << ' ';
	}

	ostrm << std::endl;
}
