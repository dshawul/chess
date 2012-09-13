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
#include "engine.h"
#include <sstream>
#include <iostream>
#include <string.h>

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

				options.insert(o);
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

				options.insert(o);
			}
		}
	}
	while (token != "uciok");
}

Engine::~Engine()
{}

void Engine::set_option(const std::string& name, Option::Type type, int value) throw (Option::Err)
{
	Option o;
	o.type = type;
	o.name = name;

	auto it = options.find(o);

	if (it != options.end())
	{
		o = *it;
		if (o.min <= value && value <= o.max)
		{
			o.value = value;
			options.erase(o);
			options.insert(o);
		}
		else
			throw Option::OutOfBounds();
	}
	else
		throw Option::NotFound();
}

void Engine::sync() const throw (IOErr)
/* Send "isready" to the engine, and wait for "readyok". This is used often by the UCI protocol to
 * synchronize the Interface and the Engine */
{
	write_line("isready\n");
	char line[LineSize];
	do
	{
		read_line(line, sizeof(line));
	}
	while (strcmp(line, "readyok\n"));
}

void Engine::set_position(const std::string& fen, const std::string& moves) const throw (IOErr)
{
	std::string s("position ");

	if (fen != "startpos")
		s += "fen ";
	s += fen;

	if (!moves.empty())
		s += " moves " + moves;

	s += '\n';
	write_line(s.c_str());

	sync();
}

std::string Engine::search(const SearchParam& sp) const throw (IOErr)
{
	std::ostringstream s;
	s << "go";
	
	if (sp.wtime || sp.winc || sp.btime || sp.binc) {
		s << " wtime " << sp.wtime << " winc " << sp.winc;
		s << " btime " << sp.btime << " binc " << sp.binc;
	}
	if (sp.movetime)
		s << " movetime " << sp.movetime;
	if (sp.depth)
		s << " depth " << sp.depth;
	if (sp.nodes)
		s << " nodes " << sp.depth;
	
	s << '\n';
	
	write_line(std::string(s.str()).c_str());
	char line[LineSize];

	for (;;)
	{
		read_line(line, sizeof(line));

		std::istringstream parser(line);
		std::string token;

		if (parser >> token
		        && token == "bestmove"
		        && parser >> token)
			return token;
	}
}
