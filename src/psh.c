#include "psh.h"
#include "pickle.h"
#include "httpc.h"
#include "cdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(X) ((void)(X))

static void *allocator(void *arena, void *ptr, const size_t oldsz, const size_t newsz) {
	UNUSED(arena);
	if (newsz == 0) { free(ptr); return NULL; }
	if (newsz > oldsz) return realloc(ptr, newsz);
	return ptr;
}

static int prompt(FILE *out, int ret, const char *str) {
	if (str[0]) if (fprintf(out, "%s\n", str) < 0) return -1;
	if (fprintf(out, "[%d] psh> ", ret) < 0) return -1;
	if (fflush(out) < 0) return -1;
	return 0;
}

int psh(int argc, char **argv) {
	(void)argc;
	(void)argv;
	pickle_t *i = NULL;

	if (pickle_tests(allocator, NULL)   != PICKLE_OK) goto fail;
	if (pickle_new(&i, allocator, NULL) != PICKLE_OK) goto fail;

	if (prompt(stdout, 0, "") < 0) goto fail;
	for (char l[256] = { 0 }; fgets(l, sizeof l, stdin); memset(l, 0, sizeof l)) {
		const char *rstr = NULL;
		const int rval = pickle_eval(i, l);
		if (pickle_get_result_string(i, &rstr) != PICKLE_OK) goto fail;
		if (prompt(stdout, rval, rstr) < 0) goto fail;
	}

fail:
	return pickle_delete(i);
}

