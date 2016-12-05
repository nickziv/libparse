inline int E_AST_CHILD = 1;
inline int E_ASTN_TYPE = 3;
inline int E_ASTN_NULL_GNM = 4;
inline int E_ASTN_OFFSET = 5;
inline int E_ASTN_LR = 6;
inline int E_ASTN_LRPTRSN = 7;
inline int E_AST_NEIGHBOR = 8;
inline int E_AST_NEIGHBORS = 9;
inline int E_ASTN_NO_PARENT = 10;
inline int E_ASTN_NO_LAST_CHILD = 11;
inline int E_ASTN_TOO_FEW_KIDS = 12;
inline int E_ASTN_STATE = 13;
inline int E_ASTN_OFF_START_GT_IN = 14;
inline int E_ASTN_OFF_END_GT_IN = 15;
inline int E_ASTN_LRPTRS = 16;
inline int E_ASTN_LRPTRS_NULL = 17;
inline int E_GN_TYPE = 20;
inline int E_GN_TOK_NULL = 21;
inline int E_GN_NAME_NULL = 23;
inline int E_AST_NULL_INPUT = 24;
inline int E_AST_ZERO_SIZE_INPUT = 25;
inline int E_TOK_NAME_NULL = 26;
inline int E_TOK_SEGS_NULL = 27;
inline int E_TOK_SEGS_EMPTY = 28;
inline int E_TSEG_OP = 29;
inline int E_TSEG_WIDTH_ZERO = 30;
inline int E_TSEG_DATA = 31;
inline int E_AST_NSPLIT_TOO_BIG = 32;
inline int E_AST_NSPLIT_TOO_SMALL = 33;
inline int E_ASTN_SPLIT_NKIDS = 34;
inline int E_ASTN_INDEX_DIFF2BIG = 35;
inline int E_ASTN_INDEX_DIFF2SMALL = 36;
inline int E_AST_GRAPH_WEIGHTS = 37;
inline int E_AST_STACK_DUPS = 38;
inline int E_AST_REF = 39;
inline int E_AST_REF_DIFF = 40;

inline string tok_op[int op] =
	op == 0 ? "ROP_ZERO_ONE" :
	op == 1 ? "ROP_ZERO_ONE_PLUS" :
	op == 2 ? "ROP_ONE" :
	op == 3 ? "ROP_ONE_PLUS" :
	op == 4 ? "ROP_ALLOF" :
	op == 5 ? "ROP_ANYOF" :
	op == 6 ? "ROP_ANYOF_ONE_PLUS" :
	op == 7 ? "ROP_ANYOF_ZERO_ONE_PLUS" :
	op == 8 ? "ROP_NONEOF" :
	"ROP_ERROR: UNKNOWN CODE";

inline string lp_e_test_descr[int err] =
	err == 0 ? "[ PASS ]" :
	err == E_AST_CHILD ? "[ BAD AST CHILD ]" :
	err == E_ASTN_TYPE ? "[ INVALID ASTN TYPE ]" :
	err == E_ASTN_NULL_GNM ? "[ ASTN NO GNM ]" :
	err == E_ASTN_OFFSET ? "[ OFF START > OFF END ]" :
	err == E_ASTN_LR ? "[ unused ]" :
	err == E_ASTN_LRPTRSN ? "[ unused ]" :
	err == E_AST_NEIGHBOR ? "[ ]" :
	err == E_AST_NEIGHBORS ? "[ ]" :
	err == E_ASTN_NO_PARENT ? "[ AST NODE NO PARENT ]" :
	err == E_ASTN_NO_LAST_CHILD ? "[ AST NODE NO LAST CHILD ]" :
	err == E_ASTN_TOO_FEW_KIDS ? "[ AST NODE TOO FEW KIDS ]" :
	err == E_ASTN_STATE ? "[ BAD AST NODE STATE ]" :
	err == E_ASTN_OFF_START_GT_IN ? "[ OFF START > AST IN SZ ]" :
	err == E_ASTN_OFF_END_GT_IN ? "[ OFF END > AST IN SZ ]" :
	err == E_ASTN_LRPTRS ? "[ ONLY CHILD HAS LEFT/RIGHT ]" :
	err == E_ASTN_LRPTRS_NULL ? "[ SIBLING HAS NO LEFT/RIGHT ]" :
	err == E_GN_TYPE ? "[ INVALID GNODE TYPE ]" :
	err == E_GN_TOK_NULL ? "[ GNODE TOKEN NULL ]" :
	err == E_GN_NAME_NULL ? "[ GNODE NAME NULL ]" :
	err == E_AST_NULL_INPUT ? "[ AST HAS NULL INPUT ]" :
	err == E_AST_ZERO_SIZE_INPUT ? "[ AST HAS ZERO SIZED INPUT ]" :
	err == E_TOK_NAME_NULL ? "[ TOK NAME NULL ]" :
	err == E_TOK_SEGS_NULL ? "[ TOK SEGS NULL ]" :
	err == E_TOK_SEGS_EMPTY ? "[ TOK SEGS EMPTY ]" :
	err == E_TSEG_OP ? "[ TSEG OP INVALID ]" :
	err == E_TSEG_WIDTH_ZERO ? "[ TSEG WIDTH ZERO ]" :
	err == E_TSEG_DATA ? "[ TSEG DATA NULL ]" :
	err == E_AST_NSPLIT_TOO_BIG ? "[ NSPLIT TOO BIG ]" :
	err == E_AST_NSPLIT_TOO_SMALL ? "[ NSPLIT TOO SMALL ]" :
	err == E_ASTN_SPLIT_NKIDS ? "[ SPLITTER NKIDS TOO BIG ]" :
	err == E_ASTN_INDEX_DIFF2BIG ? "[ INDEX DIFF TOO BIG ]" :
	err == E_ASTN_INDEX_DIFF2SMALL ? "[ INDEX DIFF TOO SMALL ]" :
	err == E_AST_GRAPH_WEIGHTS ? "[ GRAPH WEIGHTS NOT MONOTOMIC ]" :
	err == E_AST_STACK_DUPS ? "[ STACK HAS DUPLICATE AST NODES ]" :
	err == E_AST_REF ? "[ NODE IN AST HAS BAD BACKPTR ]" :
	err == E_AST_REF_DIFF ? "[ PARENT AND CHILD NODES DIFF ASTS ]" :
	"[[BAD ERROR CODE]]";

typedef enum regex_op {
	ONE,
	TWO
} regex_op_t;

typedef uintptr_t lg_graph_t;
typedef uintptr_t lp_tok_ls_t;
typedef uintptr_t slablist_bm_t;
typedef struct regex_seg {
	regex_op_t	rs_op;
	int		rs_g; /* id of grmr_node this seg is part of */
	uint8_t		rs_width; /* in bits */
	size_t		rs_size;
	char		*rs_data;
} regex_seg_t;

typedef struct lp_regex {
	slablist_t	*rgx_segs;
} lp_regex_t;

/*
 * The token type has a unique user-given id. It is associated with a regex. If
 * the regex is NULL or the token id conflicts, it is dropped. 
 */
typedef struct tok {
	char		*tok_nm;
	lp_regex_t	*tok_regex;
} tok_t;

typedef struct lp_grmr lp_grmr_t;
typedef struct lp_ast_node lp_ast_node_t;
typedef struct lp_ast lp_ast_t;

struct lp_ast {
	int             ast_fin;
	lp_ast_node_t   *ast_start; /* starting ast_node */
	lg_graph_t      *ast_graph; /* the abstract syntax tree */
	slablist_t      *ast_nodes; /* index of ast_nodes */
	lp_grmr_t       *ast_grmr;
	void            *ast_in;
	size_t          ast_sz;
	int             ast_matched;
	int             ast_eoi; /* end of input */
	int             ast_bail; /* tell DFS to bail */
	uint32_t        ast_nsplit; /* splitters pushed to stack */
	slablist_t      *ast_stack;
	lg_graph_t      *ast_to_remove;
	slablist_t      *ast_mappings;
};

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
	/*
	 * Repeats its children as many times as it can.
	 */
	REPEATER,
	PARSER
} n_type_t;

typedef enum ast_node_st {
	ANS_TRY = 0,
	ANS_FAIL,
	ANS_MATCH,
	ANS_MATCH_PART
} ast_node_st_t;

struct lp_ast_node {
	uint64_t        an_id; /* unique id */
	n_type_t        an_type;
	uint64_t        an_snap; /* for splitters only */
	uint64_t        an_srefc;
	char            *an_gnm; /* grmr_node id */
	lp_ast_t        *an_ast;
	uint32_t        an_kids;
	uint32_t        an_index;
	lp_ast_node_t   *an_parent;
	lp_ast_node_t   *an_last_child;
	lp_ast_node_t   *an_left;
	lp_ast_node_t   *an_right;
	ast_node_st_t   an_state;
	uint8_t		*an_contents;
	uint32_t        an_off_start;
	uint32_t        an_off_end;
};

typedef struct an_info {
	uint64_t	ani_id;
	uintptr_t	ani_gnm_ptr;
	string		ani_gnm;
	uint32_t	ani_type;
	uint32_t	ani_kids;
	uint32_t	ani_index;
	void		*ani_parent;
	void		*ani_last_child;
	void		*ani_right;
	void		*ani_left;
	uint32_t	ani_state;
	uint32_t	ani_off_start;
	uint32_t	ani_off_end;
} an_info_t;

#pragma D binding "1.6.1" translator
translator an_info_t < lp_ast_node_t *a >
{
	ani_id = *(uint64_t *)copyin((uintptr_t)&a->an_id,
			sizeof (a->an_id));

	ani_gnm_ptr = *(uintptr_t *)copyin(
			(uintptr_t)&a->an_gnm,
			sizeof (a->an_gnm));

	ani_gnm = copyinstr(*(uintptr_t *)
			copyin(
			(uintptr_t)&a->an_gnm,
			sizeof (a->an_gnm)));

	ani_type = *(uint32_t *)copyin((uintptr_t)&a->an_type,
			sizeof (a->an_type));

	ani_state = *(uint32_t *)copyin((uintptr_t)&a->an_state,
			sizeof (a->an_state));

	ani_index = *(uint32_t *)copyin((uintptr_t)&a->an_index,
			sizeof (a->an_index));

	ani_kids = *(uint32_t *)copyin((uintptr_t)&a->an_kids,
			sizeof (a->an_kids));

	ani_off_start = *(uint32_t *)copyin((uintptr_t)&a->an_off_start,
			sizeof (a->an_off_start));

	ani_off_end = *(uint32_t *)copyin((uintptr_t)&a->an_off_end,
			sizeof (a->an_off_end));
};

typedef struct lp_grmr_node {
	n_type_t        gn_type;
	uint16_t        gn_kids;
	char            *gn_name;
	/* If the node is a parser, it needs a token to parse */
	char            *gn_tok;
} lp_grmr_node_t;

typedef struct gn_info {
	char		gni_type;
	uint64_t	gni_kids;
	string		gni_name;
	string		gni_tok;
} gn_info_t;

#pragma D binding "1.6.1" translator
translator gn_info_t < lp_grmr_node_t *g >
{
	gni_name = copyinstr(*(uintptr_t *)
			copyin(
			(uintptr_t)&g->gn_name,
			sizeof (g->gn_name)));

	gni_type = *(char *)copyin((uintptr_t)&g->gn_type,
			sizeof (g->gn_type));

	gni_kids = *(uint64_t *)copyin((uintptr_t)&g->gn_kids,
			sizeof (g->gn_kids));

	gni_tok = copyinstr(*(uintptr_t *)
			copyin(
			(uintptr_t)&g->gn_tok,
			sizeof (g->gn_tok)));

};

struct lp_grmr {
	uint64_t        grmr_refcnt;
	char            *grmr_name;     /* debug name */
	char            grmr_fin;       /* bool */
	lp_tok_ls_t     *grmr_toks;
	slablist_t      *grmr_gnodes;/* srt ls of all grmr_node-ptrs */
	slablist_t      *grmr_bindings;/* srt ls of all binding-ptrs */
	lg_graph_t      *grmr_graph;
	lp_grmr_node_t  *grmr_root;
};


typedef struct grmrinfo {
	uint64_t	gi_refcnt;
	string		gi_name;
} grmrinfo_t;

#pragma D binding "1.6.1" translator
translator grmrinfo_t < lp_grmr_t *g >
{
	gi_refcnt = *(uint64_t *)copyin((uintptr_t)&g->grmr_refcnt,
			sizeof (g->grmr_refcnt));

	gi_name = copyinstr(*(uintptr_t *)
			copyin(
			(uintptr_t)&g->grmr_name,
			sizeof (g->grmr_name)));
};


typedef struct astinfo {
	int		asti_fin;
	void		*asti_in;
	size_t		asti_sz;
	uint32_t 	asti_nsplit;
	int		asti_matched;
	uintptr_t	asti_stack;
} astinfo_t;

#pragma D binding "1.6.1" translator
translator astinfo_t < lp_ast_t *a >
{
	asti_fin = *(int *)copyin((uintptr_t)&a->ast_fin,
			sizeof (a->ast_fin));
	asti_in = *(void **)copyin((uintptr_t)&a->ast_in,
			sizeof (a->ast_in));
	asti_sz = *(size_t *)copyin((uintptr_t)&a->ast_sz,
			sizeof (a->ast_sz));
	asti_nsplit = *(uint32_t*)copyin((uint32_t)&a->ast_nsplit,
			sizeof (a->ast_nsplit));
	asti_matched = *(int *)copyin((uintptr_t)&a->ast_matched,
			sizeof (a->ast_matched));
	asti_stack = *(uintptr_t *)copyin((uintptr_t)&a->ast_stack,
			sizeof (a->ast_stack));
};
