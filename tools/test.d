parse$target:::test_*
/arg0 != 0/
{
	printf("ERROR: %d %s\n", arg0, e_test_descr[arg0]);
	ustack();
	exit(0);
}
