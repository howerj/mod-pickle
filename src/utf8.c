#include "mod.h"
#include "utf8.h"
#include <assert.h>
#include <string.h>

static int pickleCommandUtf8(pickle_t *i, int argc, char **argv, void *pd) {
	UNUSED(pd);
	if (argc < 3)
		return error(i, "Invalid command %s: utf8 {valid} options...", argv[0]);
	if (!strcmp("valid", argv[1])) {
		size_t count = 0;
		const int r = utf8_code_points(argv[1], &count);
		return ok(i, "%c", r < 0 ? "0" : "1");
	}
	if (!strcmp("codepoints", argv[1])) {
		size_t count = 0;
		const int r = utf8_code_points(argv[2], &count);
		if (r < 0)
			return error(i, "Invalid UTF-8 %s", argv[2]);
		return ok(i, "%lu", (unsigned long)count);
	}
	if (!strcmp("index", argv[1])) {
		if (argc != 4)
			return error(i, "Invalid subcommand %s", argv[1]);
		unsigned long n = 0, codepoint = 0;
		if (sscanf(argv[3], "%lu", &n) != 1)
			return error(i, "Invalid number %s", argv[3]);
		char *s = argv[2];
		for (unsigned long j = 0; j < (n + 1) && *s; j++)
			if (utf8_next(&s, &codepoint) < 0)
				return error(i, "Invalid UTF-8 %s", argv[2]);
		if (*s) {
			char u[8] = { 0 };
			s = u;
			size_t l = sizeof u;
			if (utf8_add(&s, &l, codepoint) < 0)
				return error(i, "Invalid codepoint %lu", codepoint);
			return ok(i, "%s", u);
		}
		return ok(i, "");
	}
	if (!strcmp("ordinal", argv[1])) {
		unsigned long codepoint = 0;
		char *s = argv[2];
		if (utf8_next(&s, &codepoint) < 0)
			return error(i, "Invalid UTF-8 %s", argv[2]);
		return ok(i, "%lu", codepoint);
	}
	if (!strcmp("char", argv[1])) {
		unsigned long codepoint = 0;
		if (sscanf(argv[2], "%lu", &codepoint) != 1)
			return error(i, "Invalid number");
		if (codepoint == 0)
			return ok(i, "");
		char u[8] = { 0 };
		char *s = u;
		size_t l = sizeof u;
		if (utf8_add(&s, &l, codepoint) < 0)
			return error(i, "Invalid codepoint %lu", codepoint);
		return ok(i, "%s", u);
	}
	return error(i, "Invalid subcommand %s", argv[1]);
}

int pickleModUtf8Register(pickle_mod_t *m) {
	assert(m);
	pickle_command_t cmds[] = { { "utf8",  pickleCommandUtf8,  m }, };
	m->cleanup = NULL;
	return pickle_mod_commands_register(m, cmds, NELEMS(cmds));
}

