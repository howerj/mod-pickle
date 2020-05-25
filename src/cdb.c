#include "cdb.h"
#include "mod.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

typedef struct {
	unsigned long records;
	unsigned long total_key_length, total_value_length;
	unsigned long min_key_length, min_value_length;
	unsigned long max_key_length, max_value_length;
	unsigned long hash_start;
} cdb_statistics_t;

typedef struct {
	int creating;
	cdb_t *cdb;
} cdb_wrapper_t;

static int cdb_stats(cdb_t *cdb, const cdb_file_pos_t *key, const cdb_file_pos_t *value, void *param) {
	assert(cdb);
	assert(key);
	assert(value);
	assert(param);
	cdb_statistics_t *cs = param;
	cs->records++;
	cs->total_key_length   += key->length;
	cs->total_value_length += value->length;
	cs->min_key_length      = MIN(cs->min_key_length,   key->length);
	cs->min_value_length    = MIN(cs->min_value_length, value->length);
	cs->max_key_length      = MAX(cs->max_key_length,   key->length);
	cs->max_value_length    = MAX(cs->max_value_length, value->length);
	return 0;
}

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

	allocator_fn fn = NULL;
	void *arena = NULL;
	if (pickle_allocator_get(i, &fn, &arena) < 0)
		return PICKLE_ERROR;

	cdb_options_t ops = {
		.allocator = fn,
		.hash      = NULL, /* use defaults */
		.compare   = NULL, /* use defaults */
		.read      = cdb_read_cb,
		.write     = cdb_write_cb,
		.seek      = cdb_seek_cb,
		.open      = cdb_open_cb,
		.close     = cdb_close_cb,
		.flush     = cdb_flush_cb,
		.arena     = arena,
		.offset    = 0,
		.size      = 0, /* auto-select */
	};

	int creating = 0;
	if (!strcmp(argv[2], "w"))
		creating = 1;
	else if (strcmp(argv[2], "r"))
		return error(i, "Invalid mode to %s: %s", argv[0], argv[2]);

	cdb_wrapper_t *w = pickle_allocate(i, sizeof *w);
	if (!w)
		return PICKLE_ERROR;
	w->creating = creating;
	w->cdb = NULL;
	errno = 0;
	if (cdb_open(&w->cdb, &ops, creating, argv[1]) < 0) {
		const char *f = errno ? strerror(errno) : "unknown";
		const char *m = creating ? "create" : "read";
		(void)pickle_free(i, w);
		return error(i, "Opening file '%s' in %s mode failed: %s", argv[1], m, f);
	}
	char p[64] = { 0 };
	if (sprintf(p, "%p", w->cdb) < 0) {
		(void)cdb_close(w->cdb);
		(void)pickle_free(i, w);
		return error(i, "sprint failed");
	}
	if (pickle_mod_tag_add(pd, p, w) < 0) {
		(void)cdb_close(w->cdb);
		(void)pickle_free(i, w);
		return error(i, "failed to add tag %s", p);
	}
	return ok(i, "%s", p);
}

static int pickleCommandCdbClose(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 2)
		return error(i, "Invalid subcommand %s: expected cdb-handle", argv[0]);
	return pickle_mod_tag_remove(pd, argv[1]) != PICKLE_OK ?
		error(i, "Invalid cdb file handle", argv[1]) : PICKLE_OK;
}

static int pickleCommandCdbExists(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 3 && argc != 4)
		return error(i, "Invalid subcommand %s: expected cdb-handle key number?", argv[0]);
	cdb_wrapper_t *w = pickle_mod_tag_find(pd, argv[1]);
	if (!w || !(w->cdb))
		return error(i, "Invalid cdb file handle %s", argv[1]);
	long record = 0;
	if (argc == 4)
		if (sscanf(argv[3], "%ld", &record) != 1)
			return error(i, "Invalid number %s", argv[3]);
	const cdb_buffer_t kb = { .length = strlen(argv[2]), .buffer = argv[2] };
	cdb_file_pos_t vp = { 0, 0 };
	const int gr = cdb_lookup(w->cdb, &kb, &vp, record);
	if (gr < 0)
		return error(i, "Invalid cdb database");
	return ok(i, "%c", gr == 0 ? "0" : "1");
}

static int pickleCommandCdbCount(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 3)
		return error(i, "Invalid subcommand %s: expected cdb-handle key", argv[0]);
	cdb_wrapper_t *w = pickle_mod_tag_find(pd, argv[1]);
	if (!w || !(w->cdb))
		return error(i, "Invalid cdb file handle %s", argv[1]);
	long record = 0;
	const cdb_buffer_t kb = { .length = strlen(argv[2]), .buffer = argv[2] };
	const int gr = cdb_count(w->cdb, &kb, &record);
	if (gr < 0)
		return error(i, "Invalid cdb database");
	return ok(i, "%ld", record);
}

static int pickleCommandCdbRead(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 3 && argc != 4)
		return error(i, "Invalid subcommand %s: expected cdb-handle key number?", argv[0]);
	cdb_wrapper_t *w = pickle_mod_tag_find(pd, argv[1]);
	if (!w || !(w->cdb))
		return error(i, "Invalid cdb file handle %s", argv[1]);
	long record = 0;
	if (argc == 4)
		if (sscanf(argv[3], "%ld", &record) != 1)
			return error(i, "Invalid number %s", argv[3]);
	const cdb_buffer_t kb = { .length = strlen(argv[2]), .buffer = argv[2] };
	cdb_file_pos_t vp = { 0, 0 };
	const int gr = cdb_lookup(w->cdb, &kb, &vp, record);
	if (gr < 0)
		return error(i, "Invalid cdb database");
	if (gr == 0)
		return error(i, "Key not found %s", argv[2]);
	char *v = pickle_allocate(i, vp.length + 1);
	if (!v)
		return PICKLE_ERROR;
	int r = PICKLE_ERROR;
	v[vp.length] = '\0';
	if (cdb_seek(w->cdb, vp.position) < 0)
		goto done;
	if (cdb_read(w->cdb, v, vp.length) < 0)
		goto done;
	r = ok(i, "%s", v);
done:
	if (pickle_free(i, v) != PICKLE_OK)
		return PICKLE_ERROR;
	return r;
}

static int pickleCommandCdbWrite(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 4)
		return error(i, "Invalid subcommand %s: expected cdb-handle key value", argv[0]);
	cdb_wrapper_t *w = pickle_mod_tag_find(pd, argv[1]);
	if (!w || !(w->cdb))
		return error(i, "Invalid cdb file handle %s", argv[1]);
	if (!(w->creating))
		return error(i, "Attempting to use a read only database");
	cdb_buffer_t k = { .length = strlen(argv[2]), .buffer = argv[2] };
	cdb_buffer_t v = { .length = strlen(argv[3]), .buffer = argv[3] };
	if (cdb_add(w->cdb, &k, &v) < 0)
		return error(i, "could not add key/value pair: %s/%s", argv[2], argv[3]);
	return PICKLE_OK;
}

static int pickleCommandCdbStats(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 2)
		return error(i, "Invalid subcommand %s: expected cdb-handle", argv[0]);
	cdb_wrapper_t *w = pickle_mod_tag_find(pd, argv[1]);
	if (!w || !(w->cdb))
		return error(i, "Invalid cdb file handle %s", argv[1]);
	cdb_statistics_t s = {
		.records          = 0,
		.min_key_length   = ULONG_MAX,
		.min_value_length = ULONG_MAX,
	};

	if (cdb_foreach(w->cdb, cdb_stats, &s) < 0)
		return error(i, "cdb stats failure");

	return ok(i, "{records %ld} {key-min %ld} {key-max %ld} {key-bytes %ld} {val-min %ld} {val-max %ld} {val-bytes %ld}", 
			s.records,
			s.min_key_length,   s.max_key_length, s.total_key_length,
			s.min_value_length, s.max_value_length, s.total_value_length);
}

static int pickleCommandCdb(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc < 2)
		return error(i, "Invalid command %s: cdb {open|close|read|write|exists|stats} options..", argv[0]);
	if (!strcmp("open", argv[1]))
		return pickleCommandCdbOpen(i, argc - 1, argv + 1, pd);
	if (!strcmp("close", argv[1]))
		return pickleCommandCdbClose(i, argc - 1, argv + 1, pd);
	if (!strcmp("read", argv[1]))
		return pickleCommandCdbRead(i, argc - 1, argv + 1, pd);
	if (!strcmp("write", argv[1]))
		return pickleCommandCdbWrite(i, argc - 1, argv + 1, pd);
	if (!strcmp("stats", argv[1]))
		return pickleCommandCdbStats(i, argc - 1, argv + 1, pd);
	if (!strcmp("exists", argv[1]))
		return pickleCommandCdbExists(i, argc - 1, argv + 1, pd);
	if (!strcmp("count", argv[1]))
		return pickleCommandCdbCount(i, argc - 1, argv + 1, pd);
	if (!strcmp("version", argv[1])) {
		unsigned long v = 0;
		if (cdb_version(&v) < 0)
			return error(i, "Invalid version %lu", v);
		return ok(i, "%d %d %d", (int)((v >> 16) & 255), (int)((v >> 8) & 255), (int)((v >> 0) & 255));
	}
	//if (!strcmp("tests", argv[1]))
	//	return pickleCommandCdbTest(i, argc - 1, argv + 1, pd);
	return error(i, "Invalid subcommand %s", argv[1]);
}

static int cleanup(pickle_mod_t *m, void *tag) {
	assert(m);
	assert(tag);
	cdb_wrapper_t *w = tag;
	const int r1 = cdb_close(w->cdb);
	const int r2 = pickle_free(m->i, w);
	return r1 < 0 || r2 != PICKLE_OK ? PICKLE_ERROR : PICKLE_OK;
}	

int pickleModCdbRegister(pickle_mod_t *m) {
	assert(m);
	pickle_command_t cmds[] = { { "cdb",  pickleCommandCdb,  m }, };
	m->cleanup = cleanup;
	return pickle_mod_commands_register(m, cmds, NELEMS(cmds));
}

