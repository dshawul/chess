#include "board.h"
#include "process.h"

int main(int argc, char **argv)
{
	Process P[NB_COLOR];
	char answer[0x100];

	for (unsigned color = White; color <= Black; color++) {
		if (!process_create(&P[color], argv[color+1]))
			goto ProcessError;
		
		fputs("uci\n", P[White].out);
		do {
			if (fgets(answer, sizeof(answer), P[White].in) < 0)
				goto ProcessError;
		}
		while (strcmp(answer, "uciok\n"));
	}

ProcessError:
	process_destroy(&P[White]);
	process_destroy(&P[Black]);
}
