#pragma once
#include <stdbool.h>
#include <sys/types.h>
#include <stdio.h>

struct ProcessErr {};
struct IOErr: ProcessErr {};

class Process {
	pid_t pid;
	FILE *in, *out;
	void cleanup();	
public:
	Process(): pid(0), in(NULL), out(NULL) {}
	~Process();
	
	void create(const char *cmd) throw (ProcessErr);
	void read_line(char *buf, int n) const;
	void write_line(char *buf) const;
};