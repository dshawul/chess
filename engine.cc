#include "engine.h"
#include <sstream>
#include <iostream>

void Engine::create(const char *cmd) throw (ProcessErr)
// Process the uci ... uciok tasks (parse option and engine name)
{
	Process::create(cmd);
	write_line("uci\n");

	char line[0x100];
	for (;;)
	{
		read_line(line, sizeof(line));

		std::istringstream s(line);
		std::string token;

		if (!(s >> token))
			throw SyntaxErr();

		if (token == "uciok")
			break;

		else if (token == "id")
		{
			if ((s >> token) && token == "name")
			{
				while (s >> token)
					name += std::string(" ", !name.empty()) + token;

				if (name.empty())
					throw SyntaxErr();
			}
		}
		else if (token == "option")
		{
			Option o;

			if (!(s >> token) || token != "name")
				throw SyntaxErr();

			while ((s >> token) && token != "type")
				o.name += std::string(" ", !o.name.empty()) + token;

			if (o.name.empty())
				throw SyntaxErr();

			if (!(s >> token))
				throw SyntaxErr();

			if (token == "check")
			{
				o.type = Option::Boolean;

				if (!(	(s >> token) && token == "default"
				        && (s >> token) && (token == "true" || token == "false")) )
					throw SyntaxErr();

				o.value = token == "true";
				o.min = false;
				o.max = true; // for the sake of consistency
			}
			else if (token == "spin")
			{
				o.type = Option::Integer;

				if (!(	(s >> token) && token == "default"
				        && (s >> o.value)
				        && (s >> token) && token == "min" && (s >> o.min)
				        && (s >> token) && token == "max" && (s >> o.max)) )
					throw SyntaxErr();

				if ((o.min > o.value) || (o.max < o.value))
					throw SyntaxErr();
			}
			else if (token != "string" && token != "combo")
				throw SyntaxErr();

			options.push_back(o);
		}
		else
			throw SyntaxErr();
	}
}

Engine::~Engine()
{
	/* Nothing to do */
}
