#pragma once
#include <stdbool.h>
#include <sys/types.h>
#include <stdio.h>

typedef struct {
	pid_t pid;
	FILE *in, *out;
} Process;

extern bool process_create(Process *p, const char *cmd);
extern bool process_destroy(Process *p);