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
#include "types.h"
#include "board.h"
#include "epd.h"
#include "engine.h"

int main(int argc, char **argv)
{
	init_bitboard();

	Engine E[NB_COLOR];
	E[White].create(argv[1]);
	E[Black].create(argv[2]);

	Clock clk;
	clk.set_time(3000);
	clk.set_inc(100);
	clk.set_nodes(10000);
	
	E[White].clk = clk;
	E[Black].clk = clk;

	EPD epd(argv[3], EPD::Random);
	std::string fen = epd.next();

	E[White].set_position(fen, "");
	SearchResult r = E[White].search(White);
	std::cout << r.bestmove << '\t' << r.depth << '\t' << r.score << std::endl;
}