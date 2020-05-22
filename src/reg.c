#include "reg.h"
#include <assert.h>
#include <stdlib.h>

extern int pickleModCdbRegister(pickle_mod_t *m);

pickle_mods_t *pickle_register_mods(pickle_t *i) {
	assert(i);
	struct reg {
		int (*reg)(pickle_mod_t *m);
	} regs[] = {
		{ pickleModCdbRegister, }
	};
	const size_t regsl = sizeof (regs) / sizeof(regs[0]);
	pickle_mod_t *mods = calloc(regsl, sizeof *mods);
	pickle_mods_t *ms = calloc(1, sizeof *ms);
	if (!ms || !mods) {
		free(mods);
		free(ms);
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
	return ms;
}

void pickle_destroy_mods(pickle_mods_t *ms) {
	assert(ms);
	for (size_t j = 0; j < ms->length; j++) {
		pickle_mod_t *m = &ms->mods[j];
		//pickle_mod_tag_foreach(m, m->cleanup);
		free(m);
	}
	free(ms);
	return;
}

