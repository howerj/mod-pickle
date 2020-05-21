#include "psh.h"
#include "mod.h"
#include "pickle.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
	char *arg;   /* parsed argument */
	int error,   /* turn error reporting on/off */
	    index,   /* index into argument list */
	    option,  /* parsed option */
	    reset;   /* set to reset */
	char *place; /* internal use: scanner position */
	int  init;   /* internal use: initialized or not */
} pickle_getopt_t;   /* getopt clone; with a few modifications */

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

static int release(pickle_t *i, void *ptr) {
	void *arena = NULL;
	allocator_fn fn = NULL;
	const int r1 = pickle_allocator_get(i, &fn, &arena);
	if (fn)
		fn(arena, ptr, 0, 0);
	return fn ? r1 : PICKLE_ERROR;
}

static void *reallocator(pickle_t *i, void *ptr, size_t sz) {
	void *arena = NULL;
	allocator_fn fn = NULL;
	if (pickle_allocator_get(i, &fn, &arena) != PICKLE_OK)
		abort();
	void *r = fn(arena, ptr, 0, sz);
	if (!r) {
		release(i, ptr);
		return NULL;
	}
	return r;
}

static char *slurp(pickle_t *i, FILE *input, size_t *length, char *class) {
	char *m = NULL;
	const size_t bsz = class ? 4096 : 80;
	size_t sz = 0;
	if (length)
		*length = 0;
	for (;;) {
		if ((m = reallocator(i, m, sz + bsz + 1)) == NULL)
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
		return error(i, "Invalid command %s", argv[0]);
	size_t length = 0;
	char *line = slurp(i, (FILE*)pd, &length, "\n");
	if (!line)
		return error(i, "Out Of Memory");
	if (!length) {
		release(i, line);
		if (ok(i, "EOF") != PICKLE_OK)
			return PICKLE_ERROR;
		return PICKLE_BREAK;
	}
	const int r = ok(i, line);
	release(i, line);
	return r;
}

static int commandPuts(pickle_t *i, int argc, char **argv, void *pd) {
	FILE *out = pd;
	if (argc != 1 && argc != 2 && argc != 3)
		return error(i, "Invalid command %s -nonewline? string?", argv[0]);
	if (argc == 1)
		return fputc('\n', out) < 0 ? PICKLE_ERROR : PICKLE_OK;
	if (argc == 2)
		return fprintf(out, "%s\n", argv[1]) < 0 ? PICKLE_ERROR : PICKLE_OK;
	if (!strcmp(argv[1], "-nonewline"))
		return fputs(argv[2], out) < 0 ? PICKLE_ERROR : PICKLE_OK;
	return error(i, "Invalid option %s", argv[1]);
}

static int commandGetEnv(pickle_t *i, int argc, char **argv, void *pd) {
	UNUSED(pd);
	if (argc != 2)
		return error(i, "Invalid command %s string", argv[0]);
	const char *env = getenv(argv[1]);
	return ok(i, env ? env : "");
}

static int commandExit(pickle_t *i, int argc, char **argv, void *pd) {
	UNUSED(pd);
	if (argc != 2 && argc != 1)
		return error(i, "Invalid command %s number?", argv[0]);
	const char *code = argc == 2 ? argv[1] : "0";
	exit(atoi(code));
	return PICKLE_ERROR; /* unreachable */
}

static int commandClock(pickle_t *i, const int argc, char **argv, void *pd) {
	UNUSED(pd);
	time_t ts = 0;
	if (argc < 2)
		return error(i, "Invalid command %s subcommand...", argv[0]);
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
			return error(i, "Invalid subcommand");
		if (sscanf(argv[2], "%d", &tv) != 1)
			return error(i, "Invalid number: %s", argv[2]);
		ts = tv;
		struct tm *timeinfo = (gmt ? gmtime : localtime)(&ts);
		strftime(buf, sizeof buf, fmt, timeinfo);
		return ok(i, "%s", buf);
	}
	return error(i, "Invalid command %s subcommand...", argv[0]);
}

static int commandSource(pickle_t *i, int argc, char **argv, void *pd) {
	if (argc != 1 && argc != 2)
		return error(i, "Invalid command %s string?", argv[0]);
	errno = 0;
	FILE *file = argc == 1 ? pd : fopen(argv[1], "rb");
	if (!file)
		return error(i, "Could not open file '%s' for reading: %s", argv[1], strerror(errno));

	char *program = slurp(i, file, NULL, NULL);
	if (file != pd)
		fclose(file);
	if (!program)
		return error(i, "Out Of Memory");

	const int r = pickle_eval(i, program);
	release(i, program);
	return r;
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
			if (release(i, l) != PICKLE_OK) return -1;
			break;
		}
		const int rval = pickle_eval(i, l);
		if (release(i, l) != PICKLE_OK) return -1;
		if (pickle_result_get(i, &rstr) != PICKLE_OK) return -1;
		if (prompt(out, rval, rstr) < 0) return -1;
	}
	return 0;
}

int psh(int argc, char **argv) {
	(void)argc;
	(void)argv;
	pickle_t *i = NULL;

	if (pickle_tests(pickle_mod_allocator, NULL)   != PICKLE_OK) goto fail;
	if (pickle_new(&i, pickle_mod_allocator, NULL) != PICKLE_OK) goto fail;
	if (pickle_var_set_args(i, "argv", argc, argv)  != PICKLE_OK) goto fail;

	typedef struct {
		const char *name; pickle_func_t func; void *data;
	} commands_t;

	const commands_t cmds[] = {
		{ "gets",   commandGets,   stdin  },
		{ "puts",   commandPuts,   stdout },
		{ "getenv", commandGetEnv, NULL   },
		{ "exit",   commandExit,   NULL   },
		{ "source", commandSource, NULL   },
		{ "clock",  commandClock,  NULL   },
	};

	for (size_t j = 0; j < sizeof (cmds) / sizeof (cmds[0]); j++)
		if (pickle_command_register(i, cmds[j].name, cmds[j].func, cmds[j].data) != PICKLE_OK)
			goto fail;

	const int r = shell(i, stdin, stdout);
	if (pickle_delete(i) != PICKLE_OK)
		return 1;
	return r;
fail:
	(void)pickle_delete(i);
	return 1;
}

