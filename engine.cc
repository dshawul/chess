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
#include <cstring>
#include <chrono>

bool Option::operator< (const Option& o) const
{
	return type < o.type && name < o.name;
}

void Engine::parse_option(std::istringstream& s) throw (Err)
{
	Option o;
	std::string token;

	if (!(s >> token) || token != "name")
		throw SyntaxErr();

	while ((s >> token) && token != "type")
		o.name += std::string(" ", !o.name.empty()) + token;

	if (o.name.empty())
		throw SyntaxErr();

	if (!(s >> token))
		throw SyntaxErr();

	if (token == "check") {
		o.type = Option::Boolean;

		if (!(	(s >> token) && token == "default"
		        && (s >> token) && (token == "true" || token == "false")) )
			throw SyntaxErr();

		o.value = token == "true";
		o.min = false;
		o.max = true;

		options.insert(o);
	} else if (token == "spin") {
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

void Engine::parse_id(std::istringstream& s) throw (Err)
{
	std::string token;

	if ((s >> token) && token == "name") {
		while (s >> token)
			engine_name += std::string(" ", !engine_name.empty()) + token;

		if (engine_name.empty())
			throw SyntaxErr();
	}
}

void Engine::create(const char *cmd) throw (Process::Err, Err)
// Process the uci ... uciok tasks (parse option and engine name)
{
	p.run(cmd);

	char line[0x100];
	std::string token;
	p.write_line("uci\n");

	do {
		p.read_line(line, sizeof(line));
		std::istringstream s(line);
		s >> token;

		if (token == "id")
			parse_id(s);
		else if (token == "option")
			parse_option(s);
	} while (token != "uciok");
}

void Engine::set_option(const std::string& name, Option::Type type, int value) throw (Option::Err)
{
	Option o;
	o.type = type;
	o.name = name;

	auto it = options.find(o);

	if (it != options.end()) {
		o = *it;
		if (o.min <= value && value <= o.max) {
			o.value = value;
			options.erase(o);
			options.insert(o);
		} else
			throw Option::OutOfBounds();
	} else
		throw Option::NotFound();
}

void Engine::sync() const throw (Process::Err)
/* Send "isready" to the engine, and wait for "readyok". This is used often by the UCI protocol to
 * synchronize the Interface and the Engine */
{
	p.write_line("isready\n");
	char line[LineSize];
	do {
		p.read_line(line, sizeof(line));
	} while (strcmp(line, "readyok\n"));
}

void Engine::set_position(const std::string& fen, const std::string& moves) const throw (Process::Err)
{
	std::string s("position ");

	if (fen != "startpos")
		s += "fen ";
	s += fen;

	if (!moves.empty())
		s += " moves " + moves;

	s += '\n';
	p.write_line(s.c_str());

	sync();
}

void Engine::parse_info(std::istringstream& s, SearchResult& result) const
{
	std::string token;

	while (s >> token) {
		if (token == "score" && s >> token && token == "cp")
			s >> result.score;
		else if (token == "depth")
			s >> result.depth;
		else if (token == "pv")
			break;
	}
}

SearchResult Engine::search(Color color) const throw (Process::Err, Clock::Err)
{
	SearchResult result;

	// start chrono
	auto start = std::chrono::system_clock::now();

	// send go command
	p.write_line(clk.uci_str(color).c_str());

	char line[LineSize];
	for (;;) {
		p.read_line(line, sizeof(line));

		std::istringstream s(line);
		std::string token;
		s >> token;

		if (token == "bestmove" && s >> token) {
			// stop chrono
			auto stop = std::chrono::system_clock::now();

			result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>
			                 (stop - start).count();
			clk.consume(result.elapsed);

			result.bestmove = token;
			return result;
		} else if (token == "info")
			parse_info(s, result);
	}
}
