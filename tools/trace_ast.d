#pragma D option bufsize=50M
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
	printf("\t%s -> %s\n", args[1]->ani_gnm, args[2]->ani_gnm);
}

parse$target:::match,
parse$target:::fail,
parse$target:::ast_pop,
parse$target:::ast_push
{

	printf("\t%s\n", args[0]->ani_gnm);
}

pid$target::lg_edges:entry
{
	printf("BEGIN TRACE\n");
}

parse$target:::trace_ast
{
	printf("\t[%lu] %s -> %s\n", (uint64_t)arg3, args[1]->ani_gnm, args[2]->ani_gnm);
}

pid$target::lg_edges:return
{
	printf("END TRACE\n");
}
