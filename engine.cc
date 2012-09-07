#include "engine.h"
#include <sstream>
#include <string.h>

void Engine::create(const char *cmd) throw (ProcessErr)
// Process the uci ... uciok tasks (parse option and engine name)
{
	Process::create(cmd);
	write_line("uci\n");

	char line[0x100], s[0x10];
	Option o;
	int n;

	do {
		read_line(line, sizeof(line));

		// try to recognize the engine name
		n = sscanf(line, "id name %s\n", s);
		if (n == 1) {
			strcpy(name, s);
			continue;
		}

		// try to recognize an integer option
		o.type = Option::Integer;
		n = sscanf(line, "option name %s type spin default %d min %d max %d\n",
			o.name, &o.value, &o.min, &o.max);
		if (n == 4) goto recognized;

		// try to recognize a boolean option
		o.type = Option::Boolean;
		o.min = false;
		o.max = true;
		n = sscanf(line, "option name %s type check default %s\n", o.name, s);
		o.value = strcmp(s, "true") ? false : true;
		if (n != 2) continue;	// option not recognized, discard

recognized:
		options.push_back(o);
	}
	while (strcmp(line, "uciok\n"));
}

Engine::~Engine()
{
	/* Nothing to do */
}
