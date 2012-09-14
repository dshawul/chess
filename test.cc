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
#include "board.h"
#include <time.h>

typedef struct
{
	const char *s;
	unsigned depth;
	uint64_t value;
} TestPerft;

bool test_perft()
{
	init_bitboard();
	Board B;

	TestPerft Test[] =
	{
		{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", 6, 119060324ULL},
		{"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 5, 193690690ULL},
		{"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 7, 178633661ULL},
		{"r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ -", 6, 706045033ULL},
		{NULL, 0, 0}
	};

	uint64_t total = 0;
	clock_t start = clock();

	for (unsigned i = 0; Test[i].s; i++)
	{
		std::cout << Test[i].s << std::endl;
		B.set_fen(Test[i].s);
		if (perft(B, Test[i].depth, 0) != Test[i].value)
		{
			std::cerr << "Incorrect perft" << std::endl;
			return false;
		}
		total += Test[i].value;
	}

	double elapsed = double(clock() - start) / CLOCKS_PER_SEC;
	std::cout << "speed: " << (unsigned)(total / elapsed) << " leaf/sec" << std::endl;
	
	return true;
}
