/*
 * Copyright 2015 Nicholas Zivkovic. All rights reserved.
 * Use is subject to license terms.
 */

#ifdef UMEM
#include <umem.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "parse_impl.h"

#define	UNUSED(x) (void)(x)
#define	CTOR_HEAD	UNUSED(ignored); UNUSED(flags)

umem_cache_t *cache_tok_seg;
umem_cache_t *cache_tok_ls;
umem_cache_t *cache_tok;
umem_cache_t *cache_altq;
umem_cache_t *cache_grmr;
umem_cache_t *cache_ast;
umem_cache_t *cache_grmr_node;
umem_cache_t *cache_ast_node;
umem_cache_t *cache_cmp_walk_step;
umem_cache_t *cache_mapping;


#ifdef UMEM
int
tok_seg_ctor(void *buf, void *ignored, int flags)
{
	CTOR_HEAD;
	tok_seg_t *r = buf;
	bzero(r, sizeof (tok_seg_t));
	return (0);
}

int
tok_ls_ctor(void *buf, void *ignored, int flags)
{
	CTOR_HEAD;
	lp_tok_ls_t *r = buf;
	bzero(r, sizeof (lp_tok_ls_t));
	return (0);
}

int
tok_ctor(void *buf, void *ignored, int flags)
{
	CTOR_HEAD;
	lp_tok_t *r = buf;
	bzero(r, sizeof (lp_tok_t));
	return (0);
}

int
grmr_ctor(void *buf, void *ignored, int flags)
{
	CTOR_HEAD;
	lp_grmr_t *r = buf;
	bzero(r, sizeof (lp_grmr_t));
	return (0);
}

int
ast_ctor(void *buf, void *ignored, int flags)
{
	CTOR_HEAD;
	lp_ast_t *r = buf;
	bzero(r, sizeof (lp_ast_t));
	return (0);
}

int
grmr_node_ctor(void *buf, void *ignored, int flags)
{
	CTOR_HEAD;
	lp_grmr_node_t *r = buf;
	bzero(r, sizeof (lp_grmr_node_t));
	return (0);
}

int
ast_node_ctor(void *buf, void *ignored, int flags)
{
	CTOR_HEAD;
	lp_ast_node_t *r = buf;
	bzero(r, sizeof (lp_ast_node_t));
	return (0);
}

int
cmp_walk_step_ctor(void *buf, void *ignored, int flags)
{
	CTOR_HEAD;
	cmp_walk_step_t *r = buf;
	bzero(r, sizeof (cmp_walk_step_t));
	return (0);
}

int
mapping_ctor(void *buf, void *ignored, int flags)
{
	CTOR_HEAD;
	lp_mapping_t *r = buf;
	bzero(r, sizeof (lp_mapping_t));
	return (0);
}

#endif

int
parse_umem_init()
{
#ifdef UMEM
	cache_tok_seg = umem_cache_create("tok_seg",
		sizeof (tok_seg_t),
		0,
		tok_seg_ctor,
		NULL,
		NULL,
		NULL,
		NULL,
		0);

	cache_tok_ls = umem_cache_create("tok_ls",
		sizeof (lp_tok_ls_t),
		0,
		tok_ls_ctor,
		NULL,
		NULL,
		NULL,
		NULL,
		0);

	cache_tok = umem_cache_create("tok",
		sizeof (lp_tok_t),
		0,
		tok_ctor,
		NULL,
		NULL,
		NULL,
		NULL,
		0);

	cache_grmr = umem_cache_create("grmr",
		sizeof (lp_grmr_t),
		0,
		grmr_ctor,
		NULL,
		NULL,
		NULL,
		NULL,
		0);

	cache_ast = umem_cache_create("ast",
		sizeof (lp_ast_t),
		0,
		ast_ctor,
		NULL,
		NULL,
		NULL,
		NULL,
		0);

	cache_grmr_node = umem_cache_create("grmr_node",
		sizeof (lp_grmr_node_t),
		0,
		grmr_node_ctor,
		NULL,
		NULL,
		NULL,
		NULL,
		0);

	cache_ast_node = umem_cache_create("ast_node",
		sizeof (lp_ast_node_t),
		0,
		ast_node_ctor,
		NULL,
		NULL,
		NULL,
		NULL,
		0);

	cache_cmp_walk_step = umem_cache_create("cmp_walk_step",
		sizeof (cmp_walk_step_t),
		0,
		cmp_walk_step_ctor,
		NULL,
		NULL,
		NULL,
		NULL,
		0);

	cache_mapping = umem_cache_create("mapping",
		sizeof (lp_mapping_t),
		0,
		mapping_ctor,
		NULL,
		NULL,
		NULL,
		NULL,
		0);


#endif
	return (0);
}


tok_seg_t *
lp_mk_tok_seg()
{
#ifdef UMEM
	return (umem_cache_alloc(cache_tok_seg, UMEM_NOFAIL));
#else
	return (calloc(1, sizeof (tok_seg_t)));
#endif

}

void
lp_rm_tok_seg(tok_seg_t *r)
{
#ifdef UMEM
	bzero(r, sizeof (tok_seg_t));
	umem_cache_free(cache_tok_seg, r);
#else
	bzero(r, sizeof (tok_seg_t));
	free(r);
#endif
}

lp_tok_ls_t *
lp_mk_tok_ls()
{
#ifdef UMEM
	return (umem_cache_alloc(cache_tok_ls, UMEM_NOFAIL));
#else
	return (calloc(1, sizeof (lp_tok_ls_t)));
#endif

}

void
lp_rm_tok_ls(lp_tok_ls_t *t)
{
#ifdef UMEM
	bzero(t, sizeof (lp_tok_ls_t));
	umem_cache_free(cache_tok_ls, t);
#else
	bzero(t, sizeof (lp_tok_ls_t));
	free(t);
#endif
}

lp_tok_t *
lp_mk_tok()
{
#ifdef UMEM
	return (umem_cache_alloc(cache_tok, UMEM_NOFAIL));
#else
	return (calloc(1, sizeof (lp_tok_t)));
#endif

}

void
lp_rm_tok(lp_tok_t *t)
{
#ifdef UMEM
	bzero(t, sizeof (lp_tok_t));
	umem_cache_free(cache_tok, t);
#else
	bzero(t, sizeof (lp_tok_t));
	free(t);
#endif
}

lp_grmr_t *
lp_mk_grmr()
{
#ifdef UMEM
	return (umem_cache_alloc(cache_grmr, UMEM_NOFAIL));
#else
	return (calloc(1, sizeof (lp_grmr_t)));
#endif

}


void
lp_rm_grmr(lp_grmr_t *g)
{
#ifdef UMEM
	bzero(g, sizeof (lp_grmr_t));
	umem_cache_free(cache_grmr, g);
#else
	bzero(g, sizeof (lp_grmr_t));
	free(g);
#endif
}

lp_ast_t *
lp_mk_ast()
{
#ifdef UMEM
	return (umem_cache_alloc(cache_ast, UMEM_NOFAIL));
#else
	return (calloc(1, sizeof (lp_ast_t)));
#endif

}

void
lp_rm_ast(lp_ast_t *r)
{
#ifdef UMEM
	bzero(r, sizeof (lp_ast_t));
	umem_cache_free(cache_ast, r);
#else
	bzero(r, sizeof (lp_ast_t));
	free(r);
#endif
}

lp_grmr_node_t *
lp_mk_grmr_node()
{
#ifdef UMEM
	return (umem_cache_alloc(cache_grmr_node, UMEM_NOFAIL));
#else
	return (calloc(1, sizeof (lp_grmr_node_t)));
#endif

}

void
lp_rm_grmr_node(lp_grmr_node_t *i)
{
#ifdef UMEM
	bzero(i, sizeof (lp_grmr_node_t));
	umem_cache_free(cache_grmr_node, i);
#else
	bzero(i, sizeof (lp_grmr_node_t));
	free(i);
#endif
}

lp_ast_node_t *
lp_mk_ast_node()
{
#ifdef UMEM
	return (umem_cache_alloc(cache_ast_node, UMEM_NOFAIL));
#else
	return (calloc(1, sizeof (lp_ast_node_t)));
#endif
}

void
lp_rm_ast_node(lp_ast_node_t *i)
{
#ifdef UMEM
	bzero(i, sizeof (lp_ast_node_t));
	umem_cache_free(cache_ast_node, i);
#else
	bzero(i, sizeof (lp_ast_node_t));
	free(i);
#endif
}

cmp_walk_step_t *
lp_mk_cmp_walk_step()
{
#ifdef UMEM
	return (umem_cache_alloc(cache_cmp_walk_step, UMEM_NOFAIL));
#else
	return (calloc(1, sizeof (cmp_walk_step_t)));
#endif
}

void
lp_rm_cmp_walk_step(cmp_walk_step_t *i)
{
#ifdef UMEM
	bzero(i, sizeof (cmp_walk_step_t));
	umem_cache_free(cache_cmp_walk_step, i);
#else
	bzero(i, sizeof (cmp_walk_step_t));
	free(i);
#endif
}

lp_mapping_t *
lp_mk_mapping()
{
#ifdef UMEM
	return (umem_cache_alloc(cache_mapping, UMEM_NOFAIL));
#else
	return (calloc(1, sizeof (lp_mapping_t)));
#endif
}

void
lp_rm_mapping(lp_mapping_t *i)
{
#ifdef UMEM
	bzero(i, sizeof (lp_mapping_t));
	umem_cache_free(cache_mapping, i);
#else
	bzero(i, sizeof (lp_mapping_t));
	free(i);
#endif
}


void *
lp_mk_buf(size_t sz)
{
#ifdef UMEM
	return (umem_alloc(sz, UMEM_NOFAIL));
#else
	return (malloc(sz));
#endif
}

void *
lp_mk_zbuf(size_t sz)
{
#ifdef UMEM
	return (umem_zalloc(sz, UMEM_NOFAIL));
#else
	return (calloc(1, sz));
#endif
}

void
lp_rm_buf(void *s, size_t sz)
{
#ifdef UMEM
	umem_free(s, sz);
#else
	free(s);
#endif
}
