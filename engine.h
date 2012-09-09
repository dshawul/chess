/* Engine class:
 * - an Engine is a Process
 * - that complies with the UCI protocol
 * - and stores UCI options
 * */
#pragma once
#include <map>
#include <string>
#include "process.h"

class Engine: public Process
{
public:
	Engine(): Process() {}
	virtual ~Engine();

	struct SyntaxErr: Err {};

	struct Option
	{
		struct Err {};
		struct NotFound: Err {};
		struct OutOfBounds: Err {};

		enum Type { Boolean, Integer };
		Type type;
		std::string name;
		int value, min, max;

		typedef std::pair<Type, std::string> Key;
	};

	struct SearchParam
	{
		SearchParam();
		int wtime, winc, btime, binc, movetime;
		unsigned depth, nodes;
	};

	virtual void create(const char *cmd) throw (Err);
	void set_option(const std::string& name, Option::Type type, int value) throw (Option::Err);
	void set_position(const std::string& fen, const std::string& moves) const throw (IOErr);
	std::string search(const SearchParam& sp) const throw (IOErr);

private:
	std::map<Option::Key, Option> options;
	std::string engine_name;

	void sync() const throw (IOErr);
};

inline Engine::SearchParam::SearchParam():
	wtime(0), winc(0), btime(0), binc(0),
	movetime(0), depth(0), nodes(0)
{}
