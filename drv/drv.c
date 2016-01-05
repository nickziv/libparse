/*
 * This Source Code Form is subject to the terms of the Mozilla Public License,
 * v. 2.0. If a copy of the MPL was not distributed with this file, You can
 * obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2015, Nick Zivkovic
 */

/*
 * This file implements some simple things to parse, and is used in tests.
 */

#include <parse.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <strings.h>

void
calc_tokpr8(void *b)
{
	
}

void
calc_tokpr16(void *b)
{
	
}

/*
 * This is a simple infix calculator without parenthesis.
 *
 * The grammar will look like this:
 *
 * calc:
 *	split
 *		lead end
 *		end
 *
 * end:
 *	optws num semi
 *
 * lead:
 *	repeat
 *		optws num optws op optws
 *
 * op:
 *	split
 *		op1
 *		op2
 */
lp_grmr_t *
gr_calc()
{
	lp_grmr_t *g = lp_create_grammar("calc");
	char *ws = " \t\n";
	char *cend = ";";
	char *num = "0123456789";
	/* operations of length 1 */
	char *op1 = "+-*/%><";
	/* operations of length 2 */
	char *op2 = "==!=>=<=&&";
	lp_tok_t *tws = lp_create_tok(g, "ws");
	lp_add_tok_op(tws, ROP_ANYOF_ONE_PLUS, 8, 3, ws);
	lp_tok_t *toptws = lp_create_tok(g, "optws");
	lp_add_tok_op(toptws, ROP_ANYOF_ZERO_ONE_PLUS, 8, 3, ws);
	lp_tok_t *tsemi = lp_create_tok(g, "semi");
	lp_add_tok_op(tsemi, ROP_ONE_PLUS, 8, 1, cend);
	lp_tok_t *tnum = lp_create_tok(g, "num");
	lp_add_tok_op(tnum, ROP_ANYOF_ONE_PLUS, 8, 10, num);
	lp_tok_t *top1 = lp_create_tok(g, "op1");
	lp_add_tok_op(top1, ROP_ANYOF, 8, 7, op1);
	lp_tok_t *top2 = lp_create_tok(g, "op2");
	lp_add_tok_op(top2, ROP_ANYOF, 16, 5, op2);
	int r = lp_create_grmr_node(g, "expr", NULL, SPLITTER);
	lp_root_grmr_node(g, "expr");
	r = lp_create_grmr_node(g, "expr_frag", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "end_expr", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "end", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "op", NULL, SPLITTER);
	r = lp_create_grmr_node(g, "op1", "op1", PARSER);
	r = lp_create_grmr_node(g, "op2", "op2", PARSER);
	r = lp_create_grmr_node(g, "optws", "optws", PARSER);
	r = lp_create_grmr_node(g, "num", "num", PARSER);
	r = lp_create_grmr_node(g, "semi", "semi", PARSER);
	lp_add_child(g, "expr", "expr_frag");
	lp_add_child(g, "expr", "end_expr");
	lp_add_child(g, "end_expr", "end");
	lp_add_child(g, "expr_frag", "optws");
	lp_add_child(g, "expr_frag", "num");
	lp_add_child(g, "expr_frag", "optws");
	lp_add_child(g, "expr_frag", "op");
	lp_add_child(g, "expr_frag", "expr");
	lp_add_child(g, "end", "optws");
	lp_add_child(g, "end", "num");
	lp_add_child(g, "end", "optws");
	lp_add_child(g, "end", "semi");
	lp_add_child(g, "op", "op2");
	lp_add_child(g, "op", "op1");
	return (g);
}


/*
 * We'll eventually implement clones, but until then we have to copy and paste.
 */
lp_grmr_t *
gr_calc_paren()
{
	lp_grmr_t *g = lp_create_grammar("calc");
	char *ws = " \t\n";
	char *cend = ";";
	char *num = "0123456789";
	char *opar = "(";
	char *cpar = ")";
	/* operations of length 1 */
	char *op1 = "+-*/%><";
	/* operations of length 2 */
	char *op2 = "==!=>=<=&&";
	lp_tok_t *tws = lp_create_tok(g, "ws");
	lp_add_tok_op(tws, ROP_ANYOF_ONE_PLUS, 8, 3, ws);
	lp_tok_t *toptws = lp_create_tok(g, "optws");
	lp_add_tok_op(toptws, ROP_ANYOF_ZERO_ONE_PLUS, 8, 3, ws);
	lp_tok_t *tsemi = lp_create_tok(g, "semi");
	lp_add_tok_op(tsemi, ROP_ONE, 8, 1, cend);
	lp_tok_t *tnum = lp_create_tok(g, "num");
	lp_add_tok_op(tnum, ROP_ANYOF_ONE_PLUS, 8, 10, num);
	lp_tok_t *top1 = lp_create_tok(g, "op1");
	lp_add_tok_op(top1, ROP_ANYOF, 8, 7, op1);
	lp_tok_t *top2 = lp_create_tok(g, "op2");
	lp_add_tok_op(top2, ROP_ANYOF, 16, 5, op2);
	lp_tok_t *topar = lp_create_tok(g, "opar");
	lp_add_tok_op(topar, ROP_ONE, 8, 1, opar);
	lp_tok_t *tcpar = lp_create_tok(g, "cpar");
	lp_add_tok_op(tcpar, ROP_ONE, 8, 1, cpar);

	int r = lp_create_grmr_node(g, "expr", NULL, SEQUENCER);
	lp_root_grmr_node(g, "expr");
	r = lp_create_grmr_node(g, "semi", "semi", PARSER);
	r = lp_create_grmr_node(g, "elem", NULL, SPLITTER);
	r = lp_create_grmr_node(g, "list", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "base_expr", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "base_expr_loop", NULL, SPLITTER);
	r = lp_create_grmr_node(g, "optws", "optws", PARSER);
	r = lp_create_grmr_node(g, "op", NULL, SPLITTER);
	r = lp_create_grmr_node(g, "op1", "op1", PARSER);
	r = lp_create_grmr_node(g, "op2", "op2", PARSER);
	r = lp_create_grmr_node(g, "optws", "optws", PARSER);
	r = lp_create_grmr_node(g, "num", "num", PARSER);
	r = lp_create_grmr_node(g, "number", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "opar", "opar", PARSER);
	r = lp_create_grmr_node(g, "cpar", "cpar", PARSER);


	lp_add_child(g, "number", "optws");
	lp_add_child(g, "number", "num");
	lp_add_child(g, "number", "optws");

	lp_add_child(g, "elem", "number");
	lp_add_child(g, "elem", "list");

	lp_add_child(g, "base_expr", "elem");
	lp_add_child(g, "base_expr", "op");
	lp_add_child(g, "base_expr", "base_expr_loop");

	lp_add_child(g, "base_expr_loop", "base_expr");
	lp_add_child(g, "base_expr_loop", "elem");


	lp_add_child(g, "list", "optws");
	lp_add_child(g, "list", "opar");
	lp_add_child(g, "list", "base_expr");
	lp_add_child(g, "list", "cpar");
	lp_add_child(g, "list", "optws");


	lp_add_child(g, "op", "op2");
	lp_add_child(g, "op", "op1");

	lp_add_child(g, "expr", "list");
	lp_add_child(g, "expr", "semi");
	return (g);
}

lp_grmr_t *
gr_simple_nesting()
{
	lp_grmr_t *g = lp_create_grammar("nesting");
	char *ws = " \t\n";
	char *letter = "abcdefghijklmnopqrstuvwxyz";
	char *opar = "(";
	char *cpar = ")";
	lp_tok_t *toptws = lp_create_tok(g, "optws");
	lp_add_tok_op(toptws, ROP_ANYOF_ZERO_ONE_PLUS, 8, 3, ws);
	lp_tok_t *tchar = lp_create_tok(g, "char");
	lp_add_tok_op(tchar, ROP_ANYOF, 8, 26, letter);
	lp_tok_t *topar = lp_create_tok(g, "opar");
	lp_add_tok_op(topar, ROP_ONE, 8, 1, opar);
	lp_tok_t *tcpar = lp_create_tok(g, "cpar");
	lp_add_tok_op(tcpar, ROP_ONE, 8, 1, cpar);

	int r = lp_create_grmr_node(g, "expr", NULL, SPLITTER);
	lp_root_grmr_node(g, "expr");
	r = lp_create_grmr_node(g, "char", "char", PARSER);
	r = lp_create_grmr_node(g, "letter", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "letter_rep", NULL, SEQUENCER);
	//r = lp_create_grmr_node(g, "_split_letters", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "letters", NULL, SPLITTER);
	r = lp_create_grmr_node(g, "elem", NULL, SPLITTER);
	r = lp_create_grmr_node(g, "elems", NULL, SPLITTER);
	r = lp_create_grmr_node(g, "elem_rep", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "list", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "opar", "opar", PARSER);
	r = lp_create_grmr_node(g, "cpar", "cpar", PARSER);
	r = lp_create_grmr_node(g, "optws", "optws", PARSER);



	/*
	 * A letter is a char, with optional whitespace on either side of it.
	 */
	lp_add_child(g, "letter", "optws");
	lp_add_child(g, "letter", "char");
	lp_add_child(g, "letter", "optws");

	/*
	 * An element is either a letter or a list. This is how nesting is
	 * implemented.
	 */
	lp_add_child(g, "elem", "letter");
	lp_add_child(g, "elem", "list");

	/*
	 * A repetition of elems. We either parse the next elem or the
	 * terminating elem.
	 *
	 *
	 * +------------+
	 * |            |
	 * |            |
	 * |     elem_rep
	 * |     ^
	 * v    /
	 * elems
	 *       \
	 *        v
	 *        elem
	 */
	lp_add_child(g, "elems", "elem_rep");
	lp_add_child(g, "elems", "elem");

	lp_add_child(g, "elem_rep", "elem");
	lp_add_child(g, "elem_rep", "elems");

	/*
	 * A list is an opening parem, followed by an arbitrary length sequence
	 * of elems, followed by a cpar.
	 */
	lp_add_child(g, "list", "opar");
	lp_add_child(g, "list", "elems");
	lp_add_child(g, "list", "cpar");

	/*
	 * An expression is either a list of the form:
	 *
	 * 	(elem elem elem)
	 *
	 * Or
	 *
	 * 	 elem elem elem
	 */
	lp_add_child(g, "expr", "list");
	lp_add_child(g, "expr", "elems");


	return (g);
}

//char *calc_test = "1 + 2 - 3 * 4/5+34 >= 100&&56<=0;";
//char *calc_test = "1 + 2 - 3 * 4 - 5 + 34 >= 100 && 56 <= 0;";
char *calc_test = "1 + 2 - 3;";
char *calc_paren_test = "(1 + (2 - 3));";
char *big_nesting = "(a b c d (( e f g ) h i j k)";
//char *small_nesting = "(a (b c) d)";
char *small_nesting = "(a d)";
char *nesting = "(a (b) d)";

int
main()
{
	/*
	 * 3 different grammars, an expression grammar without parens, one with
	 * parens, and a nested-list-grammar.
	 */
	lp_grmr_t *calc = gr_calc();
	size_t calc_test_sz = strlen(calc_test);
	calc_test_sz *= 8;
	lp_ast_t *calc_res = lp_create_ast();
	lp_run_grammar(calc, calc_res, calc_test, calc_test_sz);

	lp_grmr_t *calc_paren = gr_calc_paren();
	size_t calc_paren_test_sz = strlen(calc_paren_test);
	calc_paren_test_sz *= 8;
	lp_ast_t *calc_paren_res = lp_create_ast();
	lp_run_grammar(calc_paren, calc_paren_res, calc_paren_test,
	    calc_paren_test_sz);

	lp_grmr_t *gnesting = gr_simple_nesting();
	size_t nesting_sz = strlen(nesting);
	nesting_sz *= 8;
	lp_ast_t *nesting_ast = lp_create_ast();
	lp_run_grammar(gnesting, nesting_ast, nesting, nesting_sz);
}
