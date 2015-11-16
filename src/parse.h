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

typedef enum tok_op {
	/*
	 * Following 2 ops match zero, only one, or at least one of the
	 * segment. This makes it easy to expasts optional tokens --- tokens
	 * that can be pastent, but don't have to be. Whitespace in
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
	 * Matches everything in the segment.
	 */
	ROP_ALLOF,
	/*
	 * Matches anything in the segment in any order.
	 */
	ROP_ANYOF, //TODO
	ROP_ANYOF_ONE_PLUS, //TODO
	// maybe error in this one, under?
	ROP_ANYOF_ZERO_ONE_PLUS, //TODO
	/*
	 * Explicitly fails if the thing in the segment is matched.
	 */
	ROP_NONEOF, //TODO
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
typedef struct lp_tok lp_tok_t;
typedef struct lp_grmr_node lp_grmr_node_t;

typedef void lp_match_cb_t(void *b);
typedef void lp_tokpr_t(void *b);

lp_tok_t *lp_create_tok(lp_grmr_t *g, char *name);
/* XXX should data-alloc be handled by user, or copied into libparse? */
int lp_add_tok_op(lp_tok_t *r, tok_op_t op, uint8_t width, size_t elems,
    char *data);
/* returns id or 0 on failure */
int lp_add_tok(lp_grmr_t *g, char *nm, lp_tok_t *tok);
/* sets contents, given pointer or, alternatively, id */
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
int lp_scrub_grammar(lp_grmr_t *g);
lp_ast_t *lp_create_ast();
void lp_destroy_ast(lp_ast_t *);
int lp_finalize_grammar(lp_grmr_t *);
int lp_run_grammar(lp_grmr_t *g, lp_ast_t *ast, void *in, size_t sz);

int lp_finish_run(lp_ast_t *ast);
lp_grmr_t *lp_clone_grammar(char *nm, lp_grmr_t *g);
/* it will find tokens that match expr foo and have id T */
void lp_match_token(lp_ast_t *r, uint64_t tokid, lp_tok_t *tok, lp_match_cb_t cb);
/* find intrators that match the id, and execute callback on them */
void lp_match_grmr_node(lp_ast_t *r, uint64_t id, lp_match_cb_t cb);
lp_grmr_node_t *lp_init_swap_intr(lp_grmr_t *, char *, char);
int lp_fini_swap_intr(lp_grmr_t *, lp_grmr_node_t *);
typedef void lp_grmr_cb_t(lp_grmr_node_t *);
typedef void lp_ast_cb_t(lp_grmr_node_t *);
void lp_walk_grmr_dfs(lp_grmr_t *, lp_grmr_cb_t);
void lp_walk_grmr_bfs(lp_grmr_t *, lp_grmr_cb_t);
int lp_bfs_walk_grammar(lp_grmr_t *, lp_ast_t *ast, void *, size_t);
int lp_dfs_walk_grammar(lp_grmr_t *, lp_ast_t *ast, void *, size_t);
void lp_walk_ast_dfs(lp_grmr_t *, lp_ast_cb_t);
void lp_walk_ast_bfs(lp_grmr_t *, lp_ast_cb_t);
