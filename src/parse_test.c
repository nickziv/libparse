#include "parse_impl.h"
#include "parse_provider.h"

/*
 * Some constants are no longer used. They are written as 'E_UNUSED_N'. Feel
 * free to take them for new error codes.
 */
#define E_AST_CHILD		1
#define E_UNUSED_1		2
#define E_ASTN_TYPE		3
#define E_ASTN_NULL_GNM		4
#define E_ASTN_OFFSET		5
#define E_ASTN_LR		6
#define E_ASTN_LRPTRSN		7
#define E_AST_NEIGHBOR		8
#define E_AST_NEIGHBORS		9
#define E_ASTN_NO_PARENT	10
#define E_ASTN_NO_LAST_CHILD	11
#define E_ASTN_TOO_FEW_KIDS	12
#define E_ASTN_STATE		13
#define E_ASTN_OFF_START_GT_IN	14
#define E_ASTN_OFF_END_GT_IN	15
#define E_ASTN_LRPTRS		16
#define E_ASTN_LRPTRS_NULL	17
/* GAP */
#define E_GN_TYPE		20
#define E_GN_TOK_NULL		21
#define E_GN_NAME_NULL		23
#define E_AST_NULL_INPUT	24
#define E_AST_ZERO_SIZE_INPUT	25
#define E_TOK_NAME_NULL		26
#define E_TOK_SEGS_NULL		27
#define E_TOK_SEGS_EMPTY	28
#define E_TSEG_OP		29
#define E_TSEG_WIDTH_ZERO	30
#define E_TSEG_DATA		31
#define	E_AST_NSPLIT_TOO_BIG	32
#define	E_AST_NSPLIT_TOO_SMALL	33
#define	E_ASTN_SPLIT_NKIDS	34
#define E_ASTN_INDEX_DIFF2BIG	35
#define E_ASTN_INDEX_DIFF2SMALL	36
#define	E_AST_GRAPH_WEIGHTS	37
#define	E_AST_STACK_DUPS	38
#define	E_AST_REF		39
#define	E_AST_REF_DIFF		40

static selem_t ignored;

/*
 * TESTS
 * =====
 *
 * Here are the DTrace-triggered tests.
 */

/*
 * These functions test whether the child that we are trying to connect to the
 * parent in the AST is also a child in the grammar. If it is not a child in
 * the grammar, we can't possibly want to connect it in the AST. Excuse the
 * globals.
 */
lp_ast_node_t *kid;
int is_kid;

void
lp_test_child_cb(gelem_t from, gelem_t to, gelem_t weight)
{
	(void)from;
	(void)weight;
	if (is_kid) {
		return;
	}
	lp_grmr_node_t *c = to.ge_p;
	is_kid = !strcmp(c->gn_name, kid->an_gnm);
}

int
lp_test_add_child(lp_ast_node_t *p, lp_ast_node_t *c)
{
	if (p->an_ast != c->an_ast) {
		return (E_AST_REF_DIFF);
	}
	lp_ast_t *ast = p->an_ast;
	lp_grmr_t *grmr = ast->ast_grmr;
	PARSE_GOT_HERE(p);
	PARSE_GOT_HERE(c);
	PARSE_GOT_HERE(p->an_ast);
	PARSE_GOT_HERE(c->an_ast);
	//PARSE_GOT_HERE(ast);
	//PARSE_GOT_HERE(grmr);
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

	lg_neighbors(g, pge, lp_test_child_cb);
	kid = NULL;
	if (is_kid) {
		return (0);
	}
	return (E_AST_CHILD);
}


/*
 * We sanity check the values of an ast_node_t's members.
 */
int
lp_test_ast_node(lp_ast_node_t *a)
{
	lp_ast_t *ast = a->an_ast;
	lp_grmr_t *grmr = ast->ast_grmr;

	if (a->an_type != SEQUENCER && a->an_type != SPLITTER &&
	    a->an_type != PARSER) {
		return (E_ASTN_TYPE);
	}

	if (a->an_gnm == NULL) {
		return (E_ASTN_NULL_GNM);
	}

	if (a->an_ast->ast_cloning == 0 && a->an_parent == NULL &&
	    strcmp(a->an_gnm, grmr->grmr_root->gn_name)) {
		return (E_ASTN_NO_PARENT);
	}

	if (a->an_kids && a->an_last_child == NULL) {
		return (E_ASTN_NO_LAST_CHILD);
	}

	if (a->an_kids) {
		lp_ast_node_t *kid = a->an_last_child;
		uint32_t i = 0;
		while (i < a->an_kids) {
			kid = kid->an_left;
			if (kid == NULL && i < a->an_kids - 1) {
				return (E_ASTN_TOO_FEW_KIDS);
			}
			i++;
		}
	}

	/*
	 * Splitter can only ever have 1 or 0 children.
	 */
	if (a->an_type == SPLITTER && a->an_kids > 1) {
		return (E_ASTN_SPLIT_NKIDS);
	}

	/* test that all left and right pointers are updated */
	if (a->an_kids) {
		lp_ast_node_t *kid = a->an_last_child;
		uint64_t i = 0;
		while (i < a->an_kids) {
			if (kid->an_left != NULL &&
			    kid->an_left->an_right != kid) {
				return (E_AST_NEIGHBOR);
			}

			if (kid->an_right != NULL &&
			    kid->an_right->an_left != kid) {
				return (E_AST_NEIGHBOR);
			}
			i++;
		}
	}

	if (a->an_state != ANS_TRY && a->an_state != ANS_FAIL &&
	    a->an_state != ANS_MATCH) {
		return (E_ASTN_STATE);
	}

	if (a->an_off_start > a->an_off_end) {
		return (E_ASTN_OFFSET);
	}

	if (a->an_off_start > ast->ast_sz) {
		return (E_ASTN_OFF_START_GT_IN);
	}

	if (a->an_off_end > ast->ast_sz) {
		return (E_ASTN_OFF_END_GT_IN);
	}

	if (a->an_parent != NULL) {
		if (a->an_parent->an_kids == 1) {
			if (a->an_left != NULL && a->an_right != NULL) {
				return (E_ASTN_LRPTRS);
			}
		}
		if (a->an_parent->an_kids > 1) {
			if (a->an_left == NULL && a->an_right == NULL) {
				return (E_ASTN_LRPTRS_NULL);
			}
		}
	}

	if (a->an_left != NULL) {
		if (!(a->an_left->an_index < a->an_index)) {
			return (E_ASTN_INDEX_DIFF2SMALL);
		}
		if ((a->an_index - a->an_left->an_index) > 1) {
			return (E_ASTN_INDEX_DIFF2BIG);
		}
	}

	if (a->an_right != NULL) {
		if (!(a->an_right->an_index > a->an_index)) {
			return (E_ASTN_INDEX_DIFF2SMALL);
		}
		if ((a->an_right->an_index - a->an_index) > 1) {
			return (E_ASTN_INDEX_DIFF2BIG);
		}
	}

	return (0);
}


/*
 * We sanity check the values of an grmr_node_t's members.
 */
int
lp_test_grmr_node(lp_grmr_node_t *g)
{
	if (g->gn_type != SEQUENCER || g->gn_type != SPLITTER ||
	    g->gn_type != PARSER) {
		return (E_GN_TYPE);
	}

	if (g->gn_type == PARSER && g->gn_tok == NULL) {
		return (E_GN_TOK_NULL);
	}

	if (g->gn_name == NULL) {
		return (E_GN_NAME_NULL);
	}
	return (0);
}

/*
 * We sanity check the grammar.
 */
int
lp_test_grmr(lp_grmr_t *g)
{
	(void)g;
	return (0);
}

static int test_ast_graph_failed;
static lp_ast_node_t *test_ast_graph_from;
static lp_ast_node_t *test_ast_graph_to;
static uint64_t test_ast_graph_weight;


void
test_ast_graph_cb(gelem_t f, gelem_t t, gelem_t w)
{
	lp_ast_node_t *from = f.ge_p;
	lp_ast_node_t *to = t.ge_p;
	uint64_t weight = w.ge_u;
	/*
	 * We can only begin the tests once we have already walked over the
	 * first edge.
	 */
	if (test_ast_graph_from != NULL) {
		if (from == test_ast_graph_from) {
			if (test_ast_graph_weight >= weight ||
			    (weight - test_ast_graph_weight) > 1 ) {
				test_ast_graph_failed = 1;
			}
		}
	}
	/*
	 * We save the edge compontents for comparison.
	 */
	test_ast_graph_from = from;
	test_ast_graph_to = to;
	test_ast_graph_weight = weight;
}

/*
 * We sanity check the graph that backs the AST.
 */
int
lp_test_ast_graph(lp_ast_t *ast)
{
	test_ast_graph_failed = 0;
	test_ast_graph_from = NULL;
	test_ast_graph_to = NULL;
	test_ast_graph_weight = NULL;
	/*
	 * We know that each edge that has some node FOO as its source node,
	 * must not have the same weight as any other edge that has FOO as its
	 * source node. We also know that the edges' weights must increase in
	 * sequential increments.
	 */
	lg_edges(ast->ast_graph, test_ast_graph_cb);
	return (test_ast_graph_failed);
}

typedef struct astn_count {
	uint64_t	astnc_count;
	lp_ast_node_t	*astnc_node;
} astn_count_t;

int
astnc_cmp(selem_t e1, selem_t e2)
{
	astn_count_t *c1 = e1.sle_p;
	astn_count_t *c2 = e2.sle_p;

	if (c1->astnc_node > c2->astnc_node) {
		return (1);
	}
	if (c1->astnc_node < c2->astnc_node) {
		return (-1);
	}
	return (0);
}

int
astnc_bnd(selem_t e1, selem_t min, selem_t max)
{
	astn_count_t *c1 = e1.sle_p;
	astn_count_t *cmin = min.sle_p;
	astn_count_t *cmax = max.sle_p;

	if (c1->astnc_node < cmin->astnc_node) {
		return (-1);
	}
	if (c1->astnc_node > cmax->astnc_node) {
		return (1);
	}
	return (0);
}

selem_t
count_astns(selem_t z, selem_t *e, uint64_t c)
{
	slablist_t *dups = z.sle_p;
	selem_t found;
	uint64_t i = 0;
	while (i < c) {
		lp_ast_node_t *a = e[i].sle_p;
		astn_count_t *ac = lp_mk_zbuf(sizeof (astn_count_t));
		ac->astnc_count = 0;
		ac->astnc_node = a;
		int f = slablist_find(dups, e[i], &found);
		if (f == 0) {
			astn_count_t *fnd = found.sle_p;
			fnd->astnc_count++;
		} else {
			selem_t acs;
			acs.sle_p = ac;
			slablist_add(dups, acs, 0);
		}
		i++;
	}
	return (z);
}

void
free_astnc(selem_t e)
{
	astn_count_t *a = e.sle_p;
	lp_rm_buf(a, sizeof (astn_count_t));
}

typedef struct ast_ref {
	lp_ast_t	*ar_ast;
	int		ar_ret;
} ast_ref_t;

selem_t
test_ast_ref(selem_t agg, selem_t *e, uint64_t sz)
{
	ast_ref_t *ar = agg.sle_p;
	if (ar->ar_ret) {
		return (agg);
	}
	lp_ast_t *ast = ar->ar_ast;
	uint64_t i = 0;
	while (i < sz) {
		lp_ast_node_t *n = e[i].sle_p;
		if (n->an_ast != ast) {
			ar->ar_ret = E_AST_REF;
		}
		i++;
	}
	return (agg);
}

/*
 * We sanity check the lp_ast_t struct.
 */
int
lp_test_ast(lp_ast_t *ast)
{
	selem_t zero;
	zero.sle_u = 0;
	if (ast->ast_in == NULL) {
		return (E_AST_NULL_INPUT);
	}
	if (ast->ast_sz == NULL) {
		return (E_AST_ZERO_SIZE_INPUT);
	}
	if (ast->ast_last_leaf != NULL) {
		uint32_t count = 0;
		lp_ast_node_t *l = ast->ast_last_leaf;
		while (l != NULL) {
			if (l->an_type == SPLITTER) {
				count++;
			}
			l = l->an_parent;
		}
		if (count > ast->ast_nsplit) {
			return (E_AST_NSPLIT_TOO_SMALL);
		}
		if (count < ast->ast_nsplit) {
			return (E_AST_NSPLIT_TOO_BIG);
		}
	}
	if (ast->ast_graph != NULL) {
		int r = lp_test_ast_graph(ast);
		if (r) {
			return (E_AST_GRAPH_WEIGHTS);
		}
	}
	ast_ref_t ar;
	ar.ar_ast = ast;
	ar.ar_ret = 0;
	zero.sle_p = ast;
	slablist_foldr(ast->ast_nodes, test_ast_ref, zero);
	return (ar.ar_ret);
}

/*
 * We sanity check the token.
 */
int
lp_test_tok(lp_tok_t *t)
{
	if (t->tok_nm == NULL) {
		return (E_TOK_NAME_NULL);
	}
	if (t->tok_segs == NULL) {
		return (E_TOK_SEGS_NULL);
	}
	if (slablist_get_elems(t->tok_segs) == 0) {
		return (E_TOK_SEGS_EMPTY);
	}
	return (0);
}

/*
 * We sanity check the tok_seg.
 */
int
lp_test_tok_seg(tok_seg_t *s)
{
	if (s->ts_op > 9) {
		return (E_TSEG_OP);
	}
	if (s->ts_width == 0) {
		/* TODO does it make sense to specify a zero-width seg? */
		return (E_TSEG_WIDTH_ZERO);
	}
	if (s->ts_data == NULL && s->ts_range_min == NULL) {
		return (E_TSEG_DATA);
	}
	return (0);
}
