/* Engine class:
 * - an Engine is a Process
 * - that complies with the UCI protocol
 * - and stores UCI options
 * */
#pragma once
#include <vector>
#include <string>
#include "process.h"

class Engine: public Process
{
public:
	Engine(): Process() {}
	virtual void create(const char *cmd) throw (ProcessErr);
	virtual ~Engine();
private:
	struct Option {
		enum Type { Boolean, Integer, String };		
		Type type;
		std::string name, str_value;
		int int_value, min, max;	// when type == Integer
	};
	std::vector<Option> options;
	std::string name;
};
