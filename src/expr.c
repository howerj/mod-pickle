#include "mod.h"
#include "q.h"
#include <assert.h>

static int pickleCommandExpr(pickle_t *i, int argc, char **argv, void *pd) {
	UNUSED(pd);
	if (argc != 2)
		return error(i, "Invalid command %s", argv[0]);
	const qoperations_t *ops[64];
	q_t numbers[64];
	qvariable_t e  = { "e",  qinfo.e };
	qvariable_t pi = { "pi", qinfo.pi };
	qvariable_t *vars[] = { &e, &pi };
	qexpr_t expr = {
		.ops         = ops,
		.numbers     = numbers,
		.ops_max     = NELEMS(ops),
		.numbers_max = NELEMS(numbers),
		.vars        = vars,
		.vars_max    = NELEMS(vars),
	};
	const int r = qexpr(&expr, argv[1]);
	if (r < 0)
		return error(i, "Invalid expression %s: %s", argv[1], expr.error_string);
	char n[64] = { 0 };
	if (qsprint(expr.numbers[0], n, sizeof n) < 0)
		return error(i, "Numeric conversion failed in expr");
	return ok(i, "%s", n);
}

static int cleanup(pickle_mod_t *m, void *tag) {
	UNUSED(m);
	UNUSED(tag);
	return PICKLE_OK;
}	

int pickleModExprRegister(pickle_mod_t *m) {
	assert(m);
	pickle_command_t cmds[] = { 
		{ "expr",  pickleCommandExpr,  m }, 
	};
	m->cleanup = cleanup;
	return pickle_mod_commands_register(m, cmds, NELEMS(cmds));
}

