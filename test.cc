#include "test.h"
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
		{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, 119060324ULL},
		{"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 5, 193690690ULL},
		{"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 7, 178633661ULL},
		{"r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 6, 706045033ULL},
		{NULL, 0, 0}
	};

	uint64_t total = 0;
	clock_t start = clock();

	for (unsigned i = 0; Test[i].s; i++)
	{
		printf("%s\n", Test[i].s);
		set_fen(&B, Test[i].s);
		if (perft(&B, Test[i].depth, 0) != Test[i].value)
		{
			perror("perft: ERROR");
			return false;
		}
		total += Test[i].value;
	}

	double elapsed = (clock() - start) / CLOCKS_PER_SEC;

	puts("perft: OK");
	printf("speed: %u leaf/sec\n", (unsigned)(total / elapsed));
	return true;
}
