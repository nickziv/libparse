typedef struct { int dummy; } lp_ast_t;
typedef struct { int dummy; } lp_grmr_t;
typedef struct { int dummy; } lp_grmr_node_t;
typedef struct { int dummy; } lp_ast_node_t;
typedef struct { int dummy; } grmrinfo_t;
typedef struct { int dummy; } astinfo_t;
typedef struct { int dummy; } an_info_t;
typedef struct { int dummy; } gn_info_t;

provider parse {
/*
	probe create_grmr(grmrinfo_t *);
	probe match_il(ilinfo_t *);

we may want a probe that says TAKE_BRANCH, and BACKTRACK, 

we also want an RGX_DO probe that has ROP_* as arg0, and maybe a rseg_info_t as
arg1.

How would we make a test suite for libparse? We can use the slablist method for
verifying internal state, but we would need a black-box method for verifying
ress.
*/
	/* a cursor (arg1) that corresponds to a cookie (arg0) */
	probe cursor(uint64_t, size_t);
	probe got_here(int);
	probe stack_elems(uint64_t, uint64_t);
	probe grmr_add_child(lp_grmr_t *g, char *p, char *c, uint64_t u);
	probe ast_push(lp_ast_node_t *n) : (an_info_t *n);
	probe rewind_begin();
	probe rewind_end();
	probe ast_pop(lp_ast_node_t *n) : (an_info_t *n);
	probe ast_add_child(lp_grmr_t *g, lp_ast_node_t *p, lp_ast_node_t *c) :
		(grmrinfo_t *g, an_info_t *p, an_info_t *c);
	probe ast_rem_child(lp_grmr_t *g, lp_ast_node_t *p, lp_ast_node_t *c) :
		(grmrinfo_t *g, an_info_t *p, an_info_t *c);
	probe create_grmr(lp_grmr_t *g) : (grmrinfo_t *g);
/*
	probe clone_grmr(lp_grmr_t *g, lp_grmr_t *c) :
		(grmrinfo_t *g, grmrinfo_t *c);
*/
	probe trace_ast(lp_ast_t *a, lp_ast_node_t *f, lp_ast_node_t *t,
		uint64_t e) : (astinfo_t *a, an_info_t *f, an_info_t *t,
		uint64_t e);
	probe create_ast(lp_ast_t *a) : (astinfo_t *a);
	probe nesting(lp_ast_t *a) : (astinfo_t *a);
	probe bind_begin();
	probe bind_end();
	probe run_grmr_begin(lp_grmr_t *g, lp_ast_t *a) :
		(grmrinfo_t *g, astinfo_t *a);
	probe run_grmr_end(lp_grmr_t *g, lp_ast_t *r) :
		(grmrinfo_t *g, astinfo_t *r);
	probe fail(lp_ast_node_t *a) : (an_info_t *a);
	probe match(lp_ast_node_t *a) : (an_info_t *a);
	probe match_part(lp_ast_node_t *a) : (an_info_t *a);
	probe ast_node_off_start(lp_grmr_t *g, lp_ast_node_t *n) :
		(grmrinfo_t *g, an_info_t *n);
	probe ast_node_off_end(lp_grmr_t *g, lp_ast_node_t *n) :
		(grmrinfo_t *g, an_info_t *n);
	/*probe res_fini(lp_ast_t *r) : (astinfo_t *r);*/
	probe finish_ast_node(lp_grmr_t *g, lp_ast_node_t *i) :
		(grmrinfo_t *g, an_info_t *i);
	probe create_ast_node(lp_grmr_t *g, lp_ast_node_t *i) :
		(grmrinfo_t *g, an_info_t *i);
	probe destroy_ast_node(lp_ast_node_t *i) : (an_info_t *i);
	probe create_grmr_node(lp_grmr_t *g, lp_grmr_node_t *i, char *t) :
		(grmrinfo_t *g, gn_info_t *i, char *t);
	probe reset_grmr_node(lp_grmr_t *g, lp_grmr_node_t *i, char *t) :
		(grmrinfo_t *g, gn_info_t *i, char *t);
	/* XXX hmmm, looks like we never destroy ir's */
	probe destroy_grmr_node(void *, void *);
	probe create_tok(void *);
	probe eval_tok(lp_grmr_t *g, int i, int m) :
		(grmrinfo_t *g, int i, int m);
	probe set_root(lp_grmr_t *g, char *i) :
		(grmrinfo_t *g, char *i);
	probe matched_bits(int a, void *b, lp_grmr_node_t *c, uint64_t d,
		uint64_t e) :
		(int a, void *b, gn_info_t *c, uint64_t d, uint64_t e); 
	probe unmatched_bits(int a, void *b, lp_grmr_node_t *c, uint64_t d,
		uint64_t e) :
		(int a, void *b, gn_info_t *c, uint64_t d, uint64_t e); 
	/* grmr, itegrator backtracked to, corresponding ast_node */
	probe repeat(lp_ast_node_t *a) : (an_info_t *a);
	probe repeat_retry(lp_ast_node_t *a) : (an_info_t *a);
	probe parse();
	probe split(lp_ast_node_t *a) : (an_info_t *a);
	probe input_end(lp_ast_t *r) : (astinfo_t *r);
	/*
	 * The test probes.
	 */
	probe test_add_child(int e);
	probe test_ast_node(int e);
	probe test_grmr_node(int e);
	probe test_grmr(int e);
	probe test_ast(int e);
	probe test_tok(int e);
	probe test_tok_seg(int e);
};

#pragma D attributes Evolving/Evolving/ISA      provider parse provider
#pragma D attributes Private/Private/Unknown    provider parse module
#pragma D attributes Private/Private/Unknown    provider parse function
#pragma D attributes Private/Private/ISA        provider parse name
#pragma D attributes Evolving/Evolving/ISA      provider parse args
