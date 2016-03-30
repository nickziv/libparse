/*
 * This Source Code Form is subject to the terms of the Mozilla Public License,
 * v. 2.0. If a copy of the MPL was not distributed with this file, You can
 * obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2015, Nick Zivkovic
 */

#include <stdio.h>
#include <stdint.h>

/*
 */

typedef enum scrub_err {
	SCRUB_NOROOT,
	SCRUB_NOKIDS
} scrub_err_t;

typedef enum tok_op {
	/*
	 * Following 2 ops match zero, only one, or at least one of the
	 * segment. This makes it easy to express optional tokens --- tokens
	 * that can be present, but don't have to be. Whitespace in
	 * mathematical expressions is an example --- "1 + 2" and "1+2" have
	 * the same meaning.
	 */
	ROP_ZERO_ONE = 0,
	ROP_ZERO_ONE_PLUS,
	/*
	 * Following 2 ops match only one or at least one of the segments.
	 */
	ROP_ONE,
	ROP_ONE_PLUS,
	/*
	 * Matches anything in the segment in any order.
	 */
	ROP_ANYOF,
	ROP_ANYOF_ONE_PLUS,
	// maybe error in this one, under?
	ROP_ANYOF_ZERO_ONE,
	ROP_ANYOF_ZERO_ONE_PLUS,
	/*
	 * Explicitly fails if the thing in the segment is matched.
	 */
	ROP_NONEOF,
	ROP_END /* internally used */
} tok_op_t;

typedef enum n_type {
	/*
	 * Indicates that the children should be tried in sequence.
	 */
	SEQUENCER,
	/*
	 * Indicates a split or a fork -- 2 or more alternatives to what can be
	 * parsed next.
	 */
	SPLITTER,
	PARSER
} n_type_t;

typedef struct lp_grmr lp_grmr_t;
typedef struct lp_tok_ls lp_tok_ls_t;
typedef struct lp_ast lp_ast_t;
typedef struct lp_ast_node lp_ast_node_t;
typedef struct lp_tok lp_tok_t;
typedef struct lp_grmr_node lp_grmr_node_t;

typedef void lp_match_cb_t(void *b);
typedef void lp_scrub_cb_t(scrub_err_t err, char *gnm);
typedef void lp_tokpr_t(void *b);
typedef void lp_map_query_cb_t(lp_ast_node_t *v, void *arg);

lp_tok_t *lp_create_tok(lp_grmr_t *g, char *name);
/* XXX should data-alloc be handled by user, or copied into libparse? */
int lp_add_tok_op(lp_tok_t *r, tok_op_t op, uint8_t width, size_t elems,
    char *data);
int lp_add_tok_range_op(lp_tok_t *, tok_op_t, uint8_t, char *, char*);
/* returns id or 0 on failure */
int lp_add_tok(lp_grmr_t *g, char *nm, lp_tok_t *tok);
lp_grmr_t *lp_create_grammar(char *name);
void lp_destroy_grammar(lp_grmr_t *);
int lp_create_grmr_node(lp_grmr_t *g, char *name, char *tok, n_type_t ntype);
//void lp_add_edge(lp_grmr_t *g, uint64_t from, uint64_t to);
//void lp_rem_edge(lp_grmr_t *g, uint64_t from, uint64_t to);
//void lp_add_child(lp_grmr_t *g, uint64_t parent, uint64_t child);
int lp_add_child(lp_grmr_t *g, char *parent, char *child);
/* we add a root grmr_node --- we can have at least one */
int lp_root_grmr_node(lp_grmr_t *g, char *nm);
/* useful for making modified/derivative grammars */
/* TODO implement replace */
int lp_replace_grmr_node(lp_grmr_t *g, uint64_t gnid, uint64_t swid);
int lp_scrub_grammar(lp_grmr_t *g, lp_scrub_cb_t cb);
lp_ast_t *lp_create_ast();
void lp_destroy_ast(lp_ast_t *);
int lp_finalize_grammar(lp_grmr_t *);
int lp_run_grammar(lp_grmr_t *g, lp_ast_t *ast, void *in, size_t sz);

void lp_finish_run(lp_ast_t *ast);
lp_grmr_t *lp_clone_grammar(char *nm, lp_grmr_t *g);
/* it will find tokens that match expr foo and have id T */
void lp_match_token(lp_ast_t *r, uint64_t tokid, lp_tok_t *tok, lp_match_cb_t cb);
/* find intrators that match the id, and execute callback on them */
void lp_match_grmr_node(lp_ast_t *r, uint64_t id, lp_match_cb_t cb);
lp_grmr_node_t *lp_init_swap_intr(lp_grmr_t *, char *, char);
int lp_fini_swap_intr(lp_grmr_t *, lp_grmr_node_t *);
void lp_map_query(lp_ast_t *, char *, lp_ast_node_t *, lp_map_query_cb_t, void *);
int lp_cmp_contents(char *buf, size_t sz, lp_ast_node_t *c);
char *lp_get_contents(lp_ast_node_t *);
size_t lp_get_bitwidth(lp_ast_node_t *);
void lp_rem_contents(lp_ast_node_t *, char *);
int lp_cmp_name(lp_ast_node_t *, char *);
lp_ast_node_t *lp_get_root_node(lp_ast_t *);
lp_ast_node_t *lp_deref_splitter(lp_ast_node_t *);
char *lp_get_node_name(lp_ast_node_t *);
typedef void lp_grmr_cb_t(lp_grmr_node_t *, void *);
typedef void lp_ast_cb_t(lp_ast_node_t *, void *);
typedef int lp_flatten_cb_t(lp_ast_node_t *);
void lp_grmr_dfs(lp_grmr_t *, lp_grmr_cb_t);
void lp_grmr_bfs(lp_grmr_t *, lp_grmr_cb_t);
void lp_ast_dfs(lp_ast_t *, lp_ast_cb_t, void *);
void lp_ast_bfs(lp_ast_t *, lp_ast_cb_t, void *);
void lp_ast_nodes(lp_ast_t *, lp_ast_cb_t, void *);
void lp_dump_grmr(lp_grmr_t *);
int lp_map_cc(lp_ast_t *, char *mapnm, char *parent, char *kid1, char *kid2);
int lp_map_pd(lp_ast_t *, char *mapnm, char *parent, char *descendant);
