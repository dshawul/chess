#include "engine.h"
#include <sstream>
#include <iostream>

void Engine::create(const char *cmd) throw (ProcessErr)
// Process the uci ... uciok tasks (parse option and engine name)
{
	Process::create(cmd);
	write_line("uci\n");

	char line[0x100];
	std::string token;
	do {
		read_line(line, sizeof(line));
		Option o;
		
		std::istringstream s(line);
		if (!(s >> token)) throw SyntaxErr();
		
		if (token == "id") {
			if ((s >> token) && (token == "name")) {
				while (s >> token)
					name += token + " ";
			}
		}
		else if (token == "option") {
			// option ...
			if ((s >> token) && token == "name") {
				// option name ...
				while ((s >> token) && token != "type")
					o.name += token + " ";
				s >> token;
				if (token == "check" || token == "spin") {
					o.type = token == "check" ? Option::Boolean : Option::Integer;
					// option name o.name type o.type ...					
					if (!(s >> token) || token != "default")
						throw SyntaxErr();					
					// option name o.name type check default ...
					if (o.type == Option::Boolean) {
						s >> token;
						o.value = token == "true" ? true : false;
						o.min = false; o.max = true;	// for the sake of consistency
					}
					else if (o.type == Option::Integer) {
						s >> o.value;
						if (!(s >> token) || token != "min")
							throw SyntaxErr();
						// option name o.name type spin default o.value min ...
						s >> o.min;
						if (!(s >> token) || token != "max")
							throw SyntaxErr();
						// option name o.name type spin default o.value min o.min max ...
						s >> o.max;						
					}
				}
			}
		}
	}
	while (token != "uciok");
}

Engine::~Engine()
{
	/* Nothing to do */
}
