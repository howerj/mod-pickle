#include "pickle.h"
#include "mod.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int commandHeap(pickle_t *i, int argc, char **argv, void *pd) {
	heap_t *h = pd;
	if (argc != 2)
		return error(i, "Invalid command %s", argv[0]);
	if (!strcmp(argv[1], "frees"))         return ok(i, "%ld", h->frees);
	if (!strcmp(argv[1], "allocations"))   return ok(i, "%ld", h->allocs);
	if (!strcmp(argv[1], "total"))         return ok(i, "%ld", h->total);
	if (!strcmp(argv[1], "reallocations")) return ok(i, "%ld", h->reallocs);
	return error(i, "Invalid command %s", argv[0]);
}

static int commandSource(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 1 && argc != 2)
		return error(i, "Invalid command %s", argv[0]);
	errno = 0;
	FILE *file = argc == 1 ? pd : fopen(argv[1], "rb");
	if (!file)
		return error(i, "Could not open file '%s' for reading: %s", argv[1], strerror(errno));
	char *program = pickle_slurp(i, file, NULL, NULL);
	if (file != pd)
		fclose(file);
	if (!program)
		return error(i, "Out Of Memory");
	const int r = pickle_eval(i, program);
	return pickle_free(i, program) == PICKLE_OK ? r : PICKLE_ERROR;
}

static int evalFile(pickle_t *i, char *file) {
	const int r = file ?
		commandSource(i, 2, (char*[2]){ "source", file }, NULL):
		commandSource(i, 1, (char*[1]){ "source",      }, stdin);
	if (r != PICKLE_OK) {
		const char *f = NULL;
		if (pickle_result_get(i, &f) != PICKLE_OK)
			return r;
		if (fprintf(stdout, "%s\n", f) < 0)
			return PICKLE_ERROR;
	}
	return r;
}

int main(int argc, char **argv) {
	heap_t h = { 0 };
	pickle_t *i = NULL;
	pickle_mods_t *ms = NULL;
	if (pickle_tests(pickle_mod_allocator, &h)   != PICKLE_OK) goto fail;
	if (pickle_new(&i, pickle_mod_allocator, &h) != PICKLE_OK) goto fail;
	if ((ms = pickle_register_mods(i)) == NULL) goto fail;
	if (pickle_var_set_args(i, "argv", argc, argv)  != PICKLE_OK) goto fail;
	if (pickle_command_register(i, "source", commandSource, NULL)   != PICKLE_OK) goto fail;
	if (pickle_command_register(i, "heap",   commandHeap,   &h)   != PICKLE_OK) goto fail;
	int r = 0;
	for (int j = 1; j < argc; j++) {
		r = evalFile(i, argv[j]);
		if (r < 0)
			goto fail;
		if (r == PICKLE_BREAK)
			break;
	}
	if (argc == 1)
		r = evalFile(i, NULL);
	pickle_destroy_mods(ms);
	return !!pickle_delete(i) || r < 0;
fail:
	(void)pickle_destroy_mods(ms);
	(void)pickle_delete(i);
	return 1;
}

