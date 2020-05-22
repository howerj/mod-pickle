#ifndef MOD_H
#define MOD_H

#include "pickle.h"

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
} pickle_mods_t;

typedef int (*pickle_mod_register_t)(pickle_mod_t *m);

void *pickle_mod_allocator(void *arena, void *ptr, const size_t oldsz, const size_t newsz);
int pickle_mod_commands_register(pickle_mod_t *m, pickle_command_t *c, size_t length);

void *pickle_mod_tag_find(pickle_mod_t *m, const char *name);
int pickle_mod_tag_add(pickle_mod_t *m, const char *name, void *tag);
int pickle_mod_tag_remove(pickle_mod_t *m, const char *name);
int pickle_mod_tag_foreach(pickle_mod_t *m, int (*fn)(pickle_mod_t *m, pickle_mod_tag_t *tag));
pickle_mod_t *pickle_mod_new(pickle_t *i, pickle_mod_register_t rm);
int pickle_mod_delete(pickle_t *i);

#define ok(i, ...)    pickle_result_set(i, PICKLE_OK,    __VA_ARGS__)
#define error(i, ...) pickle_result_set(i, PICKLE_ERROR, __VA_ARGS__)
#define NELEMS(X) (sizeof(X) / sizeof(X[0]))
#define UNUSED(X) ((void)(X))

#endif
