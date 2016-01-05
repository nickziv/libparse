/*
 * This Source Code Form is subject to the terms of the Mozilla Public License,
 * v. 2.0. If a copy of the MPL was not distributed with this file, You can
 * obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2015, Nick Zivkovic
 */

#include "parse_impl.h"
#include "parse_provider.h"
#include "parse_test.h"
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>


/*
 * Some useful globals.
 */
selem_t ignored;
static int init = 0;


/*
 * TOKENS
 * ======
 *
 * Here is the token-related code. Everything that involves using a token-spec
 * to parse bits from input.
 */


/*
 * Adds a token to the token list.
 */
lp_tok_t *
lp_create_tok(lp_grmr_t *g, char *name)
{
	lp_tok_t *t = lp_mk_tok();
	lp_tok_ls_t *tl = g->grmr_toks;
	PARSE_CREATE_TOK(t);
	if (name == NULL) {
		return (NULL);
	}
	selem_t te;
	te.sle_p = t;
	t->tok_nm = name;
	t->tok_segs = NULL;
	int r = slablist_add(tl->tl_list, te, 0);
	if (r != SL_SUCCESS) {
		return (NULL);
	}
	return (t);
}

int
lp_add_tok_op(lp_tok_t *t, tok_op_t op, uint8_t width, size_t elems,
    char *data)
{
	if (op >= ROP_END) {
		return (-1);
	}
	if (t->tok_segs == NULL) {
		t->tok_segs = slablist_create("tok_segs", NULL, NULL,
			SL_ORDERED);
	}
	tok_seg_t *ts = lp_mk_tok_seg();
	size_t bytes = (width / 8) + (width % 8);
	bytes *= elems;
	/* We used to copy `data`. We now assume that it is constant */
	/* char *buf = lp_mk_zbuf(bytes); */
	ts->ts_op = op;
	ts->ts_width = width;
	ts->ts_elems = elems;
	ts->ts_data = data;
	selem_t sl_elem;
	sl_elem.sle_p = ts;
	/* bcopy(data, buf, bytes); */
	slablist_add(t->tok_segs, sl_elem, 0);
	if (PARSE_TEST_TOK_ENABLED()) {
		int test = lp_test_tok(t);
		PARSE_TEST_TOK(test);
	}
	if (PARSE_TEST_TOK_SEG_ENABLED()) {
		int test = lp_test_tok_seg(ts);
		PARSE_TEST_TOK(test);
	}
	return (0);
}

/*
 * This function retrieves the bits in `c` from `from` to `to`, and places them
 * in `copy`
 */
void
get_bits(char *c, char *copy, size_t from, size_t to)
{
	size_t skip_bytes = from / 8;
	size_t skip_bits = from % 8;
	char *s = (char *)(c + skip_bytes);
	size_t bits = to - from;	/* this is the width */
	size_t drop = 0;
	if (bits == 8) {
		*copy = *s;
	} else if (bits < 8) {
		drop = 8 - bits - skip_bits;
		*copy = (*s << skip_bits) >> skip_bits;
		*copy = (*copy >> drop) << drop;
	} else if (bits > 8) {
		size_t bytes = bits / 8;
		size_t rem_bits = bits % 8;
		bcopy(s, copy, bytes);
		s = s + bytes;
		char *cp = copy;
		if (rem_bits) {
			cp = copy + bytes;
			drop = 8 - rem_bits;
			*cp = (*s >> drop) << drop;
		}
	}
}

/*
 * Returns num of bits matched. If 0, then nothing was matched.
 */
int
has_allof(tok_seg_t *s, char *in, size_t off)
{
	size_t w = s->ts_width;
	//replace elems with bytes
	char *buf1 = lp_mk_buf(s->ts_elems);
	char *buf2 = lp_mk_buf(s->ts_elems);
	get_bits(in, buf1, off, off + s->ts_elems);
	bcopy(s->ts_data, buf2, s->ts_elems);
	uint64_t i = 0;
	uint64_t j = 0;
	int match = 1;
	while (i < s->ts_elems) {
		/*
		 * We didn't find a matching char, so buf2 doesn't have all of
		 * what's in buf1.
		 */
		if (match != 1) {
			lp_rm_buf(buf1, s->ts_elems);
			lp_rm_buf(buf2, s->ts_elems);
			return (0);
		}
		match = 0;
		char c1 = buf1[i];
		while (j < s->ts_elems) {
			char c2 = buf2[j];
			if (c1 == c2) {
				match = 1;
			}
			j++;
		}
		i++;
	}
	lp_rm_buf(buf1, s->ts_elems);
	lp_rm_buf(buf2, s->ts_elems);
	return (w * s->ts_elems);
}

int
has_one_plus(tok_seg_t *s, char *in, size_t bit_off)
{
	size_t w = s->ts_width;
	char buf[32];
	bzero(buf, 32);
	size_t off = bit_off;
	int c = 0;
	int consumed = 0;
	size_t bytes = (w / 8) + (w % 8);
	do {
		get_bits(in, buf, off, off + w);
		c = bcmp(buf, s->ts_data, bytes);
		off += w;
		consumed++;
	} while (c == 0);
	size_t bits = consumed * w;
	return (bits);
}

int
has_one(tok_seg_t *s, char *in, size_t bit_off)
{
	size_t w = s->ts_width;
	size_t bytes = (w / 8) + (w % 8);
	char buf[32];
	bzero(buf, 32);
	get_bits(in, buf, bit_off, bit_off + w);
	int c = bcmp(buf, s->ts_data, bytes);
	if (c != 0) {
		return (0);
	}
	return (w);
}

int
has_anyof(tok_seg_t *s, char *in, size_t bit_off)
{
	size_t w = s->ts_width;
	char buf[32];
	char data[32];
	bzero(buf, 32);
	bzero(data, 32);
	int num = s->ts_elems;
	int i = 0;
	int c = 1;
	size_t bytes = (w / 8) + (w % 8);
	while (i < num && c != 0) {
		get_bits(in, buf, bit_off, bit_off + w);
		get_bits(s->ts_data, data, i*w, (i*w)+w);
		/*
		char b1 = buf[0];
		char b2 = buf[1];
		char d1 = data[0];
		char d2 = data[1];
		*/
		c = bcmp(buf, data, bytes);
		i++;
	}
	if (c != 0) {
		return (0);
	}
	return (w);
}

int
has_noneof(tok_seg_t *s, char *in, size_t bit_off)
{
	size_t w = s->ts_width;
	int ret = has_anyof(s, in, bit_off);
	if (ret == 0) {
		return (w);
	}
	return (0);
}

int
has_anyof_one_plus(tok_seg_t *s, char *in, size_t bit_off)
{
	size_t w = s->ts_width;
	size_t off = bit_off;
	int r = 0;
	int consumed = 0;
	do {
		r = has_anyof(s, in, off);
		off += w;
		if (r != 0) {
			consumed++;
		}
	} while (r != 0);
	size_t bits = consumed * w;
	return (bits);
}

/*
 * Returns how many bits we consumed (not how many bytes).
 */
int
eval_tok_seg(lp_grmr_t *g, tok_seg_t *s,
    char *input, size_t sz, size_t bit_off)
{
	size_t c = 0;
	if (bit_off > (sz * 8)) {
		PARSE_EVAL_TOK(g, s->ts_op, 0);
		return (0);
	}
	/* when we match ZERO_ONE whitespace, it says we matched 8 bits, when
 	 * in fact we matched 0 */
	switch (s->ts_op) {

	case ROP_ZERO_ONE:
		c = has_one(s, input, bit_off);
		break;
	case ROP_ZERO_ONE_PLUS:
		c = has_one_plus(s, input, bit_off);
		break;
	case ROP_ONE:
		c = has_one(s, input, bit_off);
		break;
	case ROP_ONE_PLUS:
		c = has_one_plus(s, input, bit_off);
		break;
	case ROP_ALLOF:
		c = has_allof(s, input, bit_off);
		break;
	case ROP_ANYOF:
		c = has_anyof(s, input, bit_off);
		break;
	case ROP_NONEOF:
		c = has_noneof(s, input, bit_off);
		break;
	case ROP_ANYOF_ONE_PLUS:
		c = has_anyof_one_plus(s, input, bit_off);
		break;
	case ROP_ANYOF_ZERO_ONE_PLUS:
		c = has_anyof_one_plus(s, input, bit_off);
		break;
	case ROP_END:
	default:
		break;
	}
	PARSE_EVAL_TOK(g, s->ts_op, c);
	return (c);
}

lp_tok_t *
find_token(lp_grmr_t *g, char *nm)
{
	selem_t e;
	lp_tok_t tok;
	selem_t etok;
	etok.sle_p = &tok;
	tok.tok_nm = nm;
	int r = slablist_find(g->grmr_toks->tl_list, etok, &e);
	if (r == SL_ENFOUND) {
		return (NULL);
	}
	lp_tok_t *ret = e.sle_p;
	return (ret);
}


int
tok_cmp(selem_t e1, selem_t e2)
{
	lp_tok_t *t1 = e1.sle_p;
	lp_tok_t *t2 = e2.sle_p;
	int c = strcmp(t1->tok_nm, t2->tok_nm);
	return (c);
}

int
tok_bnd(selem_t e, selem_t min, selem_t max)
{

	lp_tok_t *t = e.sle_p;
	lp_tok_t *tmin = min.sle_p;
	lp_tok_t *tmax = max.sle_p;
	/*
	 * TODO rewrite this using 1 loop for all 3 strs.
	 */
	int c = strcmp(t->tok_nm, tmax->tok_nm);
	if (c > 0) {
		return (1);
	}
	c = strcmp(t->tok_nm, tmin->tok_nm);
	if (c < 0) {
		return (-1);
	}
	return (0);
}

/*
 * We create a token-index. We use this to refer to tokens by ID. Optionally,
 * we can use it to avoid duplicate tokens --- although this behavior is rarely
 * needed.
 */
lp_tok_ls_t *
lp_create_tokls()
{
	lp_tok_ls_t *ls = lp_mk_tok_ls();
	ls->tl_list = slablist_create("tok_list", tok_cmp, tok_bnd,
		SL_SORTED);
	return (ls);
}

/*
 * Grammar and AST Nodes
 * =====================
 *
 * Here is all the code the pertains to the grammar nodes and AST nodes and the
 * grammar-structure and AST-strucutre in which they are stored, respectively.
 */


int
an_cmp(selem_t e1, selem_t e2)
{
	lp_ast_node_t *i1 =  e1.sle_p;
	lp_ast_node_t *i2 =  e2.sle_p;
	if (i1->an_id < i2->an_id) {
		return (-1);
	}
	if (i1->an_id > i2->an_id) {
		return (1);
	}
	return (0);
}

int
an_bnd(selem_t e, selem_t min, selem_t max)
{
	lp_ast_node_t *i = e.sle_p;
	lp_ast_node_t *imin = min.sle_p;
	lp_ast_node_t *imax = max.sle_p;
	if (i->an_id > imax->an_id) {
		return (1);
	}
	if (i->an_id < imin->an_id) {
		return (-1);
	}
	return (0);
}

lp_ast_t *
lp_create_ast(void)
{
	lp_ast_t *r = lp_mk_ast();
	r->ast_nodes = slablist_create("ast_il*", an_cmp, an_bnd, SL_SORTED);
	r->ast_graph = lg_create_wdigraph();
	PARSE_CREATE_AST(r);
	return (r);
}

void
lp_destroy_ast(lp_ast_t *r)
{
	(void)r;
	/* destroy AST and all associated data */
}

lp_grmr_t *
lp_create_grammar(char *name)
{
	if (init == 0) {
		(void)parse_umem_init();
		init = 1;
	}
	lp_grmr_t *g = lp_mk_grmr();
	lp_tok_ls_t *ls = lp_create_tokls();

	g->grmr_name = name;
	g->grmr_toks = ls;
	g->grmr_graph = lg_create_wdigraph();
	PARSE_CREATE_GRMR(g);
	return (g);
}


void
lp_destroy_grammar(lp_grmr_t *g)
{
	(void)g;
	/* destroy all data associated with grammar */
}

int
gn_cmp(selem_t e1, selem_t e2)
{
	/*
	 * Here we have to compare the names of 2 grmr_nodes. We have 4 cases:
	 *
	 *	i1	i2
	 *	pk	pk
	 *	pt	pt
	 *	pk	pt
	 *	pt	pk
	 *
	 * Where pk means packed and pt means pointed. We only need 2 bits to
	 * represent this. We can store this in a small number. And use a
	 * switch. This automatically means 1 comparison per case instead of 2.
	 *
	 * We could also implement this as a jump-table of length 4, which
	 * contains function pointers for each specific case.
	 *
	 * We would just need to know if the function call cost is greater than
	 * or less than the br-cmp cost. We could store x86 asm in an array in
	 * this function, and use that --- however that's too much for now. We
	 * could also use longjmp within this function, before we start the
	 * comparison. Again call cost vs bc-cmp cost.
	 *
	 * Starting out with simple string pointers, and will optimize later.
	 *
	 * Also we can start out with a var `f = 0`. Each time we get the first
	 * bit, we just += that var. Also, storing in pk, means that we have to
	 * bcopy pk into a pt to cmp with strcmp. pk/pk and pt/pt are easy to
	 * implement, but mixed is harder (and potentially more expensive).
	 * bcopy if there is no trailing ZERO in the string (i.e. the string is
	 * _fully_ 8 bytes).
	 */
	lp_grmr_node_t *i1 =  e1.sle_p;
	lp_grmr_node_t *i2 =  e2.sle_p;
	int c = strcmp(i1->gn_name, i2->gn_name);
	return (c);
	/* This works for 
	if (i1->gn_name < i2->gn_name) {
		return (-1);
	}
	if (i1->gn_name > i2->gn_name) {
		return (1);
	}
	return (0);
	*/
}

int
gn_bnd(selem_t e, selem_t min, selem_t max)
{
	/*
	 * TODO rewrite this in a way that doesn't use strcmp. A loop that
	 * compares all 3 strings simultaneously.
	 */
	lp_grmr_node_t *i = e.sle_p;
	lp_grmr_node_t *imin = min.sle_p;
	lp_grmr_node_t *imax = max.sle_p;
	int c = strcmp(i->gn_name, imax->gn_name);
	if (c > 0) {
		return (1);
	}
	c = strcmp(i->gn_name, imin->gn_name);
	if (c < 0) {
		return (-1);
	}
	return (0);
}


void
add_ast_node(lp_ast_t *ast, lp_ast_node_t *ast_node)
{
	selem_t ie;
	ie.sle_p = ast_node;
	slablist_add(ast->ast_nodes, ie, 0);
}

int
add_grmr_node(lp_grmr_t *g, lp_grmr_node_t *grmr_node)
{
	selem_t ie;
	ie.sle_p = grmr_node;
	int r = slablist_add(g->grmr_gnodes, ie, 0);
	return (r);
}

void
rem_grmr_node(lp_grmr_t *g, uint64_t irid)
{
	selem_t ge;
	ge.sle_u = irid;
	slablist_rem(g->grmr_gnodes, ge, 0, NULL);
}

/*
 * Returns a status. 0 on success, -1 on fail.
 */
int
lp_create_grmr_node(lp_grmr_t *g, char *name, char *tok,
    n_type_t ntype)
{
	/*
	 * Grammar nodes that are private to libparse can only begin with '_'.
	 */
	if (name[0] == '_') {
		return (-1);
	}
	lp_grmr_node_t *grmr_node = lp_mk_grmr_node();
	if (g->grmr_gnodes == NULL) {
		g->grmr_gnodes = slablist_create("grmr_nodes", gn_cmp,
		    gn_bnd, SL_SORTED);
	}
	grmr_node->gn_name = name;
	grmr_node->gn_type = ntype;
	if (ntype == PARSER) {
		grmr_node->gn_tok = tok;
	}

	int r = add_grmr_node(g, grmr_node);
	if (r != SL_SUCCESS) {
		return (-1);
	}
	PARSE_CREATE_GRMR_NODE(g, grmr_node, tok);
	return (0);
}

lp_grmr_node_t *
find_grmr_node(lp_grmr_t *g, char *name)
{
	lp_grmr_node_t gn;
	gn.gn_name = name;
	selem_t find;
	find.sle_p = &gn;
	selem_t ret;
	int f = slablist_find(g->grmr_gnodes, find, &ret);
	if (f != SL_SUCCESS) {
		return (NULL);
	} else {
		return (ret.sle_p);
	}
}

lp_ast_node_t *lp_create_ast_node(lp_grmr_node_t *, lp_ast_t *);

lp_ast_node_t *
lp_create_ast_node(lp_grmr_node_t *gn, lp_ast_t *ast)
{
	lp_ast_node_t *a = lp_mk_ast_node();
	a->an_id = slablist_get_elems(ast->ast_nodes);
	a->an_ast = ast;
	add_ast_node(ast, a);
	a->an_gnm = gn->gn_name;
	PARSE_CREATE_AST_NODE(ast->ast_grmr, a);
	return (a);
}

/*
 * Sets a root-grmr_node.
 */
int
lp_root_grmr_node(lp_grmr_t *g, char *nm)
{
	lp_grmr_node_t *grmr_node = find_grmr_node(g, nm);
	if (grmr_node != NULL) {
		g->grmr_root = grmr_node;
		PARSE_SET_ROOT(g, nm);
	} else {
		return (-1);
	}
	return (0);
}


/*
 * Gives a grmr_node a child.
 */
int
lp_add_child(lp_grmr_t *g, char *parent, char *child)
{
	lp_grmr_node_t *p = NULL;
	lp_grmr_node_t *c = NULL;
	p = find_grmr_node(g, parent);
	c = find_grmr_node(g, child);
	if (p == NULL) {
		return (-1);
	}
	if (c == NULL) {
		return (-2);
	}

	/*
	 * Parsers should be leaf nodes only.
	 */
	if (p->gn_type == PARSER) {
		return (-4);
	}

	/*
	 * A grammar node cannot refer to itself directly recursively. This
	 * would only make sense on an input of infinite size. On a finite
	 * input, the last iteration of the recursion would fail, resulting in
	 * the whole node failing . A splitter must be between each iteration.
	 * This way, that failure causes libparse to rewind to the point before
	 * the last iteration failed and to take an alternative branch.
	 */
	if (p->gn_name == c->gn_name) {
		return (-5);
	}
	gelem_t pg;
	gelem_t cg;
	gelem_t w;
	pg.ge_p = p;
	cg.ge_p = c;
	w.ge_u = p->gn_kids;
	p->gn_kids++;
	int ret = lg_wconnect(g->grmr_graph, pg, cg, w);
	PARSE_GRMR_ADD_CHILD(g, parent, child, w.ge_u);
	return (ret);
}

/*
 * Scrubs a grammar and reports any error...
 */
int
lp_scrub_grammar(lp_grmr_t *g)
{
	(void) g;
	return (0);
	/* XXX what are the possible errors */
	/*
	 * Two tokens that have the exact same tok.
	 * It is impossible to have an grmr_node that never expands to a
	 * token.
	 *
	 * OPTIONALLY: Warn or error out when two _different_ grmr_nodes can
	 * consume/match the same input --- but interpret it differently.
	 *
	 * It is impossible to have a grmr_node that is directly or
	 * inderictly recursive.
	 *
	 * Verify that a root grmr_node has been set.
	 *
	 * Verify that both grmr_nodes that are bound by a grouper grmr_node
	 * are "in band".
	 *	They must be on the same "level" of the heirarchy, they can't
	 *	span multiple levels (it makes no sense).
	 *
	 * Binding grmr_nodes can't share starting or ending grmr_nodes.
	 *
	 * Binding grmr_nodes can't repeat, ever.
	 */
}

int
lp_finalize_grammar(lp_grmr_t *g)
{
	g->grmr_fin = 1;
	return (0);
	/*
	int s = lp_scrub_grammar(g);
	if (s == 0) {
		g->grmr_fin = 1;
	}
	return (s);
	*/
}


void
lp_rm_an_cb(selem_t e)
{
	lp_ast_node_t *il = e.sle_p;
	PARSE_DESTROY_AST_NODE(il);
	/*
	 * We remove this ast_node from the index.
	 */
	selem_t ile;
	ile.sle_p = il;
	slablist_rem(il->an_ast->ast_nodes, ile, 0, NULL);
	if (il->an_content != NULL) {
		size_t bufsz = (il->an_content->ctt_bits / 8) +
		    (il->an_content->ctt_bits  % 8);

		lp_rm_buf(il->an_content->ctt_buf, bufsz);

		lp_rm_content(il->an_content);
	}
	lp_rm_ast_node(il);
}

selem_t
parse_tok_segs(selem_t z, selem_t *e, uint64_t sz)
{
	lp_ast_t *ast = z.sle_p;
	selem_t last = slablist_end(ast->ast_stack);
	lp_ast_node_t *an = last.sle_p;
	if (an->an_left != NULL) {
		an->an_off_start = an->an_left->an_off_end;
		PARSE_AST_NODE_OFF_START(ast->ast_grmr, an);
	} else if (an->an_parent != NULL) {
		an->an_off_start = an->an_parent->an_off_start;
		PARSE_AST_NODE_OFF_START(ast->ast_grmr, an);
	}
	uint64_t i = 0;
	int total_matched = 0;
	while (i < sz) {
		tok_seg_t *seg = e[i].sle_p;
		int matched = eval_tok_seg(ast->ast_grmr, seg, ast->ast_in,
		    ast->ast_sz, an->an_off_start);
		total_matched += matched;
		if ((matched == 0 && (seg->ts_op != ROP_ZERO_ONE &&
		    seg->ts_op != ROP_ZERO_ONE_PLUS &&
		    seg->ts_op != ROP_ANYOF_ZERO_ONE_PLUS &&
		    seg->ts_op != ROP_NONEOF)) || (matched > 0 &&
		    (seg->ts_op == ROP_NONEOF))) {
			an->an_state = ANS_FAIL;
			PARSE_FAIL(an);
			break;
		} else {
			an->an_state = ANS_MATCH;
			PARSE_MATCH(an);
			if (an->an_parent->an_type == SPLITTER) {
				an->an_parent->an_state = ANS_MATCH;
				PARSE_MATCH(an->an_parent);
			}
		}
		i++;
		if (PARSE_TEST_TOK_SEG_ENABLED()) {
			int test = lp_test_tok_seg(seg);
			PARSE_TEST_TOK(test);
		}
	}
	an->an_off_end = an->an_off_start + (uint32_t)total_matched;
	PARSE_AST_NODE_OFF_END(ast->ast_grmr, an);
	if (0 && an->an_off_end == ast->ast_sz) {
		ast->ast_eoi = 1;
	}
	if (an->an_state == ANS_MATCH) {
		ast->ast_matched += total_matched;
	} else {
		ast->ast_matched = 0;
	}
	if (PARSE_TEST_AST_NODE_ENABLED()) {
		int test = lp_test_ast_node(an);
		PARSE_TEST_AST_NODE(test);
	}
	return (z);
}
/*
 * Forward declaration of the ast-stack functions and graph functions.
 */
void ast_push_astn(lp_ast_t *ast, lp_ast_node_t *an);
void ast_pop_astn(lp_ast_t *ast);
void lp_rem_ast_child(lp_ast_node_t *p, lp_ast_node_t *c);
void lp_add_ast_child(lp_ast_node_t *p, lp_ast_node_t *c);

void
try_parse(lp_grmr_node_t *gn, lp_ast_t *ast)
{
	lp_tok_t *t = find_token(ast->ast_grmr, gn->gn_tok);
	slablist_t *rsl = t->tok_segs;
	selem_t sl_ast;
	ast->ast_matched = 0;
	sl_ast.sle_p = ast;
	PARSE_PARSE();
	slablist_foldr(rsl, parse_tok_segs, sl_ast);
}

void trace_ast(gelem_t a, gelem_t b, gelem_t c);

void
lp_rem_ast_child(lp_ast_node_t *p, lp_ast_node_t *c)
{
	if (PARSE_TEST_AST_NODE_ENABLED()) {
		int test = lp_test_ast_node(p);
		PARSE_TEST_AST_NODE(test);
		test = lp_test_ast_node(c);
		PARSE_TEST_AST_NODE(test);
	}
	lg_graph_t *ast_graph = p->an_ast->ast_graph;
	lp_grmr_t *grmr = p->an_ast->ast_grmr;
	gelem_t gp;
	gelem_t gc;
	gelem_t weight;
	gp.ge_p = p;
	gc.ge_p = c;
	weight.ge_u = c->an_index;
	/*
	 * We remove the parent-child relationship between these two nodes in
	 * the ast graph.
	 */
	lg_wdisconnect(ast_graph, gp, gc, weight);

	/*
	 * We update the linked list of neighbors embedded in the child nodes,
	 * as well as the pointer to the parent's rightmost child.
	 */
	if (c == p->an_last_child) {
		p->an_last_child = c->an_left;
		if (p->an_last_child != NULL) {
			p->an_last_child->an_right = NULL;
		}
	} else {
		if (c->an_right != NULL) {
			c->an_right->an_left = c->an_left;
		}
		if (c->an_left != NULL) {
			c->an_left->an_right = c->an_right;
		}
	}
	p->an_kids--;
	c->an_parent = NULL;
	PARSE_AST_REM_CHILD(grmr, p, c);
	if (PARSE_TRACE_AST_ENABLED()) {
		PARSE_TRACE_AST_BEGIN();
		lg_edges(p->an_ast->ast_graph, trace_ast);
		PARSE_TRACE_AST_END();
	}
}

/*
 * This function disconnects two nodes, but doesn't bother updating metadata.
 * Intended to be used when removing subtrees.
 */
void
lp_rem_ast_child_lite(lp_ast_node_t *p, lp_ast_node_t *c)
{
	/*
	 * Note that we get the ast graph from the child node, and not the
	 * parent node. When removing a subtree, a parent node can be NULL, but
	 * the child node is always non-NULL.
	 */
	lg_graph_t *ast_graph = c->an_ast->ast_graph;
	gelem_t gp;
	gelem_t gc;
	gelem_t weight;
	gp.ge_p = p;
	gc.ge_p = c;
	weight.ge_u = c->an_index;
	/*
	 * We remove the parent-child relationship between these two nodes in
	 * the ast graph.
	 */
	lg_wdisconnect(ast_graph, gp, gc, weight);
}


/*
 * If the ast_node we pop failed to parse, we remove it from memory. We also
 * percolate the failure up 1 level to the parent, unless the parent is a
 * splitter. However, we can't percolate successful matches, since a successful
 * match is all or nothing. In other words we won't know if a node succeeded
 * until we call on_pop on it. Unless the parent in a splitter, in which case
 * we percolate the first successfully matched child. However, the splitter
 * success is handled from on_pop() because it directly influences the DFS
 * routine.
 */
void
ast_pop_astn(lp_ast_t *ast)
{
	uint64_t e = slablist_get_elems(ast->ast_stack);
	selem_t last = slablist_end(ast->ast_stack);
	lp_ast_node_t *l = last.sle_p;
	PARSE_AST_POP(l);
	lp_ast_node_t *p = l->an_parent;
	if (l->an_state == ANS_FAIL) {
		if (p != NULL) {
			lp_rem_ast_child(p, l);
			if (p->an_type == SEQUENCER) {
				p->an_state = ANS_FAIL;
				PARSE_FAIL(p);
			}
		}
		lp_rm_ast_node(l);
	}
	if (l->an_state == ANS_MATCH) {
		/*
		 * We percolate the ending offset to the parent.
		 */
		if (p != NULL) {
			p->an_off_end = l->an_off_end;
			PARSE_AST_NODE_OFF_END(ast->ast_grmr, p);
		}
	}
	if (e > 0) {
		if (l->an_type == SPLITTER) {
			ast->ast_nsplit--;
		}
		slablist_rem(ast->ast_stack, ignored, e - 1, NULL);
	}
	if (PARSE_TEST_AST_ENABLED()) {
		int test = lp_test_ast(ast);
		PARSE_TEST_AST(test);
	}
}

void ast_rem_subtree(lp_ast_t *ast);
/*
 * This function keeps popping ast nodes from the stack until it hits a
 * splitter. It returns 0 if it has something to rewind to, and 1 if not.
 */
int
ast_rewind(lp_ast_t *ast)
{
	PARSE_REWIND_BEGIN();
	if (!(ast->ast_nsplit)) {
		PARSE_REWIND_END(1);
		return (1);
	}
	uint64_t e = slablist_get_elems(ast->ast_stack);
	selem_t last = slablist_end(ast->ast_stack);
	lp_ast_node_t *l = last.sle_p;
	/* We are already at the top level slitter */
	if (ast->ast_nsplit == 1 && l->an_type == SPLITTER) {
		PARSE_REWIND_END(0);
		return (0);
	}

	while (1) {
		lp_ast_node_t *p = l->an_parent;
		if (e > 0) {
			PARSE_AST_POP(l);
			slablist_rem(ast->ast_stack, ignored, e - 1, NULL);
			if (l->an_type == SPLITTER) {
				ast->ast_nsplit--;
			}
		}

		if (p != NULL) {
			if (p->an_type == SEQUENCER) {
				p->an_state = ANS_FAIL;
				PARSE_FAIL(p);
			} else if (p->an_type == SPLITTER) {
				ast_rem_subtree(ast);
			}
		}

		e = slablist_get_elems(ast->ast_stack);
		last = slablist_end(ast->ast_stack);
		l = last.sle_p;
		if (l->an_type == SPLITTER) {
			break;
		}
	}
	PARSE_REWIND_END(0);
	if (PARSE_TEST_AST_ENABLED()) {
		int test = lp_test_ast(ast);
		PARSE_TEST_AST(test);
	}
	return (0);
}

void
handle_rewind_failure(lp_ast_t *ast, int failed)
{
	if (failed) {
		ast->ast_bail = 1;
	}
	return;
}

void
lp_queue_removal(lp_ast_node_t *p, lp_ast_node_t *c)
{
	lp_ast_t *ast = p->an_ast;
	lg_graph_t *g = ast->ast_to_remove;
	gelem_t gp;
	gelem_t gc;
	gelem_t weight;
	gp.ge_p = p;
	gc.ge_p = c;
	weight.ge_u = c->an_index;

	lg_wconnect(g, gp, gc, weight);
}

/*
 *  We remove an edge from the graph. We use the index-value in the ast_node as
 *  the weight value. It is, however, a bug in libgraph that we do not pass the
 *  weight to the callback. When this bug is closed, this callback will have to
 *  be updated (TODO).
 */
void
queue_subtree_edge(gelem_t to, gelem_t from, gelem_t ignored)
{
	(void)ignored;
	lp_ast_node_t *parent = from.ge_p;
	lp_ast_node_t *child = to.ge_p;
	lp_queue_removal(parent, child);
}

void
rem_subtree_edge(gelem_t from, gelem_t to, gelem_t weight)
{
	(void)weight;
	lp_ast_node_t *parent = from.ge_p;
	lp_ast_node_t *child = to.ge_p;
	lp_rem_ast_child_lite(parent, child);
	lp_rm_ast_node(child);
}

/*
 * Given an AST node, it will remove all of its descendants. It uses BFS to do
 * this.
 */
void
ast_rem_subtree(lp_ast_t *ast)
{
	PARSE_REM_SUBTREE_BEGIN();
	ast->ast_to_remove = lg_create_wdigraph();
	selem_t last = slablist_end(ast->ast_stack);
	lp_ast_node_t *n = last.sle_p;
	PARSE_REM_SUBTREE_ROOT(n);
	gelem_t gn;
	gelem_t ignored;
	gn.ge_p = n;

	/*
	 * This function walks the subtree and queues all of its edges for removal.
	 */
	lg_bfs_fold(ast->ast_graph, gn, queue_subtree_edge, NULL, ignored);
	lg_edges(ast->ast_to_remove, rem_subtree_edge);
	lg_destroy_graph(ast->ast_to_remove);
	ast->ast_to_remove = NULL;
	PARSE_REM_SUBTREE_END();
}

/*
 * Forward declaration of on_push.
 */
int on_push(gelem_t, gelem_t, gelem_t *);


/*
 * This function is called from `on_pop` and does all of the necessary updates
 * to SEQUENCER nodes.
 */
void
on_pop_handle_sequencer(lp_ast_t *ast, lp_ast_node_t *a_top)
{
	a_top->an_state = ANS_MATCH;
	PARSE_MATCH(a_top);
	lp_ast_node_t *c = a_top->an_last_child;
	if (a_top->an_last_child != NULL && c->an_state == ANS_MATCH) {
		a_top->an_off_end = a_top->an_last_child->an_off_end;
		PARSE_AST_NODE_OFF_END(ast->ast_grmr, a_top);
	}
	if (a_top->an_parent != NULL &&
	    a_top->an_parent->an_type == SPLITTER) {
		a_top->an_parent->an_state = ANS_MATCH;
		PARSE_MATCH(a_top->an_parent);
	}
	if (PARSE_TEST_AST_ENABLED()) {
		int test = lp_test_ast(ast);
		PARSE_TEST_AST(test);
	}
}

int on_pop(gelem_t gn, gelem_t state);

/*
 * This gets called when we finish with a node.
 */
int
on_pop(gelem_t gn, gelem_t state)
{
	lp_ast_t *ast = state.ge_p;
	lp_grmr_node_t *node = gn.ge_p;
	selem_t ast_elem = slablist_end(ast->ast_stack);
	lp_ast_node_t *a_top = ast_elem.sle_p;
	int ret = 0;
	/*
	 * We want to update the AST and the stack. Updates to the AST involve
	 * changing the values of the ast_node_t's members. And updates to the
	 * stack involve changes to the `ast_stack` member of the AST. How we
	 * modify the AST depends on what kind of AST node we have popped.
	 * Remember, libgraph calls the pop-callback (i.e. this function), once
	 * it has popped the grammar node from its internal DFS stack. We
	 * maintain our own AST node stack, which needs to mirror libgraph's
	 * stack -- the stacks must be of the same depth, and the i'th  element
	 * in the AST node stack must have the same `n_type_t` value. The stack
	 * updates we make merely guarantee that this congruence holds.
	 *
	 * The AST updates happen, ultimately, as a result of the success and
	 * failure of the PARSE nodes. If we successfully consume an input we
	 * keep building up the tree. If we fail to consume the next part of
	 * the input, we delete the subtree of the most recent SPLITTER node,
	 * and try its next child. If all of its kids fail, we repeat this
	 * rewinding process, until we exhaust all SPLITTERs.
	 *
	 * As far as this function is concerned, it receives a node that's one
	 * of three types: PARSER, SPLITTER, SEQUENCER. This node can have a
	 * parent that's one of _two_ types: SPLITTER, SEQUENCER. If we are
	 * popping a PARSER node, we try to use it to consume some amount of
	 * input. If we failed, we rewind (if possible) and percolate the
	 * failure to the parent node. If we succeeded we percolate the success
	 * to the parent node and let the DFS carry the execution of the
	 * grammar forward. We direct the execution using return values. A
	 * return value of 0 tells the DFS to proceed as normal. A return value
	 * of 1 tells the DFS to skip the visit to the next child of the
	 * parent, and instead to pop again. A return value of 2 tells the DFS
	 * that we are rewind our stack to the nearest SPLITTER and that it
	 * should so so as well. If we fail, we return 2. If we succeed, what
	 * we return depends on the parent. If the parent is a SEQUENCER, we
	 * return 0. If it is a SPLITTER we return 1.
	 *
	 * If we are popping a SEQUENCER node. We know it was successful (if it
	 * had any failed PARSER descendents or SPLITTER descendents, it would
	 * have been nuked in a rewind).  We only need to give care to the
	 * return values. We either return 0 or 1. Only PARSER and SPLITTER
	 * nodes can return 2.  We return 1 if the parent is a SPLITTER, and 0
	 * if not.
	 *
	 * If we are popping a SPLITTER node, and it has failed, we return a 2
	 * and rewind (if possible). If the SPLITTER has succeeded, we either
	 * return a 0 or a 1. If 0, the parent is not a SPLITTER and 1 if it is
	 * a SPLITTER.
	 */

	int rewind_failed = 0;
	switch (a_top->an_type) {

	case PARSER:
		try_parse(node, ast);
		if (a_top->an_state == ANS_FAIL) {
			ret = 2;
			rewind_failed = ast_rewind(ast);
			handle_rewind_failure(ast, rewind_failed);
			return (ret);
		}
		if (a_top->an_parent->an_type == SPLITTER) {
			ret = 1;
			ast_pop_astn(ast);
			return (ret);
		} else if (a_top->an_parent->an_type == SEQUENCER) {
			ret = 0;
			ast_pop_astn(ast);
			return (ret);
		}
		break;

	case SEQUENCER:
		if (a_top->an_state == ANS_TRY) {
			on_pop_handle_sequencer(ast, a_top);
		}
		if (a_top->an_state == ANS_MATCH &&
		    a_top->an_parent != NULL &&
		    a_top->an_parent->an_type == SPLITTER) {
			a_top->an_parent->an_state = ANS_MATCH;
			PARSE_MATCH(a_top->an_parent);
			ret = 1;
			ast_pop_astn(ast);
			return (ret);
		}
		ret = 0;
		ast_pop_astn(ast);
		return (ret);
		break;

	case SPLITTER:
		if (a_top->an_state != ANS_MATCH) {
			ret = 2;
			rewind_failed = ast_rewind(ast);
			handle_rewind_failure(ast, rewind_failed);
			return (ret);
		} else {
			if (a_top->an_parent != NULL) {
				a_top->an_parent->an_state = ANS_MATCH;
				PARSE_MATCH(a_top->an_parent);
				if (a_top->an_parent->an_type == SPLITTER) {
					ret = 1;
					ast_pop_astn(ast);
					return (ret);
				} else {
					ret = 0;
					ast_pop_astn(ast);
					return (ret);
				}
			}
		}
		break;
	}

	return (ret);
}

void
ast_push_astn(lp_ast_t *ast, lp_ast_node_t *an)
{
	selem_t p;
	p.sle_p = an;
	PARSE_AST_PUSH(an);
	if (ast->ast_stack == NULL) {
		ast->ast_stack = slablist_create("ast_stack", NULL, NULL,
		    SL_ORDERED);
		ast->ast_start = an;
	}
	if (an->an_type == SPLITTER) {
		ast->ast_nsplit++;
	}
	/* This is the minimum possible value of off_end */
	an->an_off_end = an->an_off_start;
	slablist_add(ast->ast_stack, p, 0);
	if (PARSE_TEST_AST_ENABLED()) {
		int test = lp_test_ast(ast);
		PARSE_TEST_AST(test);
	}
	if (PARSE_TEST_AST_NODE_ENABLED()) {
		int test = lp_test_ast_node(an);
		PARSE_TEST_AST_NODE(test);
	}
}

int
any_child_matched(lp_ast_node_t *n)
{
	if (n->an_kids < 1) {
		return (0);
	}

	lp_ast_node_t *c = n->an_last_child;
	while (c != NULL) {
		if (c->an_state) {
			return (1);
		}
		c = c->an_left;
	}
	return (0);
}

lp_ast_node_t *
get_last_ast_node(lp_ast_t *ast)
{
	uint64_t elems = 0;
	if (ast->ast_stack != NULL) {
		elems = slablist_get_elems(ast->ast_stack);
	}
	selem_t last;
	lp_ast_node_t *last_astn = NULL;
	if (elems > 0) {
		last = slablist_end(ast->ast_stack);
		last_astn = last.sle_p;
		return (last_astn);
	}
	return (NULL);
}

lp_ast_node_t *
maybe_create_ast_node(lp_ast_t *ast, lp_grmr_node_t *node)
{
	lp_ast_node_t *p = get_last_ast_node(ast);
	/*
	 * We don't create an AST node if the parent is a matched splitter
	 * (i.e. 1 of its kids were matched). XXX We also don't create an AST
	 * node if the parent is a failed sequencer.
	 */
	if (p != NULL && ((p->an_type == SPLITTER &&
	    p->an_state == ANS_MATCH) || p->an_gnm == node->gn_name ||
	    (p->an_type == SEQUENCER && p->an_state == ANS_FAIL))) {
		return (NULL);
	}
	lp_ast_node_t *n = lp_create_ast_node(node, ast);
	n->an_type = node->gn_type;
	return (n);
}

void
lp_add_ast_child(lp_ast_node_t *p, lp_ast_node_t *c)
{
	gelem_t glast;
	glast.ge_p = p;
	gelem_t next;
	next.ge_p = c;
	gelem_t weight;
	if (PARSE_TEST_ADD_CHILD_ENABLED()) {
		int e = lp_test_add_child(p, c);
		PARSE_TEST_ADD_CHILD(e);
	}
	/*
	 * So, we handle splitters differently from every other type of ast
	 * node. A splitter can only ever have 1 child. 
	 */
	if (p->an_type != SPLITTER) {
		weight.ge_u = p->an_kids;
		c->an_index = p->an_kids;
		p->an_kids++;
		c->an_left = p->an_last_child;
		if (c->an_left != NULL) {
			c->an_left->an_right = c;
		}
		c->an_parent = p;
		p->an_last_child = c;
	} else {
		weight.ge_u = 0;
		c->an_index = 0;
		p->an_kids = 1;
		c->an_left = NULL;
		c->an_parent = p;
		p->an_last_child = c;
	}
	lg_graph_t *ast_graph = p->an_ast->ast_graph;
	lp_grmr_t *grmr = p->an_ast->ast_grmr;
	lg_wconnect(ast_graph, glast, next, weight);
	PARSE_AST_ADD_CHILD(grmr, p, c);
	if (PARSE_TRACE_AST_ENABLED()) {
		PARSE_TRACE_AST_BEGIN();
		lg_edges(p->an_ast->ast_graph, trace_ast);
		PARSE_TRACE_AST_END();
	}
	if (PARSE_TEST_AST_NODE_ENABLED()) {
		int test = lp_test_ast_node(p);
		PARSE_TEST_AST_NODE(test);
		test = lp_test_ast_node(c);
		PARSE_TEST_AST_NODE(test);
	}
}

/*
 * XXX we don't use `ast` at all in this function...
 */
void
lp_set_ast_offsets(lp_ast_t *ast, lp_ast_node_t *n)
{
	(void)ast;
	lp_ast_node_t *p = n->an_parent;
	if (p != NULL && (p->an_type == SPLITTER ||
	    n->an_left == NULL)) {
		n->an_off_start = p->an_off_start;
		if (p->an_type == SPLITTER) {
			PARSE_SPLIT(p);
			if (any_child_matched(p)) {
				p->an_state = ANS_MATCH;
				PARSE_MATCH(p);
			}
		}
	} else if (p != NULL) {
		n->an_off_start = n->an_left->an_off_end;
	} else {
		n->an_off_start = 0;
	}
}


int
on_split(gelem_t n)
{
	lp_grmr_node_t *gn = n.ge_p;
	if (gn->gn_type == SPLITTER) {
		return (1);
	}
	return (0);
}

/*
 * XXX This comment, or something like it probably belongs above the `run`
 * function or in the _impl function.
 */
/*
 * When we push, we check to the type of the node. If the node is a SEQUENCER,
 * we just make a corresponding ast_node, change the state, and leave the
 * function, letting the DFS function move on the children. If the node is a
 * SPLITTER, we do the same thing we would do with the SEQUENCER, except we
 * ignore parse failuers. The children of a SPLITTER don't represent a
 * sequence, but rather parallel, alternate futures. If the node is a PARSER,
 * we simply try to parse the input, and -- regardless of success or failure --
 * we continue the DFS.
 */
int
on_push(gelem_t state, gelem_t gn, gelem_t *ignored)
{
	(void)ignored;
	lp_ast_t *ast = state.ge_p;
	if (ast->ast_bail) {
		return (1);
	}
	lp_grmr_node_t *node = gn.ge_p;
	lp_ast_node_t *an = NULL;
	lp_ast_node_t *last_astn = NULL;

	an = maybe_create_ast_node(ast, node);
	if (an == NULL) {
		return (0);
	}
	last_astn = get_last_ast_node(ast);
	if (last_astn != NULL) {
		lp_add_ast_child(last_astn, an);
	}

	lp_set_ast_offsets(ast, an);

	PARSE_AST_NODE_OFF_START(ast->ast_grmr, an);
	ast_push_astn(ast, an);
	return (0);
}

void
trace_ast(gelem_t from, gelem_t to, gelem_t weight)
{
	lp_ast_node_t *p = from.ge_p;
	lp_ast_node_t *c = to.ge_p;
	uint64_t num = weight.ge_u;
	PARSE_TRACE_AST(p->an_ast, p, c, num);
	if (PARSE_TEST_AST_NODE_ENABLED()) {
		int test = lp_test_ast_node(p);
		PARSE_TEST_AST_NODE(test);
		test = lp_test_ast_node(c);
		PARSE_TEST_AST_NODE(test);
	}
}

/*
 * This function runs the grammar `g` on an input `in` of size `sz`, and stores
 * the result in `ast`. It walks the grammar from the root, in DFS order until
 * completion.
 */
int
lp_run_grammar(lp_grmr_t *g, lp_ast_t *ast, void *in, size_t sz)
{
	/*
	 * For every grmr_node we successfully match we need a corresponding
	 * ast_node.  Even the non-leaf grmr_nodes.
	 */
	PARSE_RUN_GRMR_BEGIN(g, ast);
	ast->ast_stack = NULL;
	ast->ast_in = in;
	ast->ast_sz = sz;
	ast->ast_grmr = g;
	gelem_t root;
	root.ge_p = g->grmr_root;
	gelem_t state;
	state.ge_p = ast;
	/*
	 * 
	 */
	lg_dfs_br_rdnt_fold(g->grmr_graph, root, on_split, on_pop, on_push,
	    state);
	PARSE_RUN_GRMR_END(g, ast);
	/*
	 * This is here as a filler. What _should_ we return, anyway?
	 */
	return (0);
}



int
node_print(gelem_t agg, gelem_t gn, gelem_t *aggp)
{
	(void)aggp;
	(void)agg;
	lp_grmr_node_t *node = gn.ge_p;
	printf("VISITED: %s\n", node->gn_name);
	return (0);
}

void
relation(gelem_t c, gelem_t p, gelem_t opt)
{
	(void)opt;
	lp_grmr_node_t *pnode = p.ge_p;
	lp_grmr_node_t *cnode = c.ge_p;
	printf("CROSSING: %s -> %s\n", pnode->gn_name, cnode->gn_name);
}

int
lp_bfs_walk_grammar(lp_grmr_t *g, lp_ast_t *ast, void *in, size_t sz)
{
	ast->ast_stack = NULL;
	ast->ast_in = in;
	ast->ast_sz = sz;
	ast->ast_grmr = g;
	gelem_t root;
	root.ge_p = g->grmr_root;
	gelem_t state;
	lp_grmr_node_t *gn = root.ge_p;
	printf("NAME: %s\n", gn->gn_name);
	state.ge_p = ast;
	lg_bfs_fold(g->grmr_graph, root, relation, node_print, state);
	return (0);
}

int
lp_dfs_walk_grammar(lp_grmr_t *g, lp_ast_t *ast, void *in, size_t sz)
{
	(void)g;
	(void)ast;
	(void)in;
	(void)sz;
	return (0);

}

/*
 * This function tells the system that the astult can be finalized as there is
 * no more input.
 */
int
lp_finish_run(lp_ast_t *ast)
{
	uint64_t foo = (uint64_t)ast;
	ast->ast_fin = 1;
	return (foo);
}


/*
 * It seems that a grammar is clonable if it passes a scrub.
 *
 * The issue is taht if we clone a grammar 100x, we do a scrub or some similar
 * check 100x. We want to do the check once, and to never do it again. So when
 * we finish a grammar, we want to call something like lp_grmr_fin() to
 * finilize the grammar. Unfortunately, this doesn't deal with a grammar that
 * has modified itself (perhaps extensively). We should make it illegal to
 * modify a grammar without cloning it first. This gives the grammar COW
 * semantics.
 */

lp_grmr_t *
lp_clone_grammar(char *nm, lp_grmr_t *g)
{
	(void)nm;
	return (g);
}

void
print_grmr_edge(gelem_t from, gelem_t to, gelem_t weight)
{
	lp_grmr_node_t *f = from.ge_p;
	lp_grmr_node_t *t = to.ge_p;
	uint64_t w = weight.ge_u;

	printf("EDGE: %s -> %s [%lu]\n", f->gn_name, t->gn_name, w);
}

void
lp_dump_grmr(lp_grmr_t *g)
{
	lg_edges(g->grmr_graph, print_grmr_edge);
}


void
lp_walk_grmr_dfs(lp_grmr_t *g, lp_grmr_cb_t cb)
{
	(void)g; (void)cb;

}

void
lp_walk_grmr_bfs(lp_grmr_t *g, lp_grmr_cb_t cb)
{
	(void)g; (void)cb;

}

void
lp_walk_ast_dfs(lp_grmr_t *g, lp_ast_cb_t cb)
{
	(void)g; (void)cb;

}

void
lp_walk_ast_bfs(lp_grmr_t *g, lp_ast_cb_t cb)
{
	(void)g; (void)cb;

}
