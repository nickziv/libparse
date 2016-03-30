/*#pragma D option flowindent*/
#pragma D option bufsize=16m
#pragma D option switchrate=600hz

dtrace:::BEGIN
{
	self->off_so_far = 0;
}

/*
parse$target:::trace_ast_begin,
parse$target:::trace_ast_end
{

}

parse$target:::trace_ast
{
	printf("[%lu] %p -> %p\n", (uint64_t)arg3, arg1, arg2);
	tracemem(copyin(arg1, 1024), 1024);
}

parse$target:::trace_ast
{
	printf(
	"\t[%lu] %s:st(%u):os(%u):oe(%u) [%p] -> %s:st(%u):os(%u):oe(%u) [%p]\n",
	(uint64_t)arg3,
	args[1]->ani_gnm, args[1]->ani_state, args[1]->ani_off_start, args[1]->ani_off_end, arg1,
	args[2]->ani_gnm, args[2]->ani_state, args[2]->ani_off_start, args[2]->ani_off_end, arg2);
}
graph$target:::rollback_change
{
	printf("ch: %p\n", arg1);
}
*/
graph$target:::change_add
{
	printf("ch: %p, s: %u, op: %d, [%lu] %p -> %p\n", arg1, arg2, arg3,
			(uint64_t)arg6, arg4, arg5);
}

pid$target::lp_add_ast_child:entry
{
	printf("%p -> %p\n", arg0, arg1);
}

/*
parse$target:::trace_ast_stack_begin,
parse$target:::trace_ast_stack_end
{

}

parse$target:::trace_ast_stack
/args[1]->ani_gnm_ptr != 0/
{
	self->stack = args[0]->asti_stack;
	printf("\t%s[%p]t(%d) nsplit: %u\n", args[1]->ani_gnm, arg1,
		args[1]->ani_type, args[0]->asti_nsplit);
}

graph$target:::bfs_deq,
graph$target:::bfs_enq,
graph$target:::bfs_visit
{
	printf("\t%p\n", arg0);
}
*/

parse$target:::ast_add_child,
parse$target:::ast_rem_child
{
	printf("\t%s [%p](%d) -> %s [%p]w(%u)\n", args[1]->ani_gnm, arg1,
		args[1]->ani_kids, args[2]->ani_gnm, arg2, args[2]->ani_index);
}

parse$target:::eval_tok
{
	printf("OP: %d MATCHED: %d\n", arg1, arg2);
}


parse$target:::ast_node_off_start
{
	printf("\t%s [%p] off = %u\n", args[1]->ani_gnm, arg1, args[1]->ani_off_start);
}

parse$target:::ast_node_off_end
{
	self->off_so_far = args[1]->ani_off_end;
	printf("\t%s [%p] off = %u\n", args[1]->ani_gnm, arg1, args[1]->ani_off_end);
}

parse$target:::ast_node_off_end
{
	@["maxoff"] = max(args[1]->ani_off_end);
}

parse$target:::rewind_begin
{

}

parse$target:::fail,
parse$target:::match
{
	printf("\t%s [%p]", args[0]->ani_gnm, arg0);
}

parse$target:::rewind_end
{
	trace(arg0);
}

parse$target:::destroy_ast_node
{
	printf("%s [%p]\n", args[0]->ani_gnm, arg0);
}


pid$target::lg_wconnect:entry
{
	printf("%x -> %x [%lu]\n", arg1, arg2, (uint64_t)arg3);
	ustack();
}

parse$target:::ast_pop
{
	printf("%s[%p]", args[0]->ani_gnm, arg0);
}

pid$target::on_pop:entry,
pid$target::on_pop:return
{
}

parse$target:::ast_push
{
	printf("%s[%p]", args[0]->ani_gnm, arg0);
}

parse$target:::nsplit_inc,
parse$target:::nsplit_dec
{
	printf("%d\n", arg0);
}

pid$target::lp_rm*:entry
{
	printf("%p\n", arg0);
}

pid$target::lp_mk*:return
{
	printf("%p\n", arg1);
}

pid$target::lg_rm*:entry
{
	printf("%p\n", arg0);
}

pid$target::lg_mk*:return
{
	printf("%p\n", arg1);
}

pid$target::astn_refc_cb:entry
{
	printf("(%x, %x, %x, %x)\n", arg0, arg1, arg2, arg3);
	ustack();
}

/*
slablist$target:::rem_begin
/arg0 == self->stack/
{
	printf("%p (elems: %u)\n", arg2, args[0]->sli_elems);
}
*/

/*
slablist$target:::add_begin
/arg1 == 0x702540/
{
	printf("%p", arg1);
	ustack();
}

slablist$target:::rem_begin
/1 || arg1 == 0x702540/
{
	printf("%p %d", arg2, arg2);
	ustack();
}

slablist$target:::rem_end
{
	printf("%p", arg0);
	ustack();
}

slablist$target:::sl_dec_elems
{
	printf("%p", arg0);
}

slablist$target:::set_end
{

	printf("%p %p", arg0, arg1);
}

slablist$target:::set_end
/arg1 == 0x702540/
{

	printf("SET_END %p %p", arg0, arg1);
}
*/

/*
pid$target:libgraph.so.1::entry,
pid$target:libgraph.so.1::return,
pid$target:libslablist.so.1::entry,
pid$target:libslablist.so.1::return,
pid$target:libparse.so.1:try_parse:entry,
pid$target:libparse.so.1:try_parse:return
{
	printf("\t%p(%d) %p(%d) %p(%d)\n", arg0, arg0,  arg1, (int)arg1, arg2, arg2);
}
*/

pid$target::lp_run_grammar:return
{
	stop();
	exit(0);
}

parse$target:::test_*
/arg0 != 0/
{
	printf("ERROR: %d %s\n", arg0, lp_e_test_descr[arg0]);
	ustack();
	exit(0);
}
