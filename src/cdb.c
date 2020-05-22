#include "cdb.h"
#include "mod.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static cdb_word_t cdb_read_cb(void *file, void *buf, size_t length) {
	assert(file);
	assert(buf);
	return fread(buf, 1, length, file);
}

static cdb_word_t cdb_write_cb(void *file, void *buf, size_t length) {
	assert(file);
	assert(buf);
	return fwrite(buf, 1, length, file);
}

static int cdb_seek_cb(void *file, long offset) {
	assert(file);
	return fseek(file, offset, SEEK_SET);
}

static void *cdb_open_cb(const char *name, int mode) {
	assert(name);
	assert(mode == CDB_RO_MODE || mode == CDB_RW_MODE);
	const char *mode_string = mode == CDB_RW_MODE ? "wb+" : "rb";
	return fopen(name, mode_string);
}

static int cdb_close_cb(void *file) {
	assert(file);
	const int r = fclose(file);
	return r;
}

static int cdb_flush_cb(void *file) {
	assert(file);
	return fflush(file);
}

int pickleCommandCdbOpen(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc < 3)
		return error(i, "Invalid command %s: expected file w/r", argv[0]);
	static cdb_options_t ops = {
		.allocator = pickle_mod_allocator,
		.hash      = NULL,
		.compare   = NULL,
		.read      = cdb_read_cb,
		.write     = cdb_write_cb,
		.seek      = cdb_seek_cb,
		.open      = cdb_open_cb,
		.close     = cdb_close_cb,
		.flush     = cdb_flush_cb,
		.arena     = NULL,
		.offset    = 0,
		.size      = 0, /* auto-select */
	};

	int creating = 0;
	if (!strcmp(argv[2], "w"))
		creating = 1;
	else if (!strcmp(argv[2], "r"))
		return error(i, "Invalid mode to %s: %s", argv[0], argv[2]);

	cdb_t *cdb = NULL;
	errno = 0;
	if (cdb_open(&cdb, &ops, creating, argv[1]) < 0) {
		const char *f = errno ? strerror(errno) : "unknown";
		const char *m = creating ? "create" : "read";
		return error(i, "Opening file '%s' in %s mode failed: %s", argv[1], f, m);
	}
	return pickle_mod_tag_add(pd, argv[1], cdb);
}

static int pickleCommandCdbClose(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 2)
		return error(i, "Invalid command %s: expected cdb file name");
	return pickle_mod_tag_remove(pd, argv[1]);
}

static int pickleCommandCdbRead(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc < 3)
		return error(i, "Invalid command %s: expected cdb file name");
	cdb_t *cdb = pickle_mod_tag_find(pd, argv[1]);
	if (!cdb)
		return error(i, "Invalid cdb file name %s", argv[1]);
	return PICKLE_OK;
}

static int pickleCommandCdbWrite(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 2)
		return error(i, "Invalid command %s: expected cdb file name");
	cdb_t *cdb = pickle_mod_tag_find(pd, argv[1]);
	if (!cdb)
		return error(i, "Invalid cdb file name %s", argv[1]);
	return PICKLE_OK;
}

static int cleanup(pickle_mod_t *m, void *tag) {
	assert(m);
	assert(tag);
	return cdb_close(tag) < 0 ? PICKLE_ERROR : PICKLE_OK;
}	

int pickleModCdbRegister(pickle_mod_t *m) {
	assert(m);
	pickle_command_t cmds[] = {
		{ "cdb.open",  pickleCommandCdbOpen,  m },
		{ "cdb.close", pickleCommandCdbClose, m },
		{ "cdb.read",  pickleCommandCdbRead,  m },
		{ "cdb.write", pickleCommandCdbWrite, m },
	};
	m->cleanup = cleanup;
	return pickle_mod_commands_register(m, cmds, NELEMS(cmds));
}
