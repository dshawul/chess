#include <cstring>
#include <fstream>
#include "epd.h"

EPD::EPD(const std::string& epd_file)
{
	std::ifstream f(epd_file);

	while (!f.eof()) {
		char s[0x100];
		f.getline(s, sizeof(s));
		strtok(s, ";");
		fen_list.push_back(s);
	}

	count = fen_list.size();
	idx = 0;
}

std::string EPD::next() const
{
	const size_t idx0 = idx;
	idx++;
	idx %= count;

	return fen_list[idx0];
}
