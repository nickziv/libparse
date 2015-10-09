parse$target:::create_res,
parse$target:::clone_grmr,
parse$target:::create_grmr,
parse$target:::run_grmr_begin,
parse$target:::run_grmr_end
{

}

/*
parse$target:::destroy_integral
{
	printf("%s\n", args[0]->ili_gnm);
}
*/

parse$target:::test_*
/arg0 != 0/
{
	printf("ERROR: %d %s\n", arg0, e_test_descr[arg0]);
	ustack();
	exit(0);
}
