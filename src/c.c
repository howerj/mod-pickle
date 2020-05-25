#include "mod.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int pickleCommandFile(pickle_t *i, int argc, char **argv, void *pd) {
	UNUSED(pd);
	if (argc < 2)
		return error(i, "Invalid command %s", argv[0]);
	if (!strcmp("rename", argv[1])) {
		if (argc != 4)
			return error(i, "Invalid command %s: expected oldname newname", argv[1]);
		errno = 0;
		if (rename(argv[2], argv[3]) < 0)
			return error(i, "rename '%s' -> '%s' failed: %s", argv[2], argv[3], strerror(errno));
		return PICKLE_OK;
	}
	if (!strcmp("delete", argv[1])) {
		if (argc < 3)
			return error(i, "Invalid command %s: expected filenames...", argv[2]);
		for (int j = 2; j < argc; j++)
			if (remove(argv[j]) < 0)
				return error(i, "removed '%s' failed: %s", argv[2], strerror(errno));
		return PICKLE_OK;
	}
	return error(i, "Invalid subcommand %s", argv[1]);
}

static int pickleCommandGets(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 1)
		return error(i, "Invalid command %s", argv[0]);
	size_t length = 0;
	char *line = pickle_slurp(i, (FILE*)pd, &length, "\n");
	if (!line)
		return error(i, "Out Of Memory");
	if (!length) {
		if (pickle_free(i, line) != PICKLE_OK)
			return PICKLE_ERROR;
		if (ok(i, "EOF") != PICKLE_OK)
			return PICKLE_ERROR;
		return PICKLE_BREAK;
	}
	const int r = ok(i, "%s", line);
	return pickle_free(i, line) == PICKLE_OK ? r : PICKLE_ERROR;
}

static int pickleCommandPuts(pickle_t *i, int argc, char **argv, void *pd) {
	FILE *out = pd;
	if (argc != 1 && argc != 2 && argc != 3)
		return error(i, "Invalid command %s", argv[0]);
	if (argc == 1)
		return fputc('\n', out) < 0 ? PICKLE_ERROR : PICKLE_OK;
	if (argc == 2)
		return fprintf(out, "%s\n", argv[1]) < 0 ? PICKLE_ERROR : PICKLE_OK;
	if (!strcmp(argv[1], "-nonewline"))
		return fputs(argv[2], out) < 0 ? PICKLE_ERROR : PICKLE_OK;
	return error(i, "Invalid option %s", argv[1]);
}

static int pickleCommandGetEnv(pickle_t *i, int argc, char **argv, void *pd) {
	UNUSED(pd);
	if (argc != 2)
		return error(i, "Invalid command %s", argv[0]);
	const char *env = getenv(argv[1]);
	return ok(i, "%s", env ? env : "");
}

static int pickleCommandExit(pickle_t *i, int argc, char **argv, void *pd) {
	UNUSED(pd);
	if (argc != 2 && argc != 1)
		return error(i, "Invalid command %s", argv[0]);
	const char *code = argc == 2 ? argv[1] : "0";
	exit(atoi(code));
	return PICKLE_ERROR; /* unreachable */
}

static int pickleCommandClock(pickle_t *i, const int argc, char **argv, void *pd) {
	UNUSED(pd);
	time_t ts = 0;
	if (argc < 2)
		return error(i, "Invalid command %s", argv[0]);
	if (!strcmp(argv[1], "clicks")) {
		const long t = (((double)(clock()) / (double)CLOCKS_PER_SEC) * 1000.0);
		return ok(i, "%ld", t);
	}
	if (!strcmp(argv[1], "seconds"))
		return ok(i, "%ld", (long)time(&ts));
	if (!strcmp(argv[1], "format")) {
		const int gmt = 1;
		char buf[512] = { 0 };
		char *fmt = argc == 4 ? argv[3] : "%a %b %d %H:%M:%S %Z %Y";
		int tv = 0;
		if (argc != 3 && argc != 4)
			return error(i, "Invalid subcommand %s", argv[1]);
		if (sscanf(argv[2], "%d", &tv) != 1)
			return error(i, "Invalid number %s", argv[2]);
		ts = tv;
		struct tm *timeinfo = (gmt ? gmtime : localtime)(&ts);
		strftime(buf, sizeof buf, fmt, timeinfo);
		return ok(i, "%s", buf);
	}
	return error(i, "Invalid command %s", argv[0]);
}

static int cleanup(pickle_mod_t *m, void *tag) {
	UNUSED(m);
	UNUSED(tag);
	return PICKLE_OK;
}	

int pickleModCRegister(pickle_mod_t *m) {
	assert(m);
	m->cleanup = cleanup;
	pickle_command_t cmds[] = { 
		{ "gets",    pickleCommandGets,    stdin }, 
		{ "puts",    pickleCommandPuts,    stdout }, 
		{ "getenv",  pickleCommandGetEnv,  m }, 
		{ "clock",   pickleCommandClock,   m }, 
		{ "exit",    pickleCommandExit,    m }, 
		{ "file",    pickleCommandFile,    m }, 
	};
	return pickle_mod_commands_register(m, cmds, NELEMS(cmds));
}

