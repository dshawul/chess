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
#include "match.h"

int main(int argc, char **argv)
{
	init_bitboard();
	
	Engine E[NB_COLOR];
	E[White].create(argv[1]);
	E[Black].create(argv[2]);
	
	Engine::SearchParam sp;
	sp.wtime = sp.btime = 3000;
	sp.winc = sp.binc = 50;
	
	EPD epd(argv[3]);
	std::string fen = epd.next();
	
	GameResult game_result = game(E, fen, sp, &std::cout);
}