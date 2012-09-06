#include "board.h"
#include "process.h"

int main(int argc, char **argv)
{
	Process P[NB_COLOR];
	char buf[0x100];

	for (unsigned color = White; color <= Black; color++) {
		P[color].create(argv[color+1]);
		
		P[color].write_line("uci\n");
		do {
			P[color].read_line(buf, sizeof(buf));
		}
		while (strcmp(buf, "uciok\n"));
	}
}
