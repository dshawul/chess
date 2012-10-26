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
#include "process.h"
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <iostream>

void Process::run(const char *cmd) throw (Err)
/*
 * Spawn a child process, and executes cmd. On success:
 * - pid is the process id
 * - in, out are FILE* to read/write from/to the process' stdout/stdin
 * */
{
	pid = 0;
	int readpipe[2], writepipe[2];
#define PARENT_READ		readpipe[0]
#define CHILD_WRITE		readpipe[1]
#define CHILD_READ		writepipe[0]
#define PARENT_WRITE	writepipe[1]

	try
	{
		if (pipe(readpipe) < 0 || pipe(writepipe) < 0)
			throw Err();

		pid = fork();

		if (pid == 0)
		{
			// in the child process
			close(PARENT_WRITE);
			close(PARENT_READ);

			if (dup2(CHILD_READ, STDIN_FILENO) == -1)
				throw Err();
			close(CHILD_READ);

			if (dup2(CHILD_WRITE, STDOUT_FILENO) == -1)
				throw Err();
			close(CHILD_WRITE);

			if (execlp(cmd, cmd, NULL) == -1)
				throw Err();
		}
		else if (pid > 0)
		{
			// in the parent process
			close(CHILD_READ);
			close(CHILD_WRITE);

			if ( !(in = fdopen(PARENT_READ, "r"))
			        || !(out = fdopen(PARENT_WRITE, "w")) )
				throw IOErr();
		}
		else
			// fork failed
			throw Err();
	}
	catch (Err &e)
	{
		cleanup();
		throw;
	}
}

void Process::cleanup()
{
	// close file descriptors
	if (in) fclose(in);
	if (out) fclose(out);

	// kill child process
	if (pid > 0) kill(pid, SIGKILL);
}

Process::~Process()
{
	cleanup();
}

void Process::write_line(const char *s) const throw(IOErr)
{
	fputs(s, out);
	fflush(out);	// don't forget to flush! (that's what she says)

	if (ferror(out))
		throw IOErr();
}

void Process::read_line(char *s, int n) const throw(IOErr)
{
	if (!fgets(s, n, in))
		throw IOErr();
}
