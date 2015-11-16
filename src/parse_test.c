#include "parse_impl.h"
#include "parse_provider.h"

#define E_TEST_BAD_AST_CHILD 1

static selem_t ignored;

/*
 * TESTS
 * =====
 *
 * Here are the DTrace-triggered tests.
 */

lp_ast_node_t *kid;
int is_kid;

void
parse_test_child_cb(gelem_t from, gelem_t to, gelem_t weight)
{
	if (is_kid) {
		return;
	}
	lp_grmr_node_t *c = to.ge_p;
	is_kid = !strcmp(c->gn_name, kid->an_gnm);
}

int
parse_test_add_child(lp_ast_node_t *p, lp_ast_node_t *c)
{
	lp_ast_t *ast = p->an_ast;
	lp_grmr_t *grmr = ast->ast_grmr;
	lg_graph_t *g = grmr->grmr_graph;
	kid = c;
	is_kid = 0;

	/*
	 * The child `c` that we are adding to `p`, must correspond to one of
	 * the children of the grammar node that is associated with `p`.
	 */
	lp_grmr_node_t *pn = find_grmr_node(grmr, p->an_gnm);
	gelem_t pge;
	pge.ge_p = pn;

	lg_neighbors(g, pge, parse_test_child_cb);
	kid = NULL;
	if (is_kid) {
		return (0);
	}
	return (E_TEST_BAD_AST_CHILD);
}
