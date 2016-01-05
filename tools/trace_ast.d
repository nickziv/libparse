#pragma D option bufsize=10M
#pragma D option flowindent

/*
pid$target:libparse::entry,
pid$target:libparse::return
{

}

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
	printf("\t%s[%p] -> %s[%p]\n", args[1]->ani_gnm, arg1,  args[2]->ani_gnm, arg2);
}

graph$target:::dfs_rdnt_push,
graph$target:::dfs_rdnt_pop
{
	printf("%p\n", arg1);
}
graph$target:::dfs_rdnt_bm
{
	printf("%p -> %p\n", arg2, arg3);
}

graph$target::get_pushable_rdnt:got_here
{
	trace(arg0);
}

pid$target:libslablist.so.1::entry,
pid$target:libslablist.so.1::return,
pid$target:libparse.so.1::entry,
pid$target:libparse.so.1::return,
pid$target:libgraph.so.1::entry,
pid$target:libgraph.so.1::return
{
	printf("%d\n", (int)arg1);
}

parse$target:::match,
parse$target:::fail,
parse$target:::ast_pop,
parse$target:::ast_push
{

	printf("\t%s[%p]\n", args[0]->ani_gnm, arg0);
}

pid$target::on_pop:return
{
	trace(arg1);
}

parse$target:::trace_ast
{
	printf("\t[%lu] %s -> %s\n", (uint64_t)arg3, args[1]->ani_gnm, args[2]->ani_gnm);
}
