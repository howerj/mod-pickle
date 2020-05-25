#include "httpc.h"
#include "mod.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/* TODO: Use the httpc module as a generic method of making TCP/IP
 * and SSL connections. And NTP client would be super easy to make,
 * if only the HTTP open/close routines supported UDP. */
typedef struct {
	size_t position, written;
	FILE *output;
} httpc_dump_t;

static httpc_options_t httpc_options = {
	.allocator  = NULL, /* set elsewhere */
	.open       = httpc_open,
	.close      = httpc_close,
	.read       = httpc_read,
	.write      = httpc_write,
	.sleep      = httpc_sleep,
	.logger     = httpc_logger,
	.arena      = NULL,
	.socketopts = NULL,
	.logfile    = NULL, /* set elsewhere */
};

static int pickleCommandSleep(pickle_t *i, int argc, char **argv, void *pd) {
	UNUSED(pd);
	if (argc != 2)
		return error(i, "Invalid command %s: number", argv[0]);
	long n = 0;
	if (sscanf(argv[1], "%ld", &n) != 1)
		return error(i, "Invalid number %s", argv[1]);
	return httpc_sleep(n);
}

static int httpc_dump_cb(void *param, unsigned char *buf, size_t length, size_t position) {
	assert(param);
	assert(buf);
	httpc_dump_t *d = param;
	const size_t l = position + length;
	if (l < position)
		return HTTPC_ERROR;
	d->position = position + length;
	d->written += length;
	if (fwrite(buf, 1, length, d->output) != length)
		return HTTPC_ERROR;
	return HTTPC_OK;
}

static int httpc_put_cb(void *param, unsigned char *buf, size_t length, size_t position) {
	assert(param);
	UNUSED(position);
	FILE *in = param;
	if (length == 0)
		return HTTPC_ERROR;
	const size_t l = fread(buf, 1, length, in);
	if (l < length)
		return ferror(in) ? HTTPC_ERROR : (int)l;
	return l;
}

static int setupOptions(pickle_mod_t *m, httpc_options_t *o) {
	assert(m);
	assert(o);
	*o = httpc_options;
	allocator_fn fn = NULL;
	void *arena = NULL;
	if (pickle_allocator_get(m->i, &fn, &arena) != PICKLE_OK)
		return PICKLE_ERROR;
	o->logfile = stderr;
	o->allocator = fn;
	o->arena = arena;
	o->flags |= HTTPC_OPT_LOGGING_ON;
	//o.flags |= HTTPC_OPT_HTTP_1_0;
	return PICKLE_OK;
}

static int pickleCommandHttpPut(pickle_t *i, int argc, char **argv, void *pd) {
	pickle_mod_t *m = pd;
	httpc_options_t o = { .allocator = NULL };
	if (setupOptions(m, &o) != PICKLE_OK)
		return PICKLE_ERROR;
	if (argc < 3)
		return error(i, "Invalid command %s: URL options...", argv[0]);
	if (argc == 3) {
		errno = 0;
		FILE *f = fopen(argv[2], "rb");
		if (!f)
			return error(i, "Unable to open file '%s' for writing: %s", argv[2], strerror(errno));
		const int r1 = httpc_put(&o, argv[1], httpc_put_cb, f);
		const int r2 = fclose(f);
		if (r1 != HTTPC_OK || r2 < 0)
			return error(i, "failed");
		return ok(i, "ok");
	}
	return error(i, "Invalid subcommand %s", argv[0]);
}

static int pickleCommandHttpGet(pickle_t *i, int argc, char **argv, void *pd) {
	pickle_mod_t *m = pd;
	httpc_options_t o = { .allocator = NULL };
	if (setupOptions(m, &o) != PICKLE_OK)
		return PICKLE_ERROR;
	if (argc < 3)
		return error(i, "Invalid command %s: URL options...", argv[0]);
	if (argc == 3) {
		errno = 0;
		FILE *f = fopen(argv[2], "wb");
		if (!f)
			return error(i, "Unable to open file '%s' for writing: %s", argv[2], strerror(errno));
		httpc_dump_t d = { .position = 0, .output = f };
		const int r1 = httpc_get(&o, argv[1], httpc_dump_cb, &d);
		const int r2 = fclose(f);
		if (r1 != HTTPC_OK || r2 < 0)
			return error(i, "failed");
		return ok(i, "%ld", (long)d.written);
	}
	return error(i, "Invalid subcommand %s", argv[0]);
}

/* TODO: Options for: logging and HTTP 1.0 flags, also save to file */
static int pickleCommandHttpc(pickle_t *i, int argc, char **argv, void *pd) {
	pickle_mod_t *m = pd;
	httpc_options_t o = { .allocator = NULL };
	if (setupOptions(m, &o) != PICKLE_OK)
		return PICKLE_ERROR;
	if (argc < 2)
		return error(i, "Invalid command %s: version *OR* {get|put|delete|head} URL options...", argv[0]);
	if (!strcmp("version", argv[1])) {
		if (argc != 2)
			return error(i, "Invalid subcommand %s", argv[1]);
		unsigned long v = 0;
		if (httpc_version(&v) < 0)
			return error(i, "Invalid version %lu", v);
		return ok(i, "%d %d %d", (int)((v >> 16) & 255),(int)((v >> 8) & 255),(int)((v >> 0) & 255));
	}

	assert(m);
	if (argc < 3)
		return error(i, "Invalid command %s: version *OR* {get|put|delete|head} URL options...", argv[0]);
	if (!strcmp("get", argv[1]))
		return pickleCommandHttpGet(i, argc - 1, argv + 1, pd);
	if (!strcmp("put", argv[1]))
		return pickleCommandHttpPut(i, argc - 1, argv + 1, pd);
	if (!strcmp("delete", argv[1]))
		return httpc_delete(&o, argv[2]) == HTTPC_OK ? ok(i, "ok") : error(i, "failed");
	if (!strcmp("head", argv[1]))
		return httpc_head(&o, argv[2]) == HTTPC_OK ? ok(i, "ok") : error(i, "failed");
	return error(i, "Invalid subcommand: %s", argv[1]);
}

static int cleanup(pickle_mod_t *m, void *tag) {
	UNUSED(m);
	UNUSED(tag);
	return PICKLE_OK;
}	

int pickleModHttpcRegister(pickle_mod_t *m) {
	assert(m);
	pickle_command_t cmds[] = { 
		{ "httpc",  pickleCommandHttpc,  m }, 
		{ "sleep",  pickleCommandSleep,  m }, 
	};
	//httpc_tests(&a)
	m->cleanup = cleanup;
	return pickle_mod_commands_register(m, cmds, NELEMS(cmds));
}

