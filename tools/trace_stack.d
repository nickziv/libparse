/*#pragma D option bufsize=M*/
#pragma D option flowindent

dtrace:::BEGIN
{
}

pid$target:libparse::entry,
pid$target:libgraph::entry,
pid$target:libparse::return,
pid$target:libgraph::return
/probefunc != "trace_ast"/
{
	printf("(%x, %x, %x, %x)\n", arg0, arg1, arg2, arg3);
}

pid$target::has_one:entry
{
	self->follow = 1;
}

pid$target::bcmp:entry
/self->follow/
{
	tracemem(copyin(arg0, 32), 32, arg2);
	tracemem(copyin(arg1, 32), 32, arg2);
	trace(arg2);
}

pid$target::has_one:return
{
	self->follow = 0;
}

/*
parse$target::try_parse:got_here
{
	trace(arg0);
}

pid$target:libgraph:pop:return
{
	printf("%p\n", arg1);
}
graph$target:::dfs_rdnt_push,
graph$target:::dfs_rdnt_pop
{
	printf("%p\n", arg1);
}

parse$target:::parse
{}
*/
parse$target:::ast_add_child,
parse$target:::ast_rem_child
{
	printf("\t%s[%x] -> %s[%x]\n", args[1]->ani_gnm, arg1,
			args[2]->ani_gnm, arg2);
}

parse$target:::match,
parse$target:::fail,
parse$target:::ast_pop,
parse$target:::ast_push
{

	printf("\t%s[%x]\n", args[0]->ani_gnm, arg0);
}

parse$target:::rewind_begin,
parse$target:::rewind_end
{

}

parse$target:::got_here
{
	trace(arg0);
}

parse$target:::got_here
/arg0 > 40000/
{
	trace(copyinstr(arg0));
}

graph$target:::dfs_rdnt_pop
{
	self->astnode = xlate<gn_info_t>((lp_grmr_node_t *)arg1).gni_name;
	printf("%s[%x]\n", self->astnode, arg1);
}

parse$target:::test_*
/arg0 != 0/
{
	printf("ERROR: %d %s\n", arg0, e_test_descr[arg0]);
	ustack();
	exit(0);
}

parse$target:::run_grmr_begin,
parse$target:::run_grmr_end
{

}

parse$target:::trace_ast
{
	printf("\t[%lu] %s -> %s\n", (uint64_t)arg3, args[1]->ani_gnm, args[2]->ani_gnm);
}


/*
parse$target:::destroy_integral
{
	printf("%s\n", args[0]->ili_gnm);
}
*/

