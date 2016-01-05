/*
 * This Source Code Form is subject to the terms of the Mozilla Public License,
 * v. 2.0. If a copy of the MPL was not distributed with this file, You can
 * obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2015, Nick Zivkovic
 */

#include <slablist.h>
#include <setjmp.h>
#include <graph.h>
#include <strings.h>
#include "parse.h"

typedef struct tok_seg {
	tok_op_t	ts_op;
	uint8_t		ts_width; /* in bits */
	size_t		ts_elems;
	char		*ts_data;
} tok_seg_t;

/*
 * The token type has a unique user-given id. It is associated with a tok. If
 * the tok is NULL or the token id conflicts, it is dropped. 
 */
typedef struct lp_tok {
	char		*tok_nm;
	slablist_t	*tok_segs;
} lp_tok_t;

/*
 * Every ast_node_t has an associated state. The ANS_TRY state is the starting
 * state. It basically signifies that we have yet to try to match the input the
 * ast_node's associates grmr_node. Once we actually try to get a match, we can
 * either reach ANS_FAIL or ANS_MATCH. 
 */
typedef enum ast_node_st {
	ANS_TRY = 0,
	ANS_FAIL,
	ANS_MATCH
} ast_node_st_t;



/*
 * The ast_node is the result of running/evaluating an grmr_node. The ast_node
 * stores the _literal_ result (a string of bits). It can also store other
 * sub-ast_nodes, in order to preserve the structure of grmr_nodes, for later
 * analysis and derivation.
 */
typedef struct lp_ast_node lp_ast_node_t;
struct lp_ast_node {
	uint64_t	an_id; /* unique id */
	n_type_t	an_type;
	char		*an_gnm; /* grmr_node id */
	lp_ast_t	*an_ast;
	uint32_t	an_kids;
	uint32_t	an_index;
	lp_ast_node_t	*an_parent;
	lp_ast_node_t	*an_last_child;
	lp_ast_node_t	*an_left;
	lp_ast_node_t	*an_right;
	ast_node_st_t	an_state;
	uint32_t	an_off_start;
	uint32_t	an_off_end;
};

typedef struct lp_grmr_node {
	n_type_t	gn_type;
	uint16_t	gn_kids;
	char		*gn_name;
	/* If the node is a parser, it needs a token to parse */
	char		*gn_tok;
} lp_grmr_node_t;

struct lp_tok_ls {
	slablist_t	*tl_list;
};

struct lp_grmr {
	uint64_t	grmr_refcnt;
	char		*grmr_name;	/* debug name */
	char		grmr_fin;	/* bool */
	lp_tok_ls_t	*grmr_toks;
	slablist_t	*grmr_gnodes;/* srt ls of all grmr_node-ptrs */
	lg_graph_t	*grmr_graph;
	lp_grmr_node_t	*grmr_root;
};


/*
 * The `lp_ast` struct carries some state, a return status, and the root
 * ast_node. It contains either a full or partial ast_node tree. The tree is
 * implemented in terms of a graph. There is also another graph which is called
 * `ast_to_remove`. This is where we store edges from `ast_graph` , which will
 * be removed later. The idea is that we essentially clone a subgraph of
 * `ast_graph` into `ast_to_remove`, and then we iterate over `ast_to_remove`
 * using `lg_edges` and call `lp_rem_ast_child() on the two nodes in each edge.
 * We do this, because we can't simultaneously walk a graph and modify it --
 * doing so confuses the walking routines which assume that the graph isn't
 * changing beneath their feet.
 *
 * Note that the `ast_to_remove` graph has the unfortunate property of
 * requiring log(N) insertions instead of O(1) insertions -- it's really just a
 * queue. A future feature of libgraph will include an edge-queue structure
 * which allow one to use lg_connect() code to add edges to the queue, and
 * iterate over them in a linear fashion. One _could_ implement the equivalent
 * functionality in libparse instead, but we would rather let libgraph re-use
 * its own edge_t structs. Until this feature gets implemented we will use the
 * lg_graph_t (TODO).
 */
struct lp_ast {
	int		ast_fin;
	lp_ast_node_t	*ast_start; /* starting ast_node */
	lg_graph_t	*ast_graph; /* the abstract syntax tree */
	slablist_t	*ast_nodes; /* index of ast_nodes */
	slablist_t	*ast_rem_q;
	lp_grmr_t	*ast_grmr;
	void		*ast_in;
	size_t		ast_sz;
	int		ast_matched;
	int		ast_eoi; /* end of input */
	int		ast_bail; /* tell DFS to bail */
	uint32_t	ast_nsplit; /* splitters pushed to stack */
	slablist_t	*ast_stack;
	lg_graph_t	*ast_to_remove;
};

/*
 * Sometimes we want to remove edge from the graph that represents an AST. In
 * order to do this, we essentially have to fold over the slablist of edges.
 * But we can't remove the target edges while we fold, because changing a
 * slablist while a fold is running has undefined results. It's like trying to
 * run on a bridge that's collapsing under your feet. So we have to queue up
 * the edges for removal, and then remove them when the fold finishes.
 */
typedef struct qed_edge {
	gelem_t		qed_from;
	gelem_t		qed_to;
	gelem_t		qed_weight;
} qed_edge_t;

/*
 * Memory allocation funcs.
 */
lp_tok_t *lp_mk_tok(void);
tok_seg_t *lp_mk_tok_seg(void);
lp_tok_ls_t *lp_mk_tok_ls(void);
lp_grmr_t *lp_mk_grmr(void);
lp_ast_t *lp_mk_ast(void);
lp_grmr_node_t *lp_mk_grmr_node(void);
lp_ast_node_t *lp_mk_ast_node(void);
qed_edge_t *lp_mk_qed_edge(void);
void *lp_mk_buf(size_t);
void *lp_mk_zbuf(size_t);
void lp_rm_tok(lp_tok_t *);
void lp_rm_tok_seg(tok_seg_t *);
void lp_rm_tok_ls(lp_tok_ls_t *);
void lp_rm_grmr(lp_grmr_t *);
void lp_rm_ast(lp_ast_t *);
void lp_rm_grmr_node(lp_grmr_node_t *);
void lp_rm_ast_node(lp_ast_node_t *);
void lp_rm_qed_edge(qed_edge_t *);
void lp_rm_buf(void *, size_t);
int parse_umem_init(void);

/*
 * Functions used in more than one file.
 */
lp_grmr_node_t *find_grmr_node(lp_grmr_t *, char *);
