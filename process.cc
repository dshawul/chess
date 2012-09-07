#include "process.h"
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>

void Process::create(const char *cmd) throw (ProcessErr)
{
	pid = 0;
	int readpipe[2], writepipe[2];
	#define PARENT_READ		readpipe[0]
	#define CHILD_WRITE		readpipe[1]
	#define CHILD_READ		writepipe[0]
	#define PARENT_WRITE	writepipe[1]

	try {
		if (pipe(readpipe) < 0 || pipe(writepipe) < 0)
			throw ProcessErr();

		pid = fork();

		if (pid == 0) {
			// in the child process
			close(PARENT_WRITE);
			close(PARENT_READ);

			if (dup2(CHILD_READ, STDIN_FILENO) == -1)
				throw ProcessErr();
			close(CHILD_READ);

			if (dup2(CHILD_WRITE, STDOUT_FILENO) == -1)
				throw ProcessErr();
			close(CHILD_WRITE);

			if (execlp(cmd, cmd, NULL) == -1)
				throw ProcessErr();
		}
		else if (pid > 0) {
			// in the parent process
			close(CHILD_READ);
			close(CHILD_WRITE);
			
			if ( !(in = fdopen(PARENT_READ, "r")) 
			|| !(out = fdopen(PARENT_WRITE, "w")) )
				throw IOErr();
		}
		else
			// fork failed
			throw ProcessErr();
	}
	catch (ProcessErr &e) {
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

void Process::write_line(const char *s)
{
	fputs(s, out);
	fflush(out);	// don't forget to flush! (that's what she says)
	
	if (ferror(out))
		throw IOErr();
}

void Process::read_line(char *s, int n)
{
	if (!fgets(s, n, in))
		throw IOErr();
}