#include "mod.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* NB. This allocator can be use to get memory statistics (printed atexit) or test allocation failures */
void *pickle_mod_allocator(void *arena, void *ptr, const size_t oldsz, const size_t newsz) {
	UNUSED(arena);
	if (newsz == 0) { free(ptr); return NULL; }
	if (newsz > oldsz) return realloc(ptr, newsz);
	return ptr;
}

int pickle_mod_commands_register(pickle_mod_t *m, pickle_command_t *c, size_t length) {
	assert(m);
	assert(c);
	for (size_t i = 0; i < length; i++)
		if (pickle_command_register(m->i, c->name, c->func, c->privdata) != PICKLE_OK)
			return PICKLE_ERROR;
	return PICKLE_OK;
}

void *pickle_mod_tag_find(pickle_mod_t *m, const char *name) {
	assert(m);
	assert(name);
	for (size_t i = 0; i < m->length; i++) {
		pickle_mod_tag_t *t = &m->tags[i];
		if (t->name && !strcmp(t->name, name))
			return t->tag;
	}
	return NULL;
}

int pickle_mod_tag_add(pickle_mod_t *m, const char *name, void *data) {
	assert(m);
	assert(name);
	return PICKLE_OK;
}

int pickle_mod_tag_remove(pickle_mod_t *m, const char *name) {
	assert(m);
	for (size_t i = 0; i < m->length; i++) {
		pickle_mod_tag_t *t = &m->tags[i];
		if (t->name && !strcmp(t->name, name)) {
			m->cleanup(m, t->tag);
			free(t->name);
			t->tag = NULL;
			t->name = NULL;
			//memmove();
			//realloc();
		}
	}
	return PICKLE_OK;
}

pickle_mod_t *pickle_mod_new(pickle_t *i, pickle_mod_register_t rm) {
	assert(i);
	assert(rm);
	pickle_mod_t *m = calloc(1, sizeof *m);
	if (!m)
		return NULL;


	return m;
}

int pickle_mode_tag_foreach(pickle_mod_t *m, int (*fn)(pickle_mod_t *m, pickle_mod_tag_t *tag)) {
	assert(m);
	assert(fn);
	int r = PICKLE_OK;
	for (size_t i = 0; i < m->length; i++)
		if (fn(m, &m->tags[i]) < 0)
			r = PICKLE_ERROR;
	return r;
}

