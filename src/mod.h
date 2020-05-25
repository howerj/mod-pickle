#ifndef MOD_H
#define MOD_H

#include "pickle.h"
#include <stdio.h>

typedef struct {
	char *name;
	void *tag;
} pickle_mod_tag_t;

struct pickle_mod;
typedef struct pickle_mod pickle_mod_t;

struct pickle_mod {
	pickle_t *i;
	pickle_mod_tag_t *tags;
	size_t length;
	int (*cleanup)(pickle_mod_t *m, void *tag);
};

typedef struct {
	const char *name;
	pickle_func_t func;
	void *privdata;
} pickle_command_t;

typedef struct {
	pickle_mod_t *mods;
	size_t length;
	pickle_t *i;
} pickle_mods_t;

typedef struct { 
	long allocs, 
	     frees, 
	     reallocs, 
	     total; 
} heap_t;

typedef struct {
	char *arg;   /* parsed argument */
	int error,   /* turn error reporting on/off */
	    index,   /* index into argument list */
	    option,  /* parsed option */
	    reset;   /* set to reset */
	char *place; /* internal use: scanner position */
	int  init;   /* internal use: initialized or not */
} pickle_getopt_t;   /* getopt clone; with a few modifications */

typedef int (*pickle_mod_register_t)(pickle_mod_t *m);

void *pickle_mod_allocator(void *arena, void *ptr, const size_t oldsz, const size_t newsz);
int pickle_free(pickle_t *i, void *ptr);
void *pickle_allocate(pickle_t *i, size_t sz);
void *pickle_realloc(pickle_t *i, void *ptr, size_t sz);
char *pickle_slurp(pickle_t *i, FILE *input, size_t *length, char *ch_class);

int pickle_mod_commands_register(pickle_mod_t *m, pickle_command_t *c, size_t length);
void *pickle_mod_tag_find(pickle_mod_t *m, const char *name);
int pickle_mod_tag_add(pickle_mod_t *m, const char *name, void *tag);
int pickle_mod_tag_remove(pickle_mod_t *m, const char *name);
int pickle_mod_tag_foreach(pickle_mod_t *m, int (*fn)(pickle_mod_t *m, pickle_mod_tag_t *tag));
pickle_mod_t *pickle_mod_new(pickle_t *i, pickle_mod_register_t rm);
int pickle_mod_delete(pickle_t *i);

pickle_mods_t *pickle_register_mods(pickle_t *i);
void pickle_destroy_mods(pickle_mods_t *ms);

int pickle_getopt(pickle_getopt_t *opt, const int argc, char *const argv[], const char *fmt);

#ifndef MIN
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#endif

#ifndef MAX
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#endif

#define ok(i, ...)    pickle_result_set(i, PICKLE_OK,    __VA_ARGS__)
#define error(i, ...) pickle_result_set(i, PICKLE_ERROR, __VA_ARGS__)
#define NELEMS(X) (sizeof(X) / sizeof(X[0]))
#define UNUSED(X) ((void)(X))

#endif
