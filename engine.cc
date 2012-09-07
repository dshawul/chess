#include "engine.h"

void Engine::create(const char *cmd) throw (ProcessErr)
/* TODO:
 * - say "uci\n"
 * - read UCI options (and store in a TBD struct using some STL magic)
 * - until "uciok\n" is received
 * - Note: need a timeout version of read_line(), in order to detect non UCI process
 * */
{
	Process::create(cmd);
	/* Example of conversation Interface <-> Process:
	* -> uci
	* <- id name DiscoCheck 3.7.1
	* <- id author Lucas Braesch
	* <- option name Hash type spin default 32 min 1 max 8192
	* <- option name Verbose type check default true
	* <- option name AutoClearHash type check default false
	* <- option name TimeBuffer type spin default 100 min 0 max 5000
	* <- uciok
	* Set the values of Engine::options accordingly
	* */
}

Engine::~Engine()
/* TODO:
 * - free options
 * - call Process::~Process()
 * */
{}