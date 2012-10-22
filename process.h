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
#pragma once
#include <stdbool.h>
#include <sys/types.h>
#include <cstdio>

class Process
{
public:
	struct Err {};
	struct IOErr: Err {};

	Process(): pid(0), in(NULL), out(NULL) {}
	virtual void create(const char *cmd) throw (Err);
	virtual ~Process();

protected:
	pid_t pid;
	FILE *in, *out;
	static const int LineSize = 0x100;

	void cleanup();
	void write_line(const char *s) const throw(IOErr);
	void read_line(char *s, int n) const throw(IOErr);
};
