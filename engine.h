/* Engine class:
 * - an Engine is a Process
 * - that complies with the UCI protocol
 * - and stores UCI options
 * */
#pragma once
#include <vector>
#include <string>
#include "process.h"

struct SyntaxErr: ProcessErr {};

class Engine: public Process
{
public:
	Engine(): Process() {}
	virtual void create(const char *cmd) throw (ProcessErr);
	virtual ~Engine();
private:
	struct Option {
		enum Type { Boolean, Integer };
		Type type;
		char name[0x20];
		int value, min, max;
	};
	std::vector<Option> options;
	char name[0x20];
};
