#include "psh.h"
#include "pickle.h"
#include "httpc.h"
#include "shrink.h"
#include "cdb.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(X) ((void)(X))

typedef struct {
	char *arg;   /* parsed argument */
	int error,   /* turn error reporting on/off */
	    index,   /* index into argument list */
	    option,  /* parsed option */
	    reset;   /* set to reset */
	char *place; /* internal use: scanner position */
	int  init;   /* internal use: initialized or not */
} pickle_getopt_t;   /* getopt clone; with a few modifications */

static void *allocator(void *arena, void *ptr, const size_t oldsz, const size_t newsz) {
	UNUSED(arena);
	if (newsz == 0) { free(ptr); return NULL; }
	if (newsz > oldsz) return realloc(ptr, newsz);
	return ptr;
}

/* Adapted from: <https://stackoverflow.com/questions/10404448> */
static int pickle_getopt(pickle_getopt_t *opt, const int argc, char *const argv[], const char *fmt) {
	assert(opt);
	assert(fmt);
	assert(argv);
	/* enum { BADARG_E = ':', BADCH_E = '?' }; */
	enum { BADARG_E = PICKLE_ERROR, BADCH_E = PICKLE_ERROR };

	if (!(opt->init)) {
		opt->place = ""; /* option letter processing */
		opt->init  = 1;
		opt->index = 1;
	}

	if (opt->reset || !*opt->place) { /* update scanning pointer */
		opt->reset = 0;
		if (opt->index >= argc || *(opt->place = argv[opt->index]) != '-') {
			opt->place = "";
			return PICKLE_RETURN;
		}
		if (opt->place[1] && *++opt->place == '-') { /* found "--" */
			opt->index++;
			opt->place = "";
			return PICKLE_RETURN;
		}
	}

	const char *oli = NULL; /* option letter list index */
	if ((opt->option = *opt->place++) == ':' || !(oli = strchr(fmt, opt->option))) { /* option letter okay? */
		 /* if the user didn't specify '-' as an option, assume it means -1.  */
		if (opt->option == '-')
			return PICKLE_RETURN;
		if (!*opt->place)
			opt->index++;
		/*if (opt->error && *fmt != ':')
			(void)fprintf(stderr, "illegal option -- %c\n", opt->option);*/
		return BADCH_E;
	}

	if (*++oli != ':') { /* don't need argument */
		opt->arg = NULL;
		if (!*opt->place)
			opt->index++;
	} else {  /* need an argument */
		if (*opt->place) { /* no white space */
			opt->arg = opt->place;
		} else if (argc <= ++opt->index) { /* no arg */
			opt->place = "";
			if (*fmt == ':')
				return BADARG_E;
			/*if (opt->error)
				(void)fprintf(stderr, "option requires an argument -- %c\n", opt->option);*/
			return BADCH_E;
		} else	{ /* white space */
			opt->arg = argv[opt->index];
		}
		opt->place = "";
		opt->index++;
	}
	return opt->option; /* dump back option letter */
}

static char *slurp(pickle_t *i, FILE *input, size_t *length, char *class) {
	char *m = NULL;
	const size_t bsz = class ? 4096 : 80;
	size_t sz = 0;
	if (length)
		*length = 0;
	for (;;) {
		if (pickle_reallocate(i, (void**)&m, sz + bsz + 1) != PICKLE_OK)
			return NULL;
		if (class) {
			size_t j = 0;
			int ch = 0, done = 0;
			for (; ((ch = fgetc(input)) != EOF) && j < bsz; ) {
				m[sz + j++] = ch;
				if (strchr(class, ch)) {
					done = 1;
					break;
				}
			}
			sz += j;
			if (done || ch == EOF)
				break;
		} else {
			size_t inc = fread(&m[sz], 1, bsz, input);
			sz += inc;
			if (inc != bsz)
				break;
		}
	}
	m[sz] = '\0'; /* ensure NUL termination */
	if (length)
		*length = sz;
	return m;
}

static int commandGets(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 1)
		return pickle_set_result_error_arity(i, 1, argc, argv);
	size_t length = 0;
	char *line = slurp(i, (FILE*)pd, &length, "\n");
	if (!line)
		return pickle_set_result_error(i, "Out Of Memory");
	if (!length) {
		if (pickle_free(i, (void**)&line) != PICKLE_OK)
			return PICKLE_ERROR;
		if (pickle_set_result(i, "EOF") != PICKLE_OK)
			return PICKLE_ERROR;
		return PICKLE_BREAK;
	}
	const int r = pickle_set_result(i, line);
	if (pickle_free(i, (void**)&line) != PICKLE_OK)
		return PICKLE_ERROR;
	return r;
}

static int commandPuts(pickle_t *i, int argc, char **argv, void *pd) {
	FILE *out = pd;
	if (argc != 1 && argc != 2 && argc != 3)
		return pickle_set_result_error_arity(i, 2, argc, argv);
	if (argc == 1)
		return fputc('\n', out) < 0 ? PICKLE_ERROR : PICKLE_OK;
	if (argc == 2)
		return fprintf(out, "%s\n", argv[1]) < 0 ? PICKLE_ERROR : PICKLE_OK;
	if (!strcmp(argv[1], "-nonewline"))
		return fputs(argv[2], out) < 0 ? PICKLE_ERROR : PICKLE_OK;
	return pickle_set_result_error(i, "Invalid option %s", argv[1]);
}

static int commandGetEnv(pickle_t *i, int argc, char **argv, void *pd) {
	UNUSED(pd);
	if (argc != 2)
		return pickle_set_result_error_arity(i, 2, argc, argv);
	const char *env = getenv(argv[1]);
	return pickle_set_result_string(i, env ? env : "");
}

static int commandExit(pickle_t *i, int argc, char **argv, void *pd) {
	UNUSED(pd);
	if (argc != 2 && argc != 1)
		return pickle_set_result_error_arity(i, 2, argc, argv);
	const char *code = argc == 2 ? argv[1] : "0";
	exit(atoi(code));
	return PICKLE_OK;
}

static int commandSource(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 1 && argc != 2)
		return pickle_set_result_error_arity(i, 2, argc, argv);
	errno = 0;
	FILE *file = argc == 1 ? pd : fopen(argv[1], "rb");
	if (!file)
		return pickle_set_result_error(i, "Could not open file '%s' for reading: %s", argv[1], strerror(errno));

	char *program = slurp(i, file, NULL, NULL);
	if (file != pd)
		fclose(file);
	if (!program)
		return pickle_set_result_error(i, "Out Of Memory");

	const int r = pickle_eval(i, program);
	if (pickle_free(i, (void**)&program) != PICKLE_OK)
		return PICKLE_ERROR;
	return r;
}

static int evalFile(pickle_t *i, char *file) {
	const int r = file ?
		commandSource(i, 2, (char*[2]){ "source", file }, NULL):
		commandSource(i, 1, (char*[1]){ "source",      }, stdin);
	if (r != PICKLE_OK) {
		const char *f = NULL;
		if (pickle_get_result_string(i, &f) != PICKLE_OK)
			return r;
		if (fprintf(stdout, "%s\n", f) < 0)
			return PICKLE_ERROR;
	}
	return r;
}

static int setArgv(pickle_t *i, int argc, char **argv) {
	char *args = NULL;
	int r = PICKLE_ERROR;
	if ((pickle_concatenate(i, argc, argv, &args) != PICKLE_OK) || args == NULL)
		goto done;
	r = pickle_set_var_string(i, "argv", args);
done:
	if (pickle_free(i, (void**)&args) != PICKLE_OK)
		return PICKLE_ERROR;
	return r;
}

static int prompt(FILE *out, int ret, const char *str) {
	if (str[0]) if (fprintf(out, "%s\n", str) < 0) return -1;
	if (fprintf(out, "[%d] psh> ", ret) < 0) return -1;
	if (fflush(out) < 0) return -1;
	return 0;
}

static int shell(pickle_t *i, FILE *in, FILE *out) {
	assert(i);
	assert(in);
	assert(out);
	if (prompt(out, 0, "") < 0) return -1;
	char *l = NULL;
	for (;;) {
		const char *rstr = NULL;
		if (!(l = slurp(i, in, NULL, "\n")))
			break;
		if (l[0] == '\0') {
			if (pickle_free(i, (void**)&l) != PICKLE_OK) return -1;
			break;
		}
		const int rval = pickle_eval(i, l);
		if (pickle_free(i, (void**)&l) != PICKLE_OK) return -1;
		if (pickle_get_result_string(i, &rstr) != PICKLE_OK) return -1;
		if (prompt(out, rval, rstr) < 0) return -1;
	}
	return 0;
}

int psh(int argc, char **argv) {
	(void)argc;
	(void)argv;
	pickle_t *i = NULL;

	if (pickle_tests(allocator, NULL)   != PICKLE_OK) goto fail;
	if (pickle_new(&i, allocator, NULL) != PICKLE_OK) goto fail;
	if (setArgv(i, argc, argv)  != PICKLE_OK) goto fail;

	typedef struct {
		const char *name; pickle_command_func_t func; void *data;
	} commands_t;

	const commands_t cmds[] = {
		{ "gets",   commandGets,   stdin  },
		{ "puts",   commandPuts,   stdout },
		{ "getenv", commandGetEnv, NULL   },
		{ "exit",   commandExit,   NULL   },
		{ "source", commandSource, NULL   },
	};

	for (size_t j = 0; j < sizeof (cmds) / sizeof (cmds[0]); j++)
		if (pickle_register_command(i, cmds[j].name, cmds[j].func, cmds[j].data) != PICKLE_OK)
			goto fail;

	const int r = shell(i, stdin, stdout);
	if (pickle_delete(i) != PICKLE_OK)
		return 1;
	return r;
fail:
	(void)pickle_delete(i);
	return 1;
}

