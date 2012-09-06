#include "process.h"
#include <assert.h>
#include <unistd.h>
#include <signal.h>

bool process_create(Process *p, const char *cmd)
{
	p->pid = 0;
	p->in = p->out = NULL;

	int readpipe[2], writepipe[2];
#define PARENT_READ		readpipe[0]
#define CHILD_WRITE		readpipe[1]
#define CHILD_READ		writepipe[0]
#define PARENT_WRITE	writepipe[1]

	if (pipe(readpipe) < 0 || pipe(writepipe) < 0)
		return false;

	p->pid = fork();

	if (p->pid == 0) {
		// in the child process
		close(PARENT_WRITE);
		close(PARENT_READ);

		if (dup2(CHILD_READ, STDIN_FILENO) == -1)
			return false;
		close(CHILD_READ);

		if (dup2(CHILD_WRITE, STDOUT_FILENO) == -1)
			return false;
		close(CHILD_WRITE);

		if (execlp(cmd, cmd, NULL) == -1)
			return false;
	}
	else if (p->pid > 0) {
		// in the parent process
		close(CHILD_READ);
		close(CHILD_WRITE);

		if (!(p->in = fdopen(PARENT_READ, "r")))
			return false;

		if (!(p->out = fdopen(PARENT_WRITE, "w")))
			return false;

		setvbuf(p->in, NULL, _IONBF, 0);
		setvbuf(p->out, NULL, _IONBF, 0);
	}
	else
		// fork failed
		return false;

	return true;
}

bool process_destroy(Process *p)
{
	// close the parent side of the pipes
	if (p->in)
		fclose(p->in);
	if (p->out)
		fclose(p->out);

	// kill the child process
	return p->pid > 0
	       ? kill(p->pid, SIGKILL) >= 0
	       : false;
}
