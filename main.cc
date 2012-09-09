#include <iostream>
#include "types.h"
#include "match.h"

int main(int argc, char **argv)
{
	init_bitboard();
	
	Engine E[NB_COLOR];
	E[White].create(argv[1]);
	E[Black].create(argv[2]);
	
	int result = match(E, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	std::cout << result << std::endl;
}