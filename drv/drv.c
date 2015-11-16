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
	int r = lp_create_grmr_node(g, "expr", NULL, SPLITTER);
	lp_root_grmr_node(g, "expr");
	r = lp_create_grmr_node(g, "expr_frag", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "expr_num", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "expr_paren", NULL, SEQUENCER);
	r = lp_create_grmr_node(g, "op", NULL, SPLITTER);
	r = lp_create_grmr_node(g, "op1", "op1", PARSER);
	r = lp_create_grmr_node(g, "op2", "op2", PARSER);
	r = lp_create_grmr_node(g, "optws", "optws", PARSER);
	r = lp_create_grmr_node(g, "num", "num", PARSER);
	r = lp_create_grmr_node(g, "opar", "opar", PARSER);
	r = lp_create_grmr_node(g, "cpar", "cpar", PARSER);

	lp_add_child(g, "expr", "expr_frag");
	lp_add_child(g, "expr", "expr_end");
	lp_add_child(g, "expr", "expr_paren");

	lp_add_child(g, "expr_frag", "optws");
	lp_add_child(g, "expr_frag", "num");
	lp_add_child(g, "expr_frag", "optws");
	lp_add_child(g, "expr_frag", "op");
	lp_add_child(g, "expr_frag", "expr");

	lp_add_child(g, "expr_num", "optws");
	lp_add_child(g, "expr_num", "num");
	lp_add_child(g, "expr_num", "optws");

	lp_add_child(g, "expr_paren", "optws");
	lp_add_child(g, "expr_paren", "opar");
	lp_add_child(g, "expr_paren", "expr");
	lp_add_child(g, "expr_paren", "cpar");
	lp_add_child(g, "expr_paren", "optws");
	lp_add_child(g, "expr_paren", "expr");

	lp_add_child(g, "op", "op2");
	lp_add_child(g, "op", "op1");
	return (g);

}

//char *calc_test1 = "1 + 2 - 3 * 4/5+34 >= 100&&56<=0;";
//char *calc_test1 = "1 + 2 - 3 * 4 - 5 + 34 >= 100 && 56 <= 0;";
char *calc_test1 = "1 + 2 - 3;";
char *calc_paren_test = "(1 + 2 - 3) * 4/5+34 >= 100&&56<=0";

int
main()
{
	lp_grmr_t *calc = gr_calc();
	lp_grmr_t *calc_paren = gr_calc_paren();
	size_t calc_test1_sz = strlen(calc_test1);
	size_t calc_paren_test_sz = strlen(calc_paren_test);
	/* convert to bits */
	calc_test1_sz *= 8;
	calc_paren_test_sz *= 8;
	//lp_ast_t *calc_res = lp_create_ast();
	//lp_run_grammar(calc, calc_res, calc_test1, calc_test1_sz);
	lp_ast_t *calc_paren_res = lp_create_ast();
	lp_run_grammar(calc_paren, calc_paren_res, calc_paren_test,
	    calc_paren_test_sz);
	/* OMG, it worked */
	/*
	lp_grmr_t *calc_paren = gr_calc_paren(calc);
	lp_result_t *calc_paren_res = lp_create_result();
	size_t calc_paren_test_sz = strlen(calc_paren_test);
	lp_run_grammar(calc_paren, calc_paren_res, calc_paren_test,
	    calc_paren_test_sz);
	*/
}
