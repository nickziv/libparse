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
	char		*ts_range_min;
	char		*ts_range_max;
	range_flag_t	ts_range_flag;
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
	uint64_t	an_snap; /* for splitters only */
	uint64_t	an_srefc;
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

typedef struct lp_map_search {
	lp_grmr_node_t	*ms_par;
	lp_grmr_node_t	*ms_key;
	lp_grmr_node_t	*ms_val;
	int		ms_key_found;
	int		ms_val_found;
} lp_map_search_t;

typedef struct lp_mapping {
	char		*map_name;
	char		*map_pd_par; /* used for pd-maps */
	lg_graph_t	*map_graph;
} lp_mapping_t;

typedef struct map_query_arg {
	lp_map_query_cb_t	*mqa_cb;
	void			*mqa_arg;
	lp_ast_node_t		*mqa_key; /* XXX do we need this? */
} map_query_arg_t;

typedef struct lp_map_cookie {
	lp_ast_node_t	*mc_p;
	lp_ast_node_t	*mc_k;
	lp_ast_node_t	*mc_v;
	int		mc_found;
	lp_map_search_t	*mc_ms;
	lp_mapping_t	*mc_mapping;
} lp_map_cookie_t;


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
	int8_t		grmr_scrub_err;
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
	int		ast_cloning;
	lp_ast_node_t	*ast_start; /* starting ast_node */
	lg_graph_t	*ast_graph; /* the abstract syntax tree */
	slablist_t	*ast_nodes; /* index of ast_nodes, by name */
	lp_grmr_t	*ast_grmr;
	void		*ast_in;
	size_t		ast_sz;
	int		ast_matched;
	int		ast_eoi; /* end of input */
	int		ast_bail; /* tell DFS to bail */
	uint32_t	ast_nsplit; /* splitters pushed to stack */
	uint32_t	ast_max_off;
	lp_ast_node_t	*ast_last_leaf;
	lg_graph_t	*ast_to_remove;
	slablist_t	*ast_mappings;
};

/*
 * We use BFS to compare 2 grammars or 2 asts. We record the info from the
 * first walk in the cookie, and we use this info to verify that the second
 * walk is identical.
 */
typedef struct cmp_cookie {
	lp_grmr_t	*cmpck_g1;
	lp_grmr_t	*cmpck_g2;
	int		cmpck_graph;
	int		cmpck_walk_mismatch;
	int		cmpck_type_mismatch;
	int		cmpck_seg_mismatch;
	lp_grmr_node_t	*cmpck_type1;
	lp_grmr_node_t	*cmpck_type2;
	lp_grmr_node_t	*cmpck_seg1;
	lp_grmr_node_t	*cmpck_seg2;
	slablist_bm_t	*cmpck_bm;
	slablist_t	*cmpck_walk;
} cmp_cookie_t;

typedef struct cmp_walk_step {
	char		*cws_from;
	char		*cws_to;
	uint64_t	cws_w;
} cmp_walk_step_t;

typedef struct reaction {
	char		*rtn_node_name;
	uint64_t	rtn_touched;
	lp_ast_rct_cb_t	*rtn_cb;
} reaction_t;

typedef struct lp_reactor {
	slablist_t	*rct_name_cb;
	lp_ast_t	*rct_ast;
	reaction_t	*rct_cur_cb;
	slablist_t	*rct_stack;
	slablist_bm_t	*rct_bm;
} lp_reactor_t;

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
cmp_walk_step_t *lp_mk_cmp_walk_step(void);
lp_mapping_t *lp_mk_mapping(void);
void *lp_mk_buf(size_t);
void *lp_mk_zbuf(size_t);
void lp_rm_tok(lp_tok_t *);
void lp_rm_tok_seg(tok_seg_t *);
void lp_rm_tok_ls(lp_tok_ls_t *);
void lp_rm_grmr(lp_grmr_t *);
void lp_rm_ast(lp_ast_t *);
void lp_rm_grmr_node(lp_grmr_node_t *);
void lp_rm_ast_node(lp_ast_node_t *);
void lp_rm_cmp_walk_step(cmp_walk_step_t *);
void lp_rm_mapping(lp_mapping_t *);
void lp_rm_buf(void *, size_t);
int parse_umem_init(void);

/*
 * Functions used in more than one file.
 */
lp_grmr_node_t *find_grmr_node(lp_grmr_t *, char *);
