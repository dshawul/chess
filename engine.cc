#include "engine.h"
#include <sstream>
#include <iostream>

void Engine::create(const char *cmd) throw (Err)
// Process the uci ... uciok tasks (parse option and engine name)
{
	Process::create(cmd);
	
	char line[0x100];
	std::string token;
	write_line("uci\n");

	do
	{
		read_line(line, sizeof(line));
		std::istringstream s(line);
		s >> token;

		if (token == "id")
		{
			if ((s >> token) && token == "name")
			{
				while (s >> token)
					engine_name += std::string(" ", !engine_name.empty()) + token;

				if (engine_name.empty())
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
				o.max = true;
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

			options.insert(o);
		}
	} while (token != "uciok");
}

Engine::~Engine()
{}

void Engine::set_option(const std::string& name, Option::Type type, int value) throw (Option::Err)
{
	Option o;
	o.name = name;
	o.type = type;
	
	auto i = options.find(o);
	if (i != options.end()) {
		if (i->min <= value && value <= i->max) {
			Option copy = *i;
			copy.value = value;
			options.erase(o);
			options.insert(copy);
		}
		else
			throw Option::OutOfBounds();
	}
	else
		throw Option::NotFound();
}