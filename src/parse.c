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
lp_add_tok_op_impl(lp_tok_t *t, tok_op_t op, uint8_t width, size_t elems,
    char *data, char *min, char *max, range_flag_t rf)
{
	if (op >= ROP_END) {
		return (-1);
	}
	if ((min != NULL || max != NULL) && data != NULL) {
		return (-1);
	}
	if (t->tok_segs == NULL) {
		t->tok_segs = slablist_create("tok_segs", NULL, NULL,
			SL_ORDERED);
	}
	tok_seg_t *ts = lp_mk_tok_seg();
	ts->ts_op = op;
	ts->ts_width = width;
	if (data != NULL) {
		/* we used to copy `data`. we now assume that it is constant */
		/* char *buf = lp_mk_zbuf(bytes); */
		ts->ts_elems = elems;
		ts->ts_data = data;
	} else {
		ts->ts_range_min = min;
		ts->ts_range_max = max;
		ts->ts_range_flag = rf;
	}
	selem_t sl_elem;
	sl_elem.sle_p = ts;
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

int
lp_add_tok_range_op(lp_tok_t *t, tok_op_t op, uint8_t width,
    char *min, char *max, range_flag_t flag)
{
	return (lp_add_tok_op_impl(t, op, width, 0, NULL, min, max, flag));
}

int
lp_add_tok_op(lp_tok_t *t, tok_op_t op, uint8_t width, size_t elems,
    char *data)
{
	return (lp_add_tok_op_impl(t, op, width, elems, data, NULL, NULL,
	    RF_NA));
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
 * We make `get_bits` available to consumers.
 */
void
lp_get_bits(char *c, char *copy, size_t f, size_t t)
{
	get_bits(c, copy, f, t);
}

static size_t
range_sz_discont(char *min, char *max, uint8_t width)
{
	int bytes = width / 8;
	int i = 0;
	size_t flippables = 0;
	while (i < bytes) {
		char tmp = min[i] & max[i];
		char bit = 0;
		int j = 0;
		while (j < 8) {
			get_bits(&tmp, &bit, j, j);
			if (bit == 0) {
				flippables++;
			}
			j++;
		}
		i++;
	}
	return (1 << flippables);
}

/*
 * This function determines if buf is in the range specified in the tok_seg. If
 * the buf is not in range, it returns non-zero. If it is in range, it will
 * return zero if ts_range_flag is set to RF_CONT. If the flag is set to
 * RF_DISCONT, it will make sure that buf isn't in one of the inferred gaps in
 * the range.
 *
 * Say we created a segment with the following min and max pair: 010, 111. If
 * the flag RF_CONT was set, we infer that the middle bit is always 1. This
 * means that even though 100 is in the continuous range, it is not in the
 * discontinuous range.
 */
int
rangecmp(char *buf, tok_seg_t *s, size_t bytes)
{
	char *range_min = s->ts_range_min;
	char *range_max = s->ts_range_max;
	size_t i = 0;
	int gt_min = 0;
	int lt_min = 0;
	while (i < bytes) {
		/* no reason to compare rest of array */
		if (buf[i] > range_min[i]) {
			gt_min = 1;
			break;
		}
		if (buf[i] < range_min[i]) {
			lt_min = 1;
			break;
		}
		i++;
	}
	int under_range = lt_min && !gt_min;
	if (under_range) {
		return (-1);
	}
	i = 0;
	int gt_max = 0;
	int lt_max = 0;
	while (i < bytes) {
		/* no reason to compare rest of array */
		if (buf[i] < range_max[i]) {
			lt_max = 1;
			break;
		}
		if (buf[i] > range_max[i]) {
			gt_max = 1;
			break;
		}
		i++;
	}
	int over_range = gt_max && !lt_max;
	if (over_range) {
		return (1);
	}
	if (s->ts_range_flag == RF_DISCONT) {
		i = 0;
		while (i < bytes) {
			char bitspec = range_max[i] ^ range_min[i];
			char rmin  = range_min[i];
			char rmin_bit;
			char buf_bit;
			char any;
			int j = 0;
			while (j < 8) {
				any = 0;
				get_bits(&bitspec, &any, j, j);
				if (!any) {
					rmin_bit = 0;
					buf_bit = 0;
					get_bits(&rmin, &rmin_bit, j, j);
					get_bits(&buf[i], &buf_bit, j, j);
					if (rmin_bit != buf_bit) {
						return (1);
					}
				}
				j++;
			}
			i++;
		}
	}
	return (0);
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
	if (s->ts_data != NULL) {
		do {
			get_bits(in, buf, off, off + w);
			c = bcmp(buf, s->ts_data, bytes);
			off += w;
			consumed++;
		} while (c == 0);
	} else {
		do {
			get_bits(in, buf, off, off + w);
			c = rangecmp(buf, s, bytes);
			off += w;
			consumed++;
		} while (c == 0);
	}
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
	int c;
	if (s->ts_data != NULL) {
		int i = 0;
		while (i < bytes) {
			i++;
		}
		c = bcmp(buf, s->ts_data, bytes);
		if (c != 0) {
			return (0);
		}
	} else {
		c = rangecmp(buf, s, bytes);
		if (c != 0) {
			return (0);
		}
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
	if (s->ts_data != NULL) {
		while (i < num && c != 0) {
			get_bits(in, buf, bit_off, bit_off + w);
			get_bits(s->ts_data, data, i*w, (i*w)+w);
			c = bcmp(buf, data, bytes);
			int j = 0;
			while (j < bytes) {
				j++;
			}
			i++;
		}
	} else {
		while (i < num && c != 0) {
			get_bits(in, buf, bit_off, bit_off + w);
			get_bits(s->ts_data, data, i*w, (i*w)+w);
			c = rangecmp(buf, s, bytes);
			i++;
		}
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
	case ROP_ANYOF:
		c = has_anyof(s, input, bit_off);
		break;
	case ROP_ANYOF_ZERO_ONE:
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
	lp_ast_node_t *i1 = e1.sle_p;
	lp_ast_node_t *i2 = e2.sle_p;
	int cmp = strcmp(i1->an_gnm, i2->an_gnm);
	if (cmp > 0) {
		return (1);
	}
	if (cmp < 0) {
		return (-1);
	}
	if (i1->an_id > i2->an_id) {
		return (1);
	}
	if (i1->an_id < i2->an_id) {
		return (-1);
	}
	return (0);
}

int
an_bnd(selem_t e, selem_t min, selem_t max)
{
	int cmp = an_cmp(e, min);
	if (cmp < 0) {
		return (-1);
	}
	cmp = an_cmp(e, max);
	if (cmp > 0) {
		return (1);
	}
	return (0);
}

void lp_destroy_ast_node(lp_ast_node_t *);

/*
 * The callback increments or decrements a reference count in the ast-node. The
 * reference count indicates that the node is present in a some number of
 * changes plus some number of edges. `inc` indicates if we should increment or
 * decrement the count.
 */
void
astn_refc_cb(uint8_t inc, gelem_t s, gelem_t d, gelem_t w)
{
	(void)w;
	lp_ast_node_t *sn = s.ge_p;
	lp_ast_node_t *dn = d.ge_p;
	if (inc) {
		sn->an_srefc++;
		dn->an_srefc++;
	} else {
		sn->an_srefc--;
		dn->an_srefc--;
	}

	/*
	 * If the refcount is 0, we destroy the ast nodes. This only takes
	 * place during a rollback, so this code only affects nodes that are
	 * part of a snapshot and thus have a splitter for an ancestor.
	 */
	lp_ast_node_t *p = NULL;
	if (!sn->an_srefc) {
		p = sn->an_parent;
		if (p->an_type == SPLITTER) {
			p->an_last_child = NULL;
			p->an_kids = 0;
		}
		lp_destroy_ast_node(sn);
	}
	if (!dn->an_srefc) {
		p = dn->an_parent;
		if (p->an_type == SPLITTER) {
			p->an_last_child = NULL;
			p->an_kids = 0;
		}
		lp_destroy_ast_node(dn);
	}
}

lp_ast_t *
lp_create_ast(void)
{
	lp_ast_t *r = lp_mk_ast();
	r->ast_nodes = slablist_create("ast_nodes", an_cmp, an_bnd, SL_SORTED);
	r->ast_graph = lg_create_wdigraph();
	lg_snapshot_cb(r->ast_graph, astn_refc_cb);
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

char *
lp_grammar_name(lp_grmr_t *g)
{
	return (g->grmr_name);
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
	(void)slablist_add(ast->ast_nodes, ie, 0);
}

void
rem_ast_node(lp_ast_t *ast, lp_ast_node_t *ast_node)
{
	selem_t ie;
	ie.sle_p = ast_node;
	(void)slablist_rem(ast->ast_nodes, ie, 0, NULL);
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
	/*
	 * We set the ID to the pointer, because it is unique and will allow us
	 * to do ranged comparisons.
	 */
	a->an_id = (uint64_t)a;
	a->an_ast = ast;
	a->an_gnm = gn->gn_name;
	add_ast_node(ast, a);
	PARSE_CREATE_AST_NODE(ast->ast_grmr, a);
	return (a);
}

void
lp_destroy_ast_node(lp_ast_node_t *n)
{
	rem_ast_node(n->an_ast, n);
	PARSE_DESTROY_AST_NODE(n);
	lp_rm_ast_node(n);
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

typedef struct scrub_arg {
	lp_grmr_t *sa_grmr;
	lp_scrub_cb_t *sa_cb;
} scrub_arg_t;

selem_t
parent_check_fold(selem_t z, selem_t *e, uint64_t sz)
{
	lp_grmr_t *grmr;
	lp_scrub_cb_t *cb;
	scrub_arg_t *sa;
	sa = z.sle_p;
	grmr = sa->sa_grmr;
	cb = sa->sa_cb;

	uint64_t i = 0;
	while (i < sz) {
		lp_grmr_node_t *g = e[i].sle_p;
		if (g->gn_type != PARSER && g->gn_kids == 0) {
			grmr->grmr_scrub_err = -1;
			cb(SCRUB_NOKIDS, g->gn_name);
		}
		i++;
	}
	return (z);
}

/*
 * Scrubs a grammar and reports any error. The nature of the error is reflected
 * in the return value. The pointer to the name of the offending grammar node
 * is saved in `name`.
 */
int
lp_scrub_grammar(lp_grmr_t *g, lp_scrub_cb_t cb)
{
	/*
	 * Verify that a root grmr_node has been set.
	 * Verify that every non-PARSER grammar node has children.
	 */
	scrub_arg_t sa;
	sa.sa_cb = cb;
	sa.sa_grmr = g;
	selem_t zero;
	zero.sle_p = &sa;
	if (g->grmr_root == NULL) {
		/* in case we need to get at this err later */
		g->grmr_scrub_err = SCRUB_NOROOT;
		cb(SCRUB_NOROOT, NULL);
		return (-1);
	}
	slablist_foldr(g->grmr_gnodes, parent_check_fold, zero);
	return (g->grmr_scrub_err);
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


selem_t
parse_tok_segs(selem_t z, selem_t *e, uint64_t sz)
{
	lp_ast_t *ast = z.sle_p;
	lp_ast_node_t *an = ast->ast_last_leaf;
	if (an->an_left != NULL) {
		an->an_off_start = an->an_left->an_off_end;
		PARSE_AST_NODE_OFF_START(ast->ast_grmr, an);
	} else if (an->an_parent != NULL) {
		an->an_off_start = an->an_parent->an_off_start;
		PARSE_AST_NODE_OFF_START(ast->ast_grmr, an);
	}
	uint64_t i = 0;
	int total_matched = 0;
	/*
	 * The general idea is that we keep parsing segments. Unless we fail to
	 * parse a segment that _must_ be parsed (i.e. is not optional).
	 */
	while (i < sz) {
		tok_seg_t *seg = e[i].sle_p;
		int matched = eval_tok_seg(ast->ast_grmr, seg, ast->ast_in,
		    ast->ast_sz, an->an_off_start + total_matched);
		total_matched += matched;
		if ((matched == 0 && (seg->ts_op != ROP_ZERO_ONE &&
		    seg->ts_op != ROP_ZERO_ONE_PLUS &&
		    seg->ts_op != ROP_ANYOF_ZERO_ONE &&
		    seg->ts_op != ROP_ANYOF_ZERO_ONE_PLUS &&
		    seg->ts_op != ROP_NONEOF)) || (matched > 0 &&
		    (seg->ts_op == ROP_NONEOF))) {
			an->an_state = ANS_FAIL;
			PARSE_FAIL(an);
			/*
			 * We may have changed the splitter's state due to an
			 * earlier match. We need to undo that.
			 */
			if (an->an_parent->an_type == SPLITTER) {
				an->an_parent->an_state = ANS_TRY;
				//PARSE_MATCH(an->an_parent);
			}
			break;
		} else {
			if (an->an_state != ANS_FAIL) {
				an->an_state = ANS_MATCH;
				PARSE_MATCH(an);
				if (an->an_parent->an_type == SPLITTER) {
					an->an_parent->an_state = ANS_MATCH;
					PARSE_MATCH(an->an_parent);
				}
			}
		}
		i++;
		if (PARSE_TEST_TOK_SEG_ENABLED()) {
			int test = lp_test_tok_seg(seg);
			PARSE_TEST_TOK(test);
		}
	}
	an->an_off_end = an->an_off_start + (uint32_t)total_matched;
	if (an->an_ast->ast_max_off < an->an_off_end) {
		an->an_ast->ast_max_off = an->an_off_end;
	}
	PARSE_AST_NODE_OFF_END(ast->ast_grmr, an);
	if (an->an_off_end == ast->ast_sz) {
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
 * until we call on_pop on it. Unless the parent is a splitter, in which case
 * we percolate the first successfully matched child. However, the splitter
 * success is handled from on_pop() because it directly influences the DFS
 * routine.
 */
void
ast_pop_astn(lp_ast_t *ast)
{
	lp_ast_node_t *l = ast->ast_last_leaf;
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
		// XXX should probably remove this...
		// lp_destroy_ast_node(l);
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
	if (l->an_type == SPLITTER) {
		ast->ast_nsplit--;
		PARSE_NSPLIT_DEC(ast->ast_nsplit);
	}
	ast->ast_last_leaf = p;
	if (PARSE_TEST_AST_ENABLED()) {
		int test = lp_test_ast(ast);
		PARSE_TEST_AST(test);
	}
}

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
	lp_ast_node_t *l = ast->ast_last_leaf;
	/* We are already at the top level splitter */
	if (ast->ast_nsplit == 1 && l->an_type == SPLITTER) {
		PARSE_REWIND_END(0);
		return (0);
	}

	do {
		lp_ast_node_t *p = l->an_parent;
		PARSE_AST_POP(l);
		ast->ast_last_leaf = p;
		if (l->an_type == SPLITTER) {
			ast->ast_nsplit--;
			PARSE_NSPLIT_DEC(ast->ast_nsplit);
		}

		if (p != NULL) {
			if (p->an_type == SPLITTER) {
				lg_rollback(ast->ast_graph, p->an_snap);
				/*
				 * We snapshot a root-splitter after we connect
				 * it to a child. This means we have to remove
				 * its child manually. This is not true for any
				 * other splitter in the ast graph.
				 */
				if (p == ast->ast_start) {
					lp_rem_ast_child(p, p->an_last_child);
				}
			}
		}

		l = p;
		if (l->an_type == SPLITTER) {
			break;
		}
	} while (l->an_type != SPLITTER);


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
	/*
	selem_t child;
	child.sle_p = c;
	int r = slablist_add(ast->ast_freelist, child, 0);
	*/
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
	lp_ast_node_t *a_top = ast->ast_last_leaf;
	int ret = 0;
	/*
	 * XXX TODO update the below paragraph, since we have an implicit
	 * stack, instead of an explicit one.
	 *
	 * We want to update the AST and the stack. Updates to the AST involve
	 * changing the values of the ast_node_t's members. And updates to the
	 * stack involve changes to the `ast_last_leaf` member of the AST. How
	 * we modify the AST depends on what kind of AST node we have popped.
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
	 * should do so as well. If we fail, we return 2. If we succeed, what
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
	PARSE_AST_PUSH(an);
	if (ast->ast_last_leaf == NULL) {
		ast->ast_start = an;
	}
	ast->ast_last_leaf = an;
	if (an->an_type == SPLITTER) {
		ast->ast_nsplit++;
		PARSE_NSPLIT_INC(ast->ast_nsplit);
	}
	/* This is the minimum possible value of off_end */
	an->an_off_end = an->an_off_start;
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
maybe_create_ast_node(lp_ast_t *ast, lp_grmr_node_t *node)
{
	lp_ast_node_t *p = ast->ast_last_leaf;
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

/* The weight-radix */
#define WADIX 10000
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
		weight.ge_u = WADIX * (p->an_kids + 1);
		c->an_index = weight.ge_u;
		p->an_kids++;
		c->an_left = p->an_last_child;
		if (c->an_left != NULL) {
			c->an_left->an_right = c;
		}
		c->an_parent = p;
		p->an_last_child = c;
	} else {
		weight.ge_u = WADIX;
		c->an_index = WADIX;
		p->an_kids = 1;
		c->an_left = NULL;
		c->an_parent = p;
		p->an_last_child = c;
	}
	lg_graph_t *ast_graph = p->an_ast->ast_graph;
	lp_grmr_t *grmr = p->an_ast->ast_grmr;
	lg_wconnect(ast_graph, glast, next, weight);
	/*
	 * We snapshot whenever we add a splitter to a parent. Unless the root
	 * node itself is a splitter, because it _has_ no parent. For the
	 * root-case we snapshot when we add a child to the root, unless that
	 * child is itself a splitter. This will result in the root-snapshot
	 * taking 1 ast-node worth of space more than a non-root-snapshot, but
	 * whatever. All this means is that when we rollback to the
	 * root-splitter, we have to remove its child manually.
	 */
	if (p->an_type == SPLITTER && p->an_ast->ast_start == p &&
	    c->an_type != SPLITTER) {
		p->an_snap = lg_snapshot(ast_graph);
	} else if (c->an_type == SPLITTER) {
		c->an_snap = lg_snapshot(ast_graph);
	}
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
 * This function is intended to allow the user to edit the AST _after_ it has
 * already been constructed. It is not used while parsing. We can only add
 * children _to the left of_ an existing ast node. To add a child to the right
 * of an AST node, you have to call this function on its next neighbor. If it
 * has no next neighbor, just use the lp_add_ast_child() function above.
 */
void
lp_add_ast_child_left(lp_ast_node_t *n, lp_ast_node_t *l)
{
	gelem_t weight;
	weight.ge_u = n->an_index - 1;
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
	if (PARSE_TEST_AST_ENABLED()) {
		int e = lp_test_ast(ast);
		PARSE_TEST_AST(e);
	}
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
	last_astn = ast->ast_last_leaf;
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
	ast->ast_last_leaf = NULL;
	ast->ast_in = in;
	ast->ast_sz = sz;
	ast->ast_grmr = g;
	PARSE_GOT_HERE(ast);
	PARSE_GOT_HERE(g);
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
	if (ast->ast_max_off < sz) {
		printf("MAX_OFF: %u\n", ast->ast_max_off);
		printf("SZ: %lu\n", sz);
		printf("EOI: %d\n", ast->ast_eoi);
		return (-1);
	}
	return (0);
}



/*
 * This function tells the system that the result can be finalized as there is
 * no more input.
 */
void
lp_finish_run(lp_ast_t *ast)
{
	ast->ast_fin = 1;
}

void
map_neighbors(gelem_t f, gelem_t t, gelem_t w, gelem_t arg)
{
	(void)f;
	(void)t;
	(void)w;
	lp_map_search_t *s = arg.ge_p;
	if (s->ms_key == t.ge_p) {
		s->ms_key_found++;
	}
	if (s->ms_val == t.ge_p) {
		s->ms_val_found++;
	}
}

int
mapping_cmp(selem_t a, selem_t b)
{
	lp_mapping_t *am = a.sle_p;
	lp_mapping_t *bm = b.sle_p;
	return (strcmp(am->map_name, bm->map_name));
}

int
mapping_bnd(selem_t a, selem_t min, selem_t max)
{
	int mincmp = mapping_cmp(a, min);
	if (mincmp < 0) {
		return (-1);
	}
	int maxcmp = mapping_cmp(a, max);
	if (maxcmp > 0) {
		return (1);
	}
	return (0);
}

void
map_adjcb(gelem_t to, gelem_t from, gelem_t weight, gelem_t cookie)
{
	(void)weight;
	lp_map_cookie_t *c = cookie.ge_p;
	lp_grmr_node_t *K = c->mc_ms->ms_key;
	lp_grmr_node_t *V = c->mc_ms->ms_val;
	lp_grmr_node_t *P = c->mc_ms->ms_par;
	lp_ast_node_t *par = from.ge_p;
	lp_ast_node_t *a = to.ge_p;
	if (strcmp(P->gn_name, par->an_gnm) == 0) {
		if (par != c->mc_p) {
			c->mc_found = 1;
		}
		if (strcmp(K->gn_name, a->an_gnm) == 0) {
			c->mc_k = a;
			c->mc_found++;
		} else if (strcmp(V->gn_name, a->an_gnm) == 0) {
			c->mc_v = a;
			c->mc_found++;
		}
		if (c->mc_found == 3) {
			/*
			 * Now we can add to the index.
			 */
			lg_graph_t *g = c->mc_mapping->map_graph;
			gelem_t key;
			gelem_t val;
			gelem_t w;
			key.ge_p = c->mc_k;
			val.ge_p = c->mc_v;
			w.ge_u = c->mc_v->an_off_start;
			lg_wconnect(g, key, val, w);
			c->mc_found = 0;
		}
	}
}


/*
 * Map Child to Child:
 * -------------------
 * This function creates a multimap (implemented as a graph_t for expediency).
 * We name the multimap `nm`, and we map the child `v` of `p` to child `k` of
 * `p`. We can only map nodes that share the same parent. The index can later
 * be accessed by its name `nm`. The index is stored in the lp_ast_t.
 *
 * This function processes an AST that has already been parsed. We _could_ have
 * implemented it to process an AST _as_ it's being built, however, this
 * implementation is easier to reason about. The on-the-fly method would
 * increase the parse-time, but _may_ reduce overall time. Or maybe not.
 * Requires some research.
 */
int
lp_map_cc(lp_ast_t *a, char *nm, char *p, char *k, char *v)
{
	if (nm == NULL || p == NULL || k == NULL || v == NULL || a == NULL) {
		return (-1);
	}
	lp_map_search_t ms;
	bzero(&ms, sizeof (ms));
	gelem_t arg;
	arg.ge_p = &ms;

	/* Make sure that `p`, `k`, and `v` exist in grmr */
	lp_grmr_node_t *P = find_grmr_node(a->ast_grmr, p);
	lp_grmr_node_t *K = find_grmr_node(a->ast_grmr, k);
	lp_grmr_node_t *V = find_grmr_node(a->ast_grmr, v);

	if (P == NULL || K == NULL || V == NULL) {
		return (-1);
	}
	ms.ms_par = P;
	ms.ms_key = K;
	ms.ms_val = V;
	/* Make sure that `p`, has kids `k` and `v` */
	gelem_t gP;
	gP.ge_p = P;
	lg_neighbors_arg(a->ast_grmr->grmr_graph, gP, map_neighbors, arg);
	/* We return if we found nothing */
	if (!(ms.ms_key_found && ms.ms_val_found)) {
		return (-1);
	}
	/*
	 * We return if any of the kids are not unique -- would need a more
	 * complicated interface to support this feature. Namely one that
	 * allowed specification of graph-weights.
	 */
	if (ms.ms_key_found > 1 || ms.ms_val_found > 1) {
		return (-1);
	}

	/* Create index, add to AST's index list */
	lp_mapping_t *m = lp_mk_mapping();
	m->map_name = nm;
	m->map_graph = lg_create_wdigraph();
	selem_t sm;
	sm.sle_p = m;
	if (a->ast_mappings == NULL) {
		a->ast_mappings = slablist_create("mappings", mapping_cmp,
		    mapping_bnd, SL_SORTED);
	}
	int f = slablist_add(a->ast_mappings, sm, 0);
	if (f == SL_EDUP) {
		lp_rm_mapping(m);
		return (-1);
	}
	/* Use a BFS to achieve this result */
	lp_map_cookie_t c;
	bzero(&c, sizeof (lp_map_cookie_t));
	c.mc_ms = &ms;
	gelem_t cookie;
	cookie.ge_p = &c;
	gelem_t start;
	start.ge_p = a->ast_start;
	lg_bfs_fold(a->ast_graph, start, map_adjcb, NULL, cookie);
	return (0);
}

selem_t
map_pd_fold(selem_t z, selem_t *e, uint64_t sz)
{
	lp_mapping_t *m = z.sle_p;
	uint64_t i = 0;
	while (i < sz) {
		lp_ast_node_t *n = e[i].sle_p;
		lp_ast_node_t *p = n->an_parent;
		while (p != NULL) {
			int c = strcmp(p->an_gnm, m->map_pd_par);
			if (c) {
				gelem_t gp;
				gelem_t gd;
				gelem_t w;
				gp.ge_p = p;
				gd.ge_p = n;
				w.ge_u = n->an_off_start;
				lg_wconnect(m->map_graph, gp, gd, w);
				break;
			}
			p = p->an_parent;
		}
		i++;
	}
	return (z);
}


/*
 * Map Parent to Descendant(s):
 * ----------------------------
 *
 * This function creats a multimap that maps all parents of type `p` to all of
 * the descendants of type `d`. Given a descendant `D` it will get mapped to
 * the nearest ancestor of type `p`.
 */
int
lp_map_pd(lp_ast_t *ast, char *mapnm, char *p, char *d)
{
	lp_ast_node_t *nmin = lp_mk_ast_node();
	nmin->an_id = 1;
	nmin->an_gnm = d;

	lp_ast_node_t *nmax = lp_mk_ast_node();
	nmin->an_id = UINT64_MAX;
	nmax->an_gnm = d;
	lp_mapping_t *m = lp_mk_mapping();
	m->map_name = mapnm;

	selem_t sm;
	sm.sle_p = m;
	if (ast->ast_mappings == NULL) {
		ast->ast_mappings = slablist_create("mappings", mapping_cmp,
		    mapping_bnd, SL_SORTED);
	}
	int f = slablist_add(ast->ast_mappings, sm, 0);
	if (f == SL_EDUP) {
		lp_rm_mapping(m);
		return (-1);
	}
	m->map_graph = lg_create_wdigraph();
	m->map_pd_par = p;


	selem_t zero;
	selem_t smax;
	selem_t smin;
	smin.sle_p = nmin;
	smax.sle_p = nmax;
	zero.sle_p = m;
	slablist_foldr_range(ast->ast_nodes, map_pd_fold, smin, smax, zero);
	lp_rm_ast_node(nmin);
	lp_rm_ast_node(nmax);
	return (0);
}

void
map_neighbors_cb(gelem_t s, gelem_t d, gelem_t w, gelem_t arg)
{
	(void)s;
	(void)w;
	map_query_arg_t *mqa = arg.ge_p;
	lp_ast_node_t *v = d.ge_p;
	mqa->mqa_cb(v, mqa->mqa_arg);
}


void
lp_map_query(lp_ast_t *a, char *map, lp_ast_node_t *key, lp_map_query_cb_t cb,
    void *arg)
{
	lp_mapping_t find_map;
	find_map.map_name = map;
	selem_t sfind_map;
	sfind_map.sle_p = &find_map;
	selem_t found;
	int r = slablist_find(a->ast_mappings, sfind_map, &found);
	if (r) {
		return;
	}
	lp_mapping_t *mapping = found.sle_p;
	gelem_t gkey;
	gkey.ge_p = key;

	map_query_arg_t mqa;
	bzero(&mqa, sizeof (map_query_arg_t));
	mqa.mqa_cb = cb;
	mqa.mqa_arg = arg;

	gelem_t garg;
	garg.ge_p = &mqa;

	lg_neighbors_arg(mapping->map_graph,gkey, &map_neighbors_cb, garg);
}

/*
 * This function returns 0 if `buf` is identical to the contents of `c`, and 1
 * if not. Note that we assume that any unused trailing bits in `buf` are zero.
 * For example if you have a 9 bit buffer, we expect that the last 7 bits of
 * the second byte are all 0.
 */
int
lp_cmp_contents(char *buf, size_t sz, lp_ast_node_t *c)
{
	size_t an_sz = c->an_off_end - c->an_off_start;
	if (an_sz != sz) {
		return (1);
	}
	char *copy = lp_mk_buf(an_sz/8);
	get_bits(c->an_ast->ast_in, copy, c->an_off_start, c->an_off_end);
	int r = bcmp(buf, copy, sz);
	lp_rm_buf(copy, an_sz/8);
	return (r);
}

void
lp_copy_contents(lp_ast_node_t *c, char *copy)
{
	get_bits(c->an_ast->ast_in, copy, c->an_off_start, c->an_off_end);
}

char *
lp_get_contents(lp_ast_node_t *c)
{
	size_t an_sz = c->an_off_end - c->an_off_start;
	char *copy = lp_mk_buf((an_sz / 8) + 1);
	get_bits(c->an_ast->ast_in, copy, c->an_off_start, c->an_off_end);
	copy[(an_sz / 8)] = 0;
	return (copy);
}

size_t
lp_get_bitwidth(lp_ast_node_t *c)
{
	size_t an_sz = c->an_off_end - c->an_off_start;
	return (an_sz);
}

void
lp_rem_contents(lp_ast_node_t *n, char *c)
{
	size_t an_sz = n->an_off_end - n->an_off_start;
	lp_rm_buf(c, (an_sz / 8) + 1);
}

lp_ast_node_t *
lp_deref_splitter(lp_ast_node_t *s)
{
	if (s == NULL || s->an_last_child == NULL || s->an_type != SPLITTER) {
		return (NULL);
	}
	return (s->an_last_child);
}

lp_ast_node_t *
lp_get_root_node(lp_ast_t *a)
{
	return (a->ast_start);
}

char *
lp_get_node_name(lp_ast_node_t *n)
{
	return (n->an_gnm);
}

int
lp_cmp_name(lp_ast_node_t *an, char *nm)
{
	return (strcmp(an->an_gnm, nm));
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

	printf("EDGE: %s(%d) %lu %s\n", f->gn_name, f->gn_type, w, t->gn_name);
}

void
lp_dump_grmr(lp_grmr_t *g)
{
	lg_edges(g->grmr_graph, print_grmr_edge);
}

void
print_ast_edge(gelem_t from, gelem_t to, gelem_t weight)
{
	lp_ast_node_t *f = from.ge_p;
	lp_ast_node_t *t = to.ge_p;
	uint64_t w = weight.ge_u;

	printf("EDGE: %s -> %s [%lu]\n", f->an_gnm, t->an_gnm, w);
	if (t->an_parent != f) {
		if (t->an_parent == NULL) {
			printf("WARN: Parent of %s is NULL\n", t->an_gnm);
		} else {
			printf("WARN: Parent of %s is %s\n", t->an_gnm,
			    t->an_parent->an_gnm);
		}
	}
}

void
lp_dump_ast(lp_ast_t *ast)
{
	lg_edges(ast->ast_graph, print_ast_edge);
}

char *tok_op_tab[] = {
	"ROP_ZERO_ONE", "ROP_ZERO_ONE_PLUS",
	"ROP_ONE", "ROP_ONE_PLUS", "ROP_ANYOF",
	"ROP_ANYOF_ONE_PLUS", "ROP_ANYOF_ZERO_ONE",
	"ROP_ANYOF_ZERO_ONE_PLUS", "ROP_NONEOF"
};

selem_t
print_tok_seg(selem_t z, selem_t *e, uint64_t sz)
{
	uint64_t i = 0;
	while (i < sz) {
		tok_seg_t *t = e[i].sle_p;
		printf("\t\tOP: %s\n", tok_op_tab[t->ts_op]);
		printf("\t\tWD: %u\n", t->ts_width);
		printf("\t\tEL: %u\n", t->ts_elems);
		printf("\t\tDA: %p\n", t->ts_data);
		printf("\t\tMI: %p\n", t->ts_range_min);
		printf("\t\tMA: %p\n", t->ts_range_max);
		printf("\t\tFL: %u\n", t->ts_range_flag);
		i++;
	}
	return (z);
}

selem_t
print_grmr_node(selem_t z, selem_t *e, uint64_t sz)
{
	uint64_t i = 0;
	lp_grmr_t *grmr = z.sle_p;
	while (i < sz) {
		lp_grmr_node_t *g = e[i].sle_p;
		printf("GNODE:\n\tTYPE: %d\n\tKIDS: %lu\n", g->gn_type,
		    g->gn_kids);
		printf("\tNAME: %s\n\tTOK: %s\n", g->gn_name, g->gn_tok);
		if (g->gn_tok) {
			lp_tok_t *tok = find_token(grmr, g->gn_tok);
			slablist_foldr(tok->tok_segs, print_tok_seg, ignored);
		}
		i++;
	}
	return (z);
}

void
lp_dump_gnodes(lp_grmr_t *g)
{
	selem_t grmr;
	grmr.sle_p = g;
	slablist_foldr(g->grmr_gnodes, print_grmr_node, grmr);
}


void
lp_grmr_dfs(lp_grmr_t *g, char *s, lp_grmr_cb_t cb)
{
	(void)g; (void)cb;

}

void
lp_grmr_bfs(lp_grmr_t *g, char *s, lp_grmr_cb_t cb)
{
	(void)g; (void)cb;

}

void
lp_ast_dfs(lp_ast_t *g, lp_ast_node_t *s, lp_ast_cb_t cb, void *arg)
{
	(void)g; (void)cb; (void)arg;

}


void
lp_ast_bfs(lp_ast_t *g, lp_ast_node_t *s, lp_ast_cb_t cb, void *arg)
{
	(void)g; (void)cb; (void)arg;

}

void
ast_adj_upln(gelem_t to, gelem_t from, gelem_t w, gelem_t agg)
{
	(void)agg;
	lp_ast_node_t *p = from.ge_p;
	lp_ast_node_t *c = to.ge_p;
	uint32_t index = w.ge_u;

	c->an_parent = p;
	c->an_index = index;
	c->an_left = p->an_last_child;
	c->an_right = NULL;
	p->an_last_child = c;
}

int
ast_bfs_cb(gelem_t agg, gelem_t n, gelem_t *aggp)
{
	return (0);
}

void
lp_ast_update_links(lp_ast_t *ast)
{
	gelem_t ignore;
	gelem_t start;
	start.ge_p = ast->ast_start;
	lg_bfs_fold(ast->ast_graph, start, ast_adj_upln, ast_bfs_cb, ignore);

}


typedef struct xfs_fold_cookie {
	lp_ast_cb_t	*xfs_cb;
	lp_ast_t	*xfs_ast;
	void		*xfs_arg;
} xfs_fold_cookie_t;

int
dfs_cb(gelem_t agg, gelem_t n, gelem_t *aggp)
{
	(void)aggp;
	xfs_fold_cookie_t *c = agg.ge_p;
	lp_ast_node_t *an = n.ge_p;
	int r = c->xfs_cb(an, c->xfs_arg);
	return (r);
}

int
bfs_cb(gelem_t agg, gelem_t n, gelem_t *aggp)
{
	(void)aggp;
	xfs_fold_cookie_t *c = agg.ge_p;
	lp_ast_node_t *an = n.ge_p;
	int r = c->xfs_cb(an, c->xfs_arg);
	return (r);
}

static selem_t
dfs_fold(selem_t arg, selem_t *e, uint64_t sz)
{
	uint64_t i = 0;
	while (i < sz) {
		xfs_fold_cookie_t *c = arg.sle_p;
		gelem_t garg;
		gelem_t n;
		garg.ge_p = c;
		n.ge_p = e[i].sle_p;
		lg_dfs_fold(c->xfs_ast->ast_graph, n, NULL, dfs_cb, garg);
		i++;
	}
	return (arg);
}

static selem_t
bfs_fold(selem_t arg, selem_t *e, uint64_t sz)
{
	uint64_t i = 0;
	while (i < sz) {
		xfs_fold_cookie_t *c = arg.sle_p;
		gelem_t garg;
		gelem_t n;
		garg.ge_p = c;
		n.ge_p = e[i].sle_p;
		lg_bfs_fold(c->xfs_ast->ast_graph, n, NULL, bfs_cb, garg);
		i++;
	}
	return (arg);
}

/*
 * Does DFS starting from each node that has an_gnm `gnm`.
 */
void
lp_ast_dfs_name(lp_ast_t *ast, char *gnm, lp_ast_cb_t cb, void *arg)
{
	lp_ast_node_t *an_min = lp_mk_ast_node();
	lp_ast_node_t *an_max = lp_mk_ast_node();
	an_min->an_gnm = gnm;
	an_max->an_gnm = gnm;
	an_min->an_id = 0;
	an_max->an_id = UINT64_MAX;
	selem_t zero;
	xfs_fold_cookie_t ck;
	ck.xfs_cb = cb;
	ck.xfs_arg = arg;
	ck.xfs_ast = ast;
	zero.sle_p = &ck;

	selem_t smin;
	selem_t smax;
	smin.sle_p = an_min;
	smax.sle_p = an_max;

	(void)slablist_foldr_range(ast->ast_nodes, dfs_fold, smin, smax, zero);
	lp_rm_ast_node(an_min);
	lp_rm_ast_node(an_max);
}

void
lp_ast_bfs_name(lp_ast_t *ast, char *name, lp_ast_cb_t cb, void *arg)
{
	lp_ast_node_t *an_min = lp_mk_ast_node();
	lp_ast_node_t *an_max = lp_mk_ast_node();
	an_min->an_gnm = gnm;
	an_max->an_gnm = gnm;
	an_min->an_id = 0;
	an_max->an_id = UINT64_MAX;
	selem_t zero;
	xfs_fold_cookie_t ck;
	ck.xfs_cb = cb;
	ck.xfs_arg = arg;
	ck.xfs_ast = ast;
	zero.sle_p = &ck;

	selem_t smin;
	selem_t smax;
	smin.sle_p = an_min;
	smax.sle_p = an_max;

	(void)slablist_foldr_range(ast->ast_nodes, bfs_fold, smin, smax, zero);
	lp_rm_ast_node(an_min);
	lp_rm_ast_node(an_max);
}

int
flatten_cb(gelem_t n, gelem_t arg)
{
	lp_flatten_cb_t *cb = arg.ge_p;
	lp_ast_node_t *an = n.ge_p;
	int ret = cb(an);
	return (ret);
}

static selem_t
flatten_fold(selem_t agg, selem_t *e, size_t sz)
{
	uint64_t i = 0;
	while (i < sz) {
		gelem_t node;
		node.ge_p = e[i].sle_p;
		lp_ast_node_t *an = node.ge_p;
		lp_ast_t *ast = an->an_ast;
		gelem_t arg;
		arg.ge_p = agg.sle_p;
		lg_flatten(ast->ast_graph, node, flatten_cb, arg);
		i++;
	}
	return (agg);
}

void
lp_flatten_astn(lp_ast_t *ast, char *gnm, lp_flatten_cb_t *cb)
{
	lp_ast_node_t *an_min = lp_mk_ast_node();
	lp_ast_node_t *an_max = lp_mk_ast_node();
	an_min->an_gnm = gnm;
	an_max->an_gnm = gnm;
	an_min->an_id = 0;
	an_max->an_id = UINT64_MAX;
	selem_t zero;
	zero.sle_p = cb;

	selem_t smin;
	selem_t smax;
	smin.sle_p = an_min;
	smax.sle_p = an_max;

	(void)slablist_foldr_range(ast->ast_nodes, flatten_fold, smin, smax, zero);
	lp_rm_ast_node(an_min);
	lp_rm_ast_node(an_max);
}


/*
 * Compare the segments of 2 grammar nodes (from 2 diff grammars) for
 * superficial equivalence.
 *
 * The gnodes are superficially equivalent if they have:
 *
 * 	same number of segments
 * 	same number of elements in each segment[1]
 * 	same sequence of segments
 * 	same ROP_* in each segment
 *
 * [1]: If 2 segments are identical in every way, but 1 is a range, and 1 is an
 * array, we compare the size of the range to the number of elements.
 *
 * It is possible for 2 PARSER gnodes to be superficially equivelent, and yet
 * not consume the same input. Eventually, once all of the low hanging fruit
 * have been picked, we may need to enhance this function to determine if 2
 * gnodes are 'deeply' equivalent.
 */
int
seg_cmp(lp_grmr_t *gr1, lp_grmr_t *gr2, lp_grmr_node_t *g1, lp_grmr_node_t *g2)
{
	lp_tok_t *t1 = find_token(gr1, g1->gn_tok);
	lp_tok_t *t2 = find_token(gr2, g2->gn_tok);
	uint64_t t1ns = slablist_get_elems(t1->tok_segs);
	uint64_t t2ns = slablist_get_elems(t2->tok_segs);
	if (t1ns != t2ns) {
		return (1);
	}
	/*
	 * We want to zip over the 2 slablists, using slablist_next().
	 */
	uint64_t i = 0;
	slablist_bm_t *bm1 = slablist_bm_create(t1->tok_segs);
	slablist_bm_t *bm2 = slablist_bm_create(t2->tok_segs);
	while (i < t1ns) {
		selem_t e1;
		selem_t e2;
		slablist_next(t1->tok_segs, bm1, &e1);
		slablist_next(t2->tok_segs, bm2, &e2);
		tok_seg_t *s1 = e1.sle_p;
		tok_seg_t *s2 = e2.sle_p;
		if (s1->ts_op != s2->ts_op || s1->ts_width != s2->ts_width ||
		    (s1->ts_elems != s2->ts_elems && ((s1->ts_data != NULL &&
		    s2->ts_range_min != NULL) || (s1->ts_range_min != NULL &&
		    s2->ts_data != NULL)))) {
			slablist_bm_destroy(bm1);
			slablist_bm_destroy(bm2);
			return (1);
		} else if (s1->ts_elems != s2->ts_elems) {
			tok_seg_t *rs;
			tok_seg_t *ds;
			if (s1->ts_range_min == NULL &&
			    s2->ts_range_min == NULL) {
				return (1);
			}
			if (s1->ts_range_min) {
				rs = s1;
				ds = s2;
			} else if (s2->ts_range_min) {
				rs = s2;
				ds = s1;
			}
			size_t elems = range_sz_discont(rs->ts_range_min,
			    rs->ts_range_max, rs->ts_width);
			if (elems != ds->ts_elems) {
				slablist_bm_destroy(bm1);
				slablist_bm_destroy(bm2);
				return (1);
			}
		}
		i++;
	}
	slablist_bm_destroy(bm1);
	slablist_bm_destroy(bm2);
	return (0);
}

void
grmr_cmp_acb(gelem_t to, gelem_t from, gelem_t w, gelem_t cookie)
{
	lp_grmr_node_t *fn = from.ge_p;
	lp_grmr_node_t *tn = to.ge_p;
	cmp_cookie_t *ck = cookie.ge_p;
	lp_grmr_t *g1 = ck->cmpck_g1;
	lp_grmr_t *g2 = ck->cmpck_g2;

	cmp_walk_step_t *c = NULL;
	selem_t sc;
	if (ck->cmpck_graph < 1) {
		cmp_walk_step_t *c = lp_mk_cmp_walk_step();
		c->cws_from = fn->gn_name;
		c->cws_to = tn->gn_name;
		c->cws_w = w.ge_u;
		sc.sle_p = c;
		slablist_add(ck->cmpck_walk, sc, 0);
	} else {
		selem_t e;
		selem_t *ep = &e;
		slablist_next(ck->cmpck_walk, ck->cmpck_bm, ep);
		c = e.sle_p;
		if (strcmp(c->cws_from, fn->gn_name) ||
		    strcmp(c->cws_to, tn->gn_name) || c->cws_w != w.ge_u) {
			ck->cmpck_walk_mismatch = 1;
		} else {
			if (ck->cmpck_type1 != NULL) {
				return;
			}
			lp_grmr_node_t *gf =
			    find_grmr_node(g1, c->cws_from);
			lp_grmr_node_t *gt =
			    find_grmr_node(g1, c->cws_to);
			if (gf->gn_type != fn->gn_type) {
				ck->cmpck_type1 = gf;
				ck->cmpck_type2 = fn;
				ck->cmpck_type_mismatch = 1;
				return;
			} else if (gt->gn_type != tn->gn_type) {
				ck->cmpck_type1 = gt;
				ck->cmpck_type2 = tn;
				ck->cmpck_type_mismatch = 1;
				return;
			}
			if (ck->cmpck_seg1 != NULL) {
				return;
			}
			if (gf->gn_type == PARSER) {
				int seg_diff = seg_cmp(g1, g2, gf, fn);
				if (seg_diff) {
					ck->cmpck_seg_mismatch = 1;
					ck->cmpck_seg1 = gf;
					ck->cmpck_seg2 = fn;
					return;
				}
			}
			if (gt->gn_type == PARSER) {
				int seg_diff = seg_cmp(g1, g2, gt, tn);
				if (seg_diff) {
					ck->cmpck_seg_mismatch = 1;
					ck->cmpck_seg1 = gt;
					ck->cmpck_seg2 = tn;
					return;
				}
			}
		}
	}
}

void
astn_cmp_acb(gelem_t to, gelem_t from, gelem_t w, gelem_t cookie)
{
	lp_ast_node_t *fn = from.ge_p;
	lp_ast_node_t *tn = to.ge_p;
	cmp_cookie_t *ck = cookie.ge_p;

	cmp_walk_step_t *c = NULL;
	selem_t sc;
	if (ck->cmpck_graph < 1) {
		cmp_walk_step_t *c = lp_mk_cmp_walk_step();
		c->cws_from = fn->an_gnm;
		c->cws_to = tn->an_gnm;
		c->cws_w = w.ge_u;
		sc.sle_p = c;
		slablist_add(ck->cmpck_walk, sc, 0);
	} else {
		selem_t e;
		selem_t *ep = &e;
		slablist_next(ck->cmpck_walk, ck->cmpck_bm, ep);
		c = e.sle_p;
		if (strcmp(c->cws_from, fn->an_gnm) ||
		    strcmp(c->cws_to, tn->an_gnm) || c->cws_w != w.ge_u) {
			ck->cmpck_walk_mismatch = 1;
		}
	}
}

int
grmr_cmp_cb(gelem_t agg, gelem_t node, gelem_t *aggp)
{
	cmp_cookie_t *ck = agg.ge_p;
	if (ck->cmpck_walk_mismatch || ck->cmpck_type_mismatch ||
	    ck->cmpck_seg_mismatch) {
		return (1);
	}
	return (0);
}

int
astn_cmp_cb(gelem_t agg, gelem_t node, gelem_t *aggp)
{
	cmp_cookie_t *ck = agg.ge_p;
	if (ck->cmpck_walk_mismatch) {
		return (1);
	}
	return (0);
}

void
clean_cws(selem_t e)
{
	cmp_walk_step_t *s = e.sle_p;
	lp_rm_cmp_walk_step(s);
}

int
lp_cmp_grmr(lp_grmr_t *g1, lp_grmr_t *g2, char **gnp1, char **gnp2)
{
	lg_graph_t *gr1 = g1->grmr_graph;
	lg_graph_t *gr2 = g2->grmr_graph;
	uint64_t e1 = lg_nedges(gr1);
	uint64_t e2 = lg_nedges(gr2);
	if (e1 != e2) {
		return (1);
	}

	uint64_t gn1 = slablist_get_elems(g1->grmr_gnodes);
	uint64_t gn2 = slablist_get_elems(g2->grmr_gnodes);
	if (gn1 != gn2) {
		return (2);
	}
	/*
	 * We do BFS on gr1, and save the parent-child-weight tuples into a
	 * list. We then do a BFS on gr2, and pass the list as an argument.
	 * That cb for that BFS will expect to run into all the same tuples in
	 * the same order.
	 *
	 * We need a cookie for each BFS. For BFS 1 it needs to have a
	 * container that it can append parent-child-weight tuples to.
	 *
	 * For BFS2 it needs the same container, and a cursor.
	 */
	gelem_t root_gr1;
	gelem_t root_gr2;
	root_gr1.ge_p = g1->grmr_root;
	root_gr2.ge_p = g2->grmr_root;

	cmp_cookie_t ck;
	bzero(&ck, sizeof (cmp_cookie_t));
	ck.cmpck_g1 = g1;
	ck.cmpck_g2 = g2;
	ck.cmpck_walk = slablist_create("cmpck_walk", NULL, NULL, SL_ORDERED);
	gelem_t cookie;
	cookie.ge_p = &ck;

	lg_bfs_fold(gr1, root_gr1, grmr_cmp_acb, grmr_cmp_cb, cookie);

	ck.cmpck_bm = slablist_bm_create();
	ck.cmpck_graph = 1;
	lg_bfs_fold(gr2, root_gr2, grmr_cmp_acb, grmr_cmp_cb, cookie);
	slablist_destroy(ck.cmpck_walk, clean_cws);
	if (ck.cmpck_walk_mismatch) {
		return (3);
	}
	if (ck.cmpck_type_mismatch) {
		if (ck.cmpck_type1 == NULL || ck.cmpck_type2 == NULL) {
			printf("cmpck_type NULL\n");
			printf("type1 =%p\n", ck.cmpck_type1);
			printf("type2 =%p\n", ck.cmpck_type2);
			abort();
		}
		*gnp1 = ck.cmpck_type1->gn_name;
		*gnp2 = ck.cmpck_type2->gn_name;
		return (4);
	}
	if (ck.cmpck_seg_mismatch) {
		if (ck.cmpck_seg1 == NULL || ck.cmpck_seg2 == NULL) {
			printf("cmpck_seg NULL\n");
			abort();
		}
		*gnp1 = ck.cmpck_seg1->gn_name;
		*gnp2 = ck.cmpck_seg2->gn_name;
		return (5);
	}
	return (0);
}

/*
 * This function is identical to the above except we operate on ASTs not
 * grammars.
 */
int
lp_cmp_ast(lp_ast_t *a1, lp_ast_t *a2)
{
	lg_graph_t *gr1 = a1->ast_graph;
	lg_graph_t *gr2 = a2->ast_graph;
	uint64_t e1 = lg_nedges(gr1);
	uint64_t e2 = lg_nedges(gr2);
	if (e1 != e2) {
		return (1);
	}

	uint64_t gn1 = slablist_get_elems(a1->ast_nodes);
	uint64_t gn2 = slablist_get_elems(a2->ast_nodes);
	if (gn1 != gn2) {
		return (2);
	}
	gelem_t root_gr1;
	gelem_t root_gr2;
	root_gr1.ge_p = a1->ast_start;
	root_gr2.ge_p = a2->ast_start;

	cmp_cookie_t ck;
	ck.cmpck_graph = 0;
	ck.cmpck_walk_mismatch = 0;
	ck.cmpck_walk = slablist_create("cmpck_walk", NULL, NULL, SL_ORDERED);
	gelem_t cookie;
	cookie.ge_p = &ck;

	lg_bfs_fold(gr1, root_gr1, astn_cmp_acb, astn_cmp_cb, cookie);

	ck.cmpck_bm = slablist_bm_create();
	ck.cmpck_graph = 1;
	lg_bfs_fold(gr2, root_gr2, astn_cmp_acb, astn_cmp_cb, cookie);
	slablist_destroy(ck.cmpck_walk, clean_cws);
	if (ck.cmpck_walk_mismatch) {
		return (3);
	}
	return (0);
}

lp_ast_node_t *
lp_ast_node_next(lp_ast_node_t *n)
{
	return (n->an_right);
}

lp_ast_node_t *
lp_ast_node_prev(lp_ast_node_t *n)
{
	return (n->an_left);
}

lp_ast_node_t *
lp_ast_node_parent(lp_ast_node_t *n)
{
	return (n->an_parent);
}

lp_ast_node_t *
lp_ast_node_last_child(lp_ast_node_t *n)
{
	return (n->an_last_child);
}

int
reaction_cmp(selem_t e1, selem_t e2)
{
	reaction_t *w1 = e1.sle_p;
	reaction_t *w2 = e2.sle_p;
	int c = strcmp(w1->rtn_node_name, w2->rtn_node_name);
	return (c);
}

int
reaction_bnd(selem_t e, selem_t min, selem_t max)
{
	int c = reaction_cmp(e, min);
	if (c < 0) {
		return (c);
	}
	c = reaction_cmp(e, max);
	if (c > 0) {
		return (c);
	}
	return (0);
}

lp_reactor_t *
lp_create_reactor(lp_ast_t *ast)
{
	lp_reactor_t *rctr = calloc(1, sizeof (lp_reactor_t));
	rctr->rct_ast = ast;
	rctr->rct_name_cb = slablist_create("rct_name_cb", reaction_cmp,
	    reaction_bnd, SL_SORTED);
	return (rctr);
}



int
lp_ast_on(lp_reactor_t *rctr, char *nm, lp_ast_rct_cb_t *cb)
{
	reaction_t *w = calloc(1, sizeof (reaction_t));
	w->rtn_node_name = nm;
	w->rtn_cb = cb;
	selem_t sw;
	sw.sle_p = w;
	int r = slablist_add(rctr->rct_name_cb, sw, 0);
	if (r != SL_SUCCESS) {
		free(w);
	}
	return (r);
}

lp_grmr_t *
lp_ast_grmr(lp_ast_t *ast)
{
	return (ast->ast_grmr);
}

int
rctr_cb(lp_ast_node_t *n, void *r)
{
	lp_reactor_t *rctr = r;
	reaction_t rn;
	rn.rtn_node_name = n->an_gnm;
	selem_t srn;
	srn.sle_p = &rn;
	selem_t found;
	int rf = slablist_find(rctr->rct_name_cb, srn, &found);
	if (rf != SL_SUCCESS) {
		return (0);
	}
	reaction_t *f = found.sle_p;
	f->rtn_cb(rctr, rctr->rct_ast, n, f->rtn_touched);
	f->rtn_touched++;
	return (0);
}

selem_t
reset_touched(selem_t ignored, selem_t *e, uint64_t sz)
{
	uint64_t i = 0;
	while (i < sz) {
		reaction_t *r = e[i].sle_p;
		r->rtn_touched = 0;
		i++;
	}
	return (ignored);
}

void
lp_ast_react_dfs_name(lp_reactor_t *rctr, char *nm)
{
	slablist_foldr(rctr->rct_name_cb, reset_touched, ignored);
	lp_ast_dfs_name(rctr->rct_ast, nm, rctr_cb, (void *)rctr);
	slablist_foldr(rctr->rct_name_cb, reset_touched, ignored);
}

void
lp_reactor_push(lp_reactor_t *rctr, void *data)
{
	if (rctr->rct_stack == NULL) {
		rctr->rct_stack = slablist_create("rctr_stk", NULL, NULL,
		    SL_ORDERED);
	}
	selem_t e;
	e.sle_p = data;
	slablist_add(rctr->rct_stack, e, 0);
}

void *
lp_reactor_popr(lp_reactor_t *rctr)
{
	uint64_t elems = slablist_get_elems(rctr->rct_stack);
	if (elems) {
		selem_t r = slablist_end(rctr->rct_stack);
		slablist_rem(rctr->rct_stack, ignored, elems - 1, NULL);
		return ((void *)r.sle_p);
	}
	return (NULL);
}

void *
lp_reactor_popl(lp_reactor_t *rctr)
{
	uint64_t elems = slablist_get_elems(rctr->rct_stack);
	if (elems) {
		selem_t r = slablist_head(rctr->rct_stack);
		slablist_rem(rctr->rct_stack, ignored, 0, NULL);
		return ((void *)r.sle_p);
	}
	return (NULL);
}

/* Returns first elem and sets bookmark to point to it */
void *
lp_reactor_first(lp_reactor_t *rctr)
{
	if (rctr->rct_stack != NULL && rctr->rct_bm != NULL) {
		slablist_bm_destroy(rctr->rct_bm);
	}
	rctr->rct_bm = slablist_bm_create(rctr->rct_stack);
	selem_t r;
	slablist_next(rctr->rct_stack, rctr->rct_bm, &r);
	return ((void *)r.sle_p);
}

/* Same as above but for last elem */
void *
lp_reactor_last(lp_reactor_t *rctr)
{
	if (rctr->rct_stack != NULL && rctr->rct_bm != NULL) {
		slablist_bm_destroy(rctr->rct_bm);
	}
	rctr->rct_bm = slablist_bm_create(rctr->rct_stack);
	selem_t r;
	slablist_prev(rctr->rct_stack, rctr->rct_bm, &r);
	return ((void *)r.sle_p);
}

/* Moves bookmark to right -- if bmark is empty, this func behaves like _first above */
void *
lp_reactor_next(lp_reactor_t *rctr)
{
	if (rctr->rct_stack == NULL) {
		return (lp_reactor_first(rctr));
	}
	selem_t r;
	slablist_next(rctr->rct_stack, rctr->rct_bm, &r);
	return ((void *)r.sle_p);
}

/* Move mark to the left --  if bmark is empty, this func behaves like _first above */
void *
lp_reactor_prev(lp_reactor_t *rctr)
{
	if (rctr->rct_stack == NULL) {
		return (lp_reactor_last(rctr));
	}
	selem_t r;
	slablist_prev(rctr->rct_stack, rctr->rct_bm, &r);
	return ((void *)r.sle_p);
}

typedef struct clone_args {
	lp_ast_t	*ca_ast;
	lg_graph_t	*ca_clone_set;
} clone_args_t;

void
get_clone_cb(gelem_t from, gelem_t to, gelem_t weight, gelem_t arg)
{
	lp_ast_node_t **c = arg.ge_p;
	*c = to.ge_p;
}

lp_ast_node_t *
get_clone(lg_graph_t *clones, lp_ast_node_t *n)
{
	gelem_t gn;
	gn.ge_p = n;
	lp_ast_node_t *clone = NULL;
	gelem_t arg;
	arg.ge_p = &clone;
	lg_neighbors_arg(clones, gn, get_clone_cb, arg);
	if (clone == NULL) {
		clone = lp_mk_ast_node();
		gelem_t gclone;
		gclone.ge_p = clone;
		lg_connect(clones, gn, gclone);
	}
	return (clone);
}

void
clone_n_link(gelem_t from, gelem_t to, gelem_t weight, gelem_t arg)
{
	clone_args_t *ca = arg.ge_p;
	lp_ast_t *ast = ca->ca_ast;
	lg_graph_t *clones = ca->ca_clone_set;

	lp_ast_node_t *f = from.ge_p;
	lp_ast_node_t *t = to.ge_p;
	uint64_t w = weight.ge_u;
	lp_ast_t *orig = f->an_ast;

	lp_ast_node_t *ff = get_clone(clones, f);
	lp_ast_node_t *tt = get_clone(clones, t);
	ff->an_id = (uint64_t)ff;
	tt->an_id = (uint64_t)tt;
	ff->an_ast = ast;
	tt->an_ast = ast;
	ff->an_gnm = f->an_gnm;
	tt->an_gnm = t->an_gnm;
	ff->an_type = f->an_type;
	tt->an_type = t->an_type;
	ff->an_off_start = f->an_off_start;
	tt->an_off_start = t->an_off_start;
	ff->an_off_end = f->an_off_end;
	tt->an_off_end = t->an_off_end;
	add_ast_node(ast, tt);
	add_ast_node(ast, ff);
	lp_add_ast_child(ff, tt);
	if (f == orig->ast_start) {
		ast->ast_start = ff;
	}
}

lp_ast_t *
lp_clone_ast(lp_ast_t *ast)
{
	clone_args_t ca;
	ca.ca_ast = lp_create_ast();
	ca.ca_ast->ast_cloning = 1;
	ca.ca_clone_set = lg_create_digraph();
	ca.ca_ast->ast_in = ast->ast_in;
	ca.ca_ast->ast_sz = ast->ast_sz;
	ca.ca_ast->ast_grmr = ast->ast_grmr;
	gelem_t gclone;
	gclone.ge_p = &ca;
	lg_edges_arg(ast->ast_graph, clone_n_link, gclone);
	ca.ca_ast->ast_cloning = 0;
	lg_destroy_graph(ca.ca_clone_set);
	return (ca.ca_ast);
}
