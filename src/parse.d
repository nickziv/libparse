inline int E_AST_CHILD = 1;
inline int E_ASTN_ID = 2;
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
inline int E_ASTN_CONTENT = 18;
inline int E_ASTN_NO_CONTENT = 19;
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

inline string e_test_descr[int err] =
	err == 0 ? "[ PASS ]" :
	err == E_AST_CHILD ? "[ BAD AST CHILD ]" :
	err == E_ASTN_ID ? "[ AST ID TOO BIG ]" :
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
	err == E_ASTN_CONTENT ? "[ NON PARSER HAS CONTENT ]" :
	err == E_ASTN_NO_CONTENT ? "[ PARSER HAS NO CONTENT ]" :
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
	"[[BAD ERROR CODE]]";

typedef enum regex_op {
	ONE,
	TWO
} regex_op_t;

typedef uintptr_t slablist_t;
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

typedef struct content {
	size_t		ctt_bits;
	char		*ctt_buf;
} content_t;

typedef struct lp_grmr lp_grmr_t;
typedef struct lp_ast_node lp_ast_node_t;
typedef struct lp_ast lp_ast_t;

struct lp_ast {
        int             ast_fin;
        lp_ast_node_t   *ast_start; /* starting ast_node */
        lg_graph_t      *ast_graph;
        slablist_t      *ast_nodes; /* index of ast_nodes */
        lp_grmr_t       *ast_grmr;
        void            *ast_in;
        size_t          ast_sz;
	int		ast_matched;
        slablist_t      *ast_stack;
        int             ast_dfs_nesting;
	int		ast_dfs_unwind;
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
	char            *an_gnm; /* grmr_node id */
	lp_ast_t        *an_ast;
	uint32_t        an_kids;
	uint32_t        an_index;
	lp_ast_node_t   *an_parent;
	lp_ast_node_t   *an_last_child;
	lp_ast_node_t   *an_left;
	lp_ast_node_t   *an_right;
	content_t       *an_content;
	ast_node_st_t   an_state;
	uint32_t        an_off_start;
	uint32_t        an_off_end;
};

typedef struct an_info {
	uint64_t	ani_id;
	string		ani_gnm;
	uint32_t	ani_type;
	uint32_t	ani_kids;
	uint32_t	ani_index;
	void		*ani_parent;
	void		*ani_last_child;
	void		*ani_right;
	void		*ani_left;
	void		*ani_content;
	uint32_t	ani_state;
	uint32_t	ani_off_start;
	uint32_t	ani_off_end;
} an_info_t;

#pragma D binding "1.6.1" translator
translator an_info_t < lp_ast_node_t *a >
{
	ani_id = *(uint64_t *)copyin((uintptr_t)&a->an_id,
			sizeof (a->an_id));

	ani_gnm = copyinstr(*(uintptr_t *)
			copyin(
			(uintptr_t)&a->an_gnm,
			sizeof (a->an_gnm)));

	ani_type = *(uint32_t *)copyin((uintptr_t)&a->an_type,
			sizeof (a->an_type));

	ani_state = *(uint32_t *)copyin((uintptr_t)&a->an_state,
			sizeof (a->an_state));

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
	int	asti_fin;
	void	*asti_in;
	size_t	asti_sz;
	int	asti_matched;
	int	asti_nesting;
	int	asti_unwind;
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
	asti_nesting = *(int *)copyin((uintptr_t)&a->ast_dfs_nesting,
			sizeof (a->ast_dfs_nesting));
	asti_unwind = *(int *)copyin((uintptr_t)&a->ast_dfs_unwind,
			sizeof (a->ast_dfs_unwind));
	asti_matched = *(int *)copyin((uintptr_t)&a->ast_matched,
			sizeof (a->ast_matched));
};
