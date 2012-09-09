/* Engine class:
 * - an Engine is a Process
 * - that complies with the UCI protocol
 * - and stores UCI options
 * */
#pragma once
#include <set>
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

		bool operator < (const Option& o) const;
	};

	virtual void create(const char *cmd) throw (Err);
	void set_option(const std::string& name, Option::Type type, int value) throw (Option::Err);
	void set_position(const std::string& fen, const std::string& moves) const throw (IOErr);

private:
	std::set<Option> options;
	std::string engine_name;
	
	void sync() const throw (IOErr);
};

inline bool Engine::Option::operator < (const Option& o) const
{
	return type < o.type || name < o.name;
}
