#include "mod.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern int pickleModCdbRegister(pickle_mod_t *m);
extern int pickleModUtf8Register(pickle_mod_t *m);

typedef struct {
	size_t allocs, frees, total;
} heap_stats_t;

/* TODO: Add a heap command for collecting statistics about the heap */
void *pickle_mod_allocator(void *arena, void *ptr, const size_t oldsz, const size_t newsz) {
	UNUSED(arena);
	if (newsz == 0) { free(ptr); return NULL; }
	if (newsz > oldsz) return realloc(ptr, newsz);
	return ptr;
}

int pickle_free(pickle_t *i, void *ptr) {
	void *arena = NULL;
	allocator_fn fn = NULL;
	const int r1 = pickle_allocator_get(i, &fn, &arena);
	if (fn)
		fn(arena, ptr, 0, 0);
	return fn ? r1 : PICKLE_ERROR;
}

void *pickle_realloc(pickle_t *i, void *ptr, size_t sz) {
	void *arena = NULL;
	allocator_fn fn = NULL;
	if (pickle_allocator_get(i, &fn, &arena) != PICKLE_OK)
		abort();
	void *r = fn(arena, ptr, 0, sz);
	if (!r) {
		pickle_free(i, ptr);
		return NULL;
	}
	return r;
}

void *pickle_allocate(pickle_t *i, size_t sz) {
	assert(i);
	void *r = pickle_realloc(i, NULL, sz);
	return r ? memset(r, 0, sz) : NULL;
}

char *pickle_slurp(pickle_t *i, FILE *input, size_t *length, char *ch_class) {
	char *m = NULL;
	const size_t bsz = ch_class ? 4096 : 80;
	size_t sz = 0;
	if (length)
		*length = 0;
	for (;;) {
		if ((m = pickle_realloc(i, m, sz + bsz + 1)) == NULL)
			return NULL;
		if (ch_class) {
			size_t j = 0;
			int ch = 0, done = 0;
			for (; ((ch = fgetc(input)) != EOF) && j < bsz; ) {
				m[sz + j++] = ch;
				if (strchr(ch_class, ch)) {
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

static char *pickleStrdup(pickle_t *i, const char *s) {
	assert(s);
	assert(i);
	const size_t l = strlen(s);
	char *r = pickle_allocate(i, l);
	return r ? memcpy(r, s, l + 1) : NULL;
}

int pickle_mod_commands_register(pickle_mod_t *m, pickle_command_t *c, size_t length) {
	assert(m);
	assert(c);
	for (size_t i = 0; i < length; i++)
		if (pickle_command_register(m->i, c[i].name, c[i].func, c[i].privdata) != PICKLE_OK)
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
	if (pickle_mod_tag_find(m, name))
		return PICKLE_ERROR;
	pickle_mod_tag_t *old = m->tags;
	if ((m->tags = pickle_realloc(m->i, m->tags, m->length + 1 * sizeof (*m->tags))) == NULL) {
		/* TODO: handle error */
		(void)old;
		return PICKLE_ERROR;
	}

	pickle_mod_tag_t *t = &m->tags[m->length++];
	if (!(t->name = pickleStrdup(m->i, name))) {
		/* TODO: handle error */
		return PICKLE_ERROR;
	}
	t->tag = data;
	return PICKLE_OK;
}

int pickle_mod_tag_remove(pickle_mod_t *m, const char *name) {
	assert(m);
	for (size_t i = 0; i < m->length; i++) {
		pickle_mod_tag_t *t = &m->tags[i];
		if (t->name && !strcmp(t->name, name)) {
			if (t->tag)
				m->cleanup(m, t->tag);
			free(t->name);
			t->tag = NULL;
			t->name = NULL;
			//memmove();
			//pickle_realloc();
			return PICKLE_OK;
		}
	}
	return PICKLE_ERROR;
}

pickle_mod_t *pickle_mod_new(pickle_t *i, pickle_mod_register_t rm) {
	assert(i);
	assert(rm);
	pickle_mod_t *m = pickle_allocate(i, sizeof *m);
	if (!m)
		return NULL;
	return m;
}

int pickle_mod_tag_foreach(pickle_mod_t *m, int (*fn)(pickle_mod_t *m, pickle_mod_tag_t *tag)) {
	assert(m);
	assert(fn);
	int r = PICKLE_OK;
	for (size_t i = 0; i < m->length; i++)
		if (fn(m, &m->tags[i]) < 0)
			r = PICKLE_ERROR;
	return r;
}

static int pickleCommandModule(pickle_t *i, int argc, char **argv, void *pd) {
	pickle_mods_t *ms = pd;
	if (argc < 2)
		return error(i, "Invalid command %s", argv[0]);
	if (!strcmp("loaded", argv[1]))
		return ok(i, "%lu", (unsigned long)ms->length);
	return error(i, "Invalid subcommand %s", argv[1]);
}

pickle_mods_t *pickle_register_mods(pickle_t *i) {
	assert(i);
	struct reg {
		int (*reg)(pickle_mod_t *m);
	} regs[] = {
		{ pickleModCdbRegister, },
		{ pickleModUtf8Register, },
	};
	const size_t regsl = sizeof (regs) / sizeof(regs[0]);
	pickle_mod_t *mods = pickle_allocate(i, regsl * sizeof *mods);
	pickle_mods_t *ms = pickle_allocate(i, sizeof *ms);
	if (!ms || !mods) {
		pickle_free(i, mods);
		pickle_free(i, ms);
		return NULL;
	}
	if (pickle_command_register(i, "module", pickleCommandModule, ms) != PICKLE_OK) {
		pickle_free(i, mods);
		pickle_free(i, ms);
		return NULL;
	}


	for (size_t j = 0; j < regsl; j++) {
		struct reg *r = &regs[j];
		mods[j].i = i;
		if (r->reg(&mods[j]) < 0) {
		}
	}
	ms->length = regsl;
	ms->mods = mods;
	ms->i = i;
	return ms;
}

void pickle_destroy_mods(pickle_mods_t *ms) {
	assert(ms);
	for (size_t j = 0; j < ms->length; j++) {
		pickle_mod_t *m = &ms->mods[j];
		//pickle_mod_tag_foreach(m, m->cleanup);
		pickle_free(ms->i, m);
	}
	pickle_free(ms->i, ms);
	return;
}



