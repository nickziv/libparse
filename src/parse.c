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
static char *lp_nm_meta = "libpazu_meta_repeater";


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
	do {
		get_bits(in, buf, off, off + w);
		c = bcmp(buf, s->ts_data, w);
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
	char buf[32];
	bzero(buf, 32);
	get_bits(in, buf, bit_off, bit_off + w);
	int c = bcmp(buf, s->ts_data, w);
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

lp_grmr_node_t *find_grmr_node(lp_grmr_t *g, char *name);

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
add_grouper(lp_grmr_t *g, lp_grmr_node_t *grmr_node)
{
	selem_t ie;
	ie.sle_p = grmr_node;
	slablist_add(g->grmr_groupers, ie, 0);
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
	lp_grmr_node_t ir;
	ir.gn_name = name;
	selem_t find;
	find.sle_p = &ir;
	selem_t ret;
	int f = slablist_find(g->grmr_gnodes, find, &ret);
	if (f != SL_SUCCESS) {
		return (NULL);
	} else {
		return (ret.sle_p);
	}
}

lp_grmr_node_t *
find_grouper(lp_grmr_t *g, char *nm)
{
	lp_grmr_node_t ir;
	ir.gn_name = nm;
	selem_t find;
	find.sle_p = &ir;
	selem_t ret;
	int f = slablist_find(g->grmr_groupers, find, &ret);
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
	 * Directly nesting splitters results in more edges, and slower
	 * execution time. Better to force the user to collapse or inline
	 * directly nested splitters.
	 */
	if (p->gn_type == SPLITTER && c->gn_type == SPLITTER) {
		return (-3);
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
 * This function defines a grouping. A grouping is a logical group of
 * ast_nodes. For example by defining a grouping as "(" and ")", every ast-node
 * that's between those two tokens has their parent node replaced by a new
 * parent node that represents the group. The `scope` argument indicates what
 * the parent of the grouper _must_ be. This makes it impossible to create a
 * nonsensical groups -- like an if-statement that encloses an entire C code
 * file (only function definitions are allowed at the root of the file).
 *
 * This allows us to break expressions down into parenthesized subexpressions,
 * for example. This also allows us to implemented nesting such as in the case
 * of nested loops. It allows to structure the AST in a way that makes sense,
 * without recursively defining the grammar graph.
 */
int
lp_add_grouping(lp_grmr_t *g, char *start, char *end, char *scope)
{
	
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
	} else if (an->an_parent != NULL) {
		an->an_off_start = an->an_parent->an_off_start;
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
		}
		i++;
	}
	an->an_off_end = an->an_off_start + (uint32_t)total_matched;
	PARSE_AST_NODE_OFF_END(ast->ast_grmr, an);
	if (an->an_state == ANS_MATCH) {
		ast->ast_matched += total_matched;
	} else {
		ast->ast_matched = 0;
	}
	return (z);
}

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
		c->an_right->an_left = c->an_left;
		if (c->an_left != NULL) {
			c->an_left->an_right = c->an_right;
		}
	}
	/*
	 * We unlink the child from the meta_repeater.
	 */
	if (p->an_type == REPEATER) {
		gelem_t meta_weight;
		gelem_t meta;
		meta.ge_p = p->an_meta;
		meta_weight.ge_u = weight.ge_u;
		lg_wdisconnect(ast_graph, meta, gc, meta_weight);
		PARSE_AST_REM_CHILD(grmr, p->an_meta, c);
	}
	p->an_kids--;
	PARSE_AST_REM_CHILD(grmr, p, c);
	if (PARSE_TRACE_AST_ENABLED()) {
		lg_edges(p->an_ast->ast_graph, trace_ast);
	}
}

int
meta_rem_sub(gelem_t whatever, gelem_t ast_node, gelem_t *ignored)
{
	(void)whatever;
	(void)ignored;
	lp_ast_node_t *n = ast_node.ge_p;
	lp_ast_t *a = n->an_ast;
	lp_ast_node_t *p = n->an_parent;
	if (p != NULL) {
		gelem_t gpm;
		gelem_t gp;
		gelem_t gn;

		gp.ge_p = p;
		gpm.ge_p = p->an_meta;
		gn.ge_p = n;

		/*
		 * If this an edge with a meta_repeater source, we want to
		 * queue it for removal.
		 */
		if (gpm.ge_p != NULL) {
			qed_edge_t *qe = lp_mk_qed_edge();
			qe->qed_from = gpm;
			qe->qed_to = gn;
			qe->qed_weight.ge_u = n->an_index;

			selem_t sqe;
			sqe.sle_p = qe;
			slablist_add(a->ast_rem_q, sqe, 0);
		}
		/*
		 * And now we remove the edge between parent and child.
		 */
		qed_edge_t *qe2 = lp_mk_qed_edge();
		qe2->qed_from = gp;
		qe2->qed_to = gn;
		qe2->qed_weight.ge_u = n->an_index;

		selem_t sqe2;
		sqe2.sle_p = qe2;
		slablist_add(a->ast_rem_q, sqe2, 0);

	}
	if (PARSE_TRACE_AST_ENABLED()) {
		lg_edges(a->ast_graph, trace_ast);
	}
	return (0);
}

void
meta_rem_con(gelem_t from, gelem_t to, gelem_t weight)
{
	/*
	 * We want to queue up all of the edges that have the meta_repeater as
	 * the source, for removal.
	 */
	lp_ast_node_t *mr = from.ge_p;
	lp_ast_t *a = mr->an_ast;
	qed_edge_t *q = lp_mk_qed_edge();
	q->qed_from = from;
	q->qed_to = to;
	q->qed_weight = weight;
	selem_t sq;
	sq.sle_p = q;
	slablist_add(a->ast_rem_q, sq, 0);
}

selem_t
rem_qed(selem_t zero, selem_t *e, uint64_t sz)
{
	lp_ast_t *a = zero.sle_p;
	uint64_t i = 0;
	/*
	 * XXX We may have to deallocate the ast_nodes.
	 */
	while (i < sz) {
		qed_edge_t *q = e[i].sle_p;
		/*
		 * If `p` is not the parent of `c`, then it must be the
		 * meta_repeater. So we manually disconnect.
		 */
		lg_wdisconnect(a->ast_graph, q->qed_from, q->qed_to,
		    q->qed_weight);
		//lp_rm_ast_node(q->qed_to.ge_p);
		lp_rm_qed_edge(q);
		i++;
	}
	return (zero);
}

/*
 * This function removes all elements in the AST's rem queue.
 */
void
clear_rem_q(slablist_t *s)
{
	uint64_t e = slablist_get_elems(s);
	while (e > 0) {
		slablist_rem(s, ignored, 0, NULL);
		e--;
	}
}

/*
 * Given a repeater that is about to be repeated, this function removes the the
 * links from the meta_repeater to the repeater's children from the most recent
 * iteration. In this way, it effectively 'resets' the meta_repeater.
 */
void
reset_meta_repeater(lp_ast_node_t *r)
{
	gelem_t meta;
	lp_ast_t *ast = r->an_ast;
	ast->ast_meta_targ = r->an_meta;
	meta.ge_p = r->an_meta;
	lg_graph_t *g = r->an_ast->ast_graph;
	if (r->an_ast->ast_rem_q == NULL) {
		r->an_ast->ast_rem_q = slablist_create("ast_rem_q", NULL, NULL,
		    SL_ORDERED);
	}
	/*
	 * We queue the edges that link the meta_repeater to its kids. We then
	 * disconnect the meta_repeater from its children. We queue the edges
	 * as triples, which have to be allocated and deallocated.
	 */
	lg_neighbors(g, meta, meta_rem_con);
	selem_t zero;
	zero.sle_p = r->an_ast;
	slablist_foldr(r->an_ast->ast_rem_q, rem_qed, zero);
	clear_rem_q(r->an_ast->ast_rem_q);
}

/*
 * Given a repeater that has state MATCH_PART, this function removes the
 * subtree of the last iteration, which should have failed by now.
 */
void
remove_partial_subtree(lp_ast_node_t *r)
{
	gelem_t meta;
	gelem_t z;
	lp_ast_t *ast = r->an_ast;
	ast->ast_meta_targ = r->an_meta;
	meta.ge_p = r->an_meta;
	r->an_meta = NULL;
	z.ge_p = meta.ge_p;
	lg_graph_t *g = r->an_ast->ast_graph;
	/* queue the whole subtree */
	lg_bfs_fold(g, meta, NULL, meta_rem_sub, z);
	/* remove the whole subtree */
	selem_t zero;
	zero.sle_p = ast;
	slablist_foldr(r->an_ast->ast_rem_q, rem_qed, zero);
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
			if (p->an_type == REPEATER) {
				if (p->an_state == ANS_MATCH) {
					p->an_state = ANS_MATCH_PART;
					if (p->an_meta != NULL) {
						remove_partial_subtree(p);
					}
					PARSE_MATCH_PART(p);
				} else {
					p->an_state = ANS_FAIL;
					PARSE_FAIL(p);
				}
			} else if (p->an_type == SEQUENCER) {
				p->an_state = ANS_FAIL;
				PARSE_FAIL(p);
			}
			
		}
		lp_rm_ast_node(l);
	}
	if (e > 0) {
		slablist_rem(ast->ast_stack, ignored, e - 1, NULL);
	}
}

/*
 * Forward declaration of on_push.
 */
int on_push(gelem_t, gelem_t, gelem_t *);

void ast_push_astn(lp_ast_t *ast, lp_ast_node_t *an);

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
}

int on_pop(gelem_t gn, gelem_t state);

/*
 * Handles REPEATERS.
 */
int
on_pop_handle_repeater(lp_ast_t *ast, lp_ast_node_t *a_top, gelem_t gn,
    gelem_t state)
{
	/*
	 * XXX once we get to the last if-clause, we want to destroy the failed
	 * child, mark the repeater as done, possibly get out of the 2nd level
	 * of nesting, and go to the repeater's parent.
	 */
	if (a_top->an_state == ANS_TRY) {
		a_top->an_state = ANS_MATCH;
		PARSE_MATCH(a_top);
	}

	/*
	 * We don't want to change the offset, since we're going to throw away
	 * the children that resulted from the partial match.
	 */
	if (a_top->an_state != ANS_MATCH_PART) {
		a_top->an_off_end = a_top->an_last_child->an_off_end;
		PARSE_AST_NODE_OFF_END(ast->ast_grmr, a_top);
	}

	if (a_top->an_state == ANS_MATCH && ast->ast_dfs_nesting < 1) {
		PARSE_REPEAT(a_top);
		if (a_top->an_meta != NULL) {
			reset_meta_repeater(a_top);
		}
		ast->ast_dfs_nesting = 1;
		PARSE_NESTING(ast);
		lg_dfs_rdnt_fold(ast->ast_grmr->grmr_graph, gn,
		    on_pop, on_push, state);
		ast->ast_dfs_nesting = 0;
		/*
		 * At this point we've finished with our sub-dfs.  Because the
		 * on_push and on_pop functions maintain a stack of
		 * ast_node_t's that needs to be implicitly kept in sync with
		 * the stack maintained by libgraph's dfs() routine, we end up
		 * popping `a_top` from the stack. From the perspective of the
		 * sub_dfs this is perfectly acceptable behaviour since, the
		 * corresponding grmr_node_t is being popped in libgraph.
		 * However, the grmr_node at the bottom of the sub_dfs's stack
		 * is the same node that's at the top of _this_ dfs's stack. As
		 * a result, we have to push the node that was inadvertently
		 * popped back onto the AST stack.
		 */
		selem_t s_re_push = slablist_end(ast->ast_stack);
		lp_ast_node_t *re_push = s_re_push.sle_p;
		if (re_push->an_gnm != a_top->an_gnm) {
			ast_push_astn(ast, re_push->an_last_child);
		}
		/*
		 * Now that we've finished with the sub-dfs we try to
		 * relaunch it, in case the parse repeats again. We
		 * don't have to update anything. All of the nodes
		 * generated by the repeater are its children, thus
		 * a_top stays the same. 
		 */
		PARSE_REPEAT_RETRY(a_top);
		//goto retry_sub_dfs;
		return (1);
	} else if (a_top->an_state == ANS_MATCH &&
	    ast->ast_dfs_nesting == 1) {
		ast->ast_dfs_unwind = 1;
		PARSE_NESTING(ast);
		return (-1);
	}
	return (0);
}

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
	int rep = 1;


	switch (a_top->an_type) {

	case SPLITTER:
		if (a_top->an_state == ANS_MATCH) {
			ast_pop_astn(ast);
			return (0);
		} else {
			a_top->an_state = ANS_FAIL;
			PARSE_FAIL(a_top);
			ast_pop_astn(ast);
			return (0);
		}
		break;

	case PARSER:
		try_parse(node, ast);
		break;

	case SEQUENCER:
		if (a_top->an_state == ANS_TRY) {
			on_pop_handle_sequencer(ast, a_top);
		}
		break;

	case REPEATER:
		while (rep > 0) {
			rep = on_pop_handle_repeater(ast, a_top, gn, state);
		}
		/* Bail early */
		if (rep < 0) {
			return (ret);
		}
		break;

	case GROUPER:
		break;
	}

	/*
	 * If we've finished the child of a splitter, we want to pop twice, so
	 * that we end up at the splitter's parent. To do this, we set `ret` to
	 * 1.
	 *
	 * Also we explicitly forbid the nesting of splitters, so we don't have
	 * to loop up the parent chain, to fill in any offsets.
	 */
	lp_ast_node_t *p = a_top->an_parent;
	if (a_top->an_state == ANS_MATCH && p != NULL &&
	    p->an_type == SPLITTER) {
		p->an_off_end = a_top->an_off_end;
		p->an_state = ANS_MATCH;
		PARSE_AST_NODE_OFF_END(ast->ast_grmr, p);
		PARSE_MATCH(p);
		ret = 1;
	}

	ast_pop_astn(ast);
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
	slablist_add(ast->ast_stack, p, 0);
	/*
	 * If we are pushing a repeater, we want to create a meta-repeater AST
	 * node. This node is not reachable from the root of the AST graph. It
	 * has edges leading to the children of the current iteration of the
	 * repeater. This means that when we fail to match the last iteration
	 * of the repeater, we can free and unlink all of the failed and
	 * matched children of that iteration, by calling DFS on the
	 * meta-repeater. The meta-repeater is completely empty.
	 * XXX we can probably create a meta_repeater_t struct that doesn't
	 * contain anything besides the name.
	 */
	if (an->an_type == REPEATER) {
		lp_ast_node_t *mr = lp_mk_ast_node();
		mr->an_gnm = lp_nm_meta;
		mr->an_ast = an->an_ast;
		an->an_meta = mr;
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
	 * (i.e. 1 of its kids were matched) or if the parent and the child
	 * have the same name (which is possible when we run into a repeater --
	 * we end up pushing the repeater back onto the DFS stack in libgraph,
	 * but we already have a corresponding AST node. So this function
	 * returns NULL in order to turn on_push into a no-op for the repeater
	 * in question. And then business resumes as usual.
	 */
	if (p != NULL && ((p->an_type == SPLITTER &&
	    p->an_state == ANS_MATCH) || p->an_gnm == node->gn_name)) {
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
	/*
	 * So, we handle splitters differently from every other type of ast
	 * node. A splitter can only ever have 1 child. 
	 */
	if (p->an_type != SPLITTER) {
		weight.ge_u = p->an_kids;
		c->an_index = p->an_kids;
		p->an_kids++;
		c->an_left = p->an_last_child;
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
	/*
	 * Over here we connect the repeater's children to its associated
	 * meta-repeater.
	 */
	if (p->an_type == REPEATER) {
		gelem_t meta;
		gelem_t meta_weight;
		meta_weight.ge_u = weight.ge_u;
		meta.ge_p = p->an_meta;
		lg_wconnect(ast_graph, meta, next, meta_weight);
		PARSE_AST_ADD_CHILD(grmr, p->an_meta, c);
	}
	PARSE_AST_ADD_CHILD(grmr, p, c);
	if (PARSE_TRACE_AST_ENABLED()) {
		lg_edges(p->an_ast->ast_graph, trace_ast);
	}
}

/*
 * XXX we don't use `ast` at all in this function...
 */
void
lp_set_ast_offsets(lp_ast_t *ast, lp_ast_node_t *n)
{
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
PARSE_GOT_HERE(1);
		n->an_off_start = n->an_left->an_off_end;
	} else {
PARSE_GOT_HERE(2);
		n->an_off_start = 0;
	}
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
 * we continue the DFS. If the node is a REPEATER, we push its children
 * one-by-one, parsing as we go. When we double-pop a REPEATER, we try another
 * DFS with that REPEATER as the `start` node.
 */
int
on_push(gelem_t state, gelem_t gn, gelem_t *ignored)
{
	(void)ignored;
	lp_ast_t *ast = state.ge_p;
	lp_grmr_node_t *node = gn.ge_p;
	lp_ast_node_t *an = NULL;
	lp_ast_node_t *last_astn = NULL;

	/*
	 * We just tried to call DFS again, in on_pop. However, we don't want
	 * more than 1 level of DFS nesting. So, we return 1, from this
	 * function, forcing the sub-DFS that we are in to end. Then the
	 * top-level DFS in on_pop resumes right after the call to the libgraph
	 * function. And over there, we will try to call DFS again. And so it
	 * goes until we fail to repeat the parse successfully. We do this
	 * zig-zagging to prevent the stack-size from exploding -- which it
	 * does for even the simplest grammars.
	 */
	if (ast->ast_dfs_nesting > 0 && ast->ast_dfs_unwind) {
		ast->ast_dfs_nesting = 0;
		ast->ast_dfs_unwind = 0;
		PARSE_NESTING(ast);
		return (1);
	}

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
	lg_dfs_rdnt_fold(g->grmr_graph, root, on_pop, on_push, state);
	PARSE_RUN_GRMR_END(g, ast);
	//printf("digraph ast {\n");
	//lg_edges(ast->ast_graph, print_ast);
	//printf("}\n");
	/*
	 * This is here as a filler. What _should_ we return, anyway?
	 */
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
