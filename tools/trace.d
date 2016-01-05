parse$target:::ast_add_child,
parse$target:::ast_rem_child
/args[1]->ani_gnm == "letters"/
{
	printf("\t%s [%p](%d) -> %s [%p]\n", args[1]->ani_gnm, arg1,
			args[1]->ani_kids, args[2]->ani_gnm, arg2);
	ustack();
}

parse$target::ast_rewind:got_here
{
	printf("%u\n", arg0);
}

parse$target:::ast_pop,
parse$target:::rem_subtree_root,
parse$target:::ast_push
{
	printf("\t%s [%p]\n", args[0]->ani_gnm, arg0);
}


parse$target:::rewind_begin
{
	ustack();
}

parse$target:::ast_node_off_start
{
	printf("\t%s [%p] (%u)\n", args[1]->ani_gnm, arg1,
			args[1]->ani_off_start);
}

parse$target:::ast_node_off_end
{
	printf("\t%s [%p] (%u)\n", args[1]->ani_gnm, arg1,
			args[1]->ani_off_end);
}

parse$target::lp_set_ast_offsets:got_here
{
	trace(arg0);
}

parse$target:::split,
parse$target:::parse,
parse$target:::rem_subtree_begin,
parse$target:::rem_subtree_end
{

}

parse$target:::match,
parse$target:::fail
{
	printf("\t%s [%p]", args[0]->ani_gnm, arg0);

}

parse$target:::trace_ast_begin,
parse$target:::trace_ast_end
{

}

parse$target:::trace_ast
{
	printf("\t[%lu] %s [%p] -> %s [%p]\n", (uint64_t)arg3,
		args[1]->ani_gnm, arg1,  args[2]->ani_gnm, arg2);
}
