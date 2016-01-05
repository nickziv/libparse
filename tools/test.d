#pragma D option flowindent

parse$target:::trace_ast_begin,
parse$target:::trace_ast_end
{

}

parse$target:::trace_ast
{
	printf(
	"\t[%lu] %s:st(%u):os(%u):oe(%u) [%p] -> %s:st(%u):os(%u):oe(%u) [%p]\n",
	(uint64_t)arg3,
	args[1]->ani_gnm, args[1]->ani_state, args[1]->ani_off_start, args[1]->ani_off_end, arg1,
	args[2]->ani_gnm, args[2]->ani_state, args[2]->ani_off_start, args[2]->ani_off_end, arg2);
}

parse$target::lp_test_ast_node:got_here
{
	printf("%p", arg0);
}

parse$target::lp_set_ast_offsets:got_here
{
	printf("%u", arg0);
}

parse$target:::ast_add_child,
parse$target:::ast_rem_child
{
	printf("\t%s [%p](%d) -> %s [%p]w(%u)\n", args[1]->ani_gnm, arg1,
		args[1]->ani_kids, args[2]->ani_gnm, arg2, args[2]->ani_index);
}

parse$target:::ast_node_off_start
{
	printf("\t%s [%p] off = %u\n", args[1]->ani_gnm, arg1, args[1]->ani_off_start);
}

parse$target:::ast_node_off_end
{
	printf("\t%s [%p] off = %u\n", args[1]->ani_gnm, arg1, args[1]->ani_off_end);
}

parse$target:::rewind_begin,
parse$target:::rem_subtree_begin,
parse$target:::rem_subtree_end
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
parse$target::ast_rewind:got_here
{
	trace(arg0);
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

/*
pid$target:libparse.so.1::entry,
pid$target:libparse.so.1::return,
pid$target:libgraph.so.1::entry,
pid$target:libgraph.so.1::return,
pid$target:libslablist.so.1::entry,
pid$target:libslablist.so.1::return,
parse$target:::got_here
{
	printf("\t%p(%d) %p(%d) %p(%d)\n", arg0, arg0,  arg1, arg1, arg2, arg2);
}
*/

parse$target::lp_rem_ast_child:got_here
{
	printf("%u\n", arg0);
}

parse$target:::test_*
/arg0 != 0/
{
	printf("ERROR: %d %s\n", arg0, e_test_descr[arg0]);
	ustack();
	exit(0);
}
