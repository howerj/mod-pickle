#include "mod.h"
#include "sntp.h"
#include <assert.h>

static int pickleCommandSntp(pickle_t *i, int argc, char **argv, void *pd) {
	UNUSED(pd);
	if (argc != 2 && argc != 3)
		return error(i, "Invalid command %s", argv[0]);

	unsigned port = 123;
	if (argc == 3) {
		if (scanf(argv[2], "%u", &port) != 1)
		       return error(i, "Invalid number %s", argv[2]);	
	}
#ifdef _WIN32
	return error(i, "not implemented");
#else
	unsigned long seconds = 0, fractional = 0;
	const int r = sntp(argv[1], port, &seconds, &fractional);
	if (r < 0)
		return error(i, "failed");
	return ok(i, "%lu %lu", seconds, fractional);
#endif
}

static int cleanup(pickle_mod_t *m, void *tag) {
	UNUSED(m);
	UNUSED(tag);
	return PICKLE_OK;
}	

int pickleModSntpRegister(pickle_mod_t *m) {
	assert(m);
	pickle_command_t cmds[] = { 
		{ "sntp",  pickleCommandSntp,  m }, 
	};
	m->cleanup = cleanup;
	return pickle_mod_commands_register(m, cmds, NELEMS(cmds));
}

