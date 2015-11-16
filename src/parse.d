
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
	err == 1 ? "[ BAD AST CHILD ]" :
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
	lp_ast_node_t	*an_meta;
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
	void		*ani_meta;
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

	ani_meta = *(void **)copyin((uintptr_t)&a->an_meta,
			sizeof (a->an_meta));

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
