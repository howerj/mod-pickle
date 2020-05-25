#include "mod.h"
#include "utf8.h"
#include <assert.h>

static int pickleCommandUtf8(pickle_t *i, int argc, char **argv, void *pd) {
	UNUSED(pd);
	if (argc < 2)
		return error(i, "Invalid command %s: utf8 { } options..", argv[0]);
	return PICKLE_OK;
}

int pickleModUtf8Register(pickle_mod_t *m) {
	assert(m);
	pickle_command_t cmds[] = { { "utf8",  pickleCommandUtf8,  m }, };
	m->cleanup = NULL;
	return pickle_mod_commands_register(m, cmds, NELEMS(cmds));
}

