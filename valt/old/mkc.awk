NR == 1		{
			for ( x=1; x<=NF; ++x ) {
				hdr[x] = $x
				printf("%-16s ",$x)
			}
			print " "
}
NR > 1		{
			printf("\t{ %s,\n\t  \"%s\",\n\t  { ",$1,$2);
			for ( x=3; x<=NF; ++x ) {
				if ( x <= 8 )
					printf("\"%s\", ",$x);
			}
			print "},"
			printf("\t  \"%s\",\n",$10);
			print "\t},"
}
END		{
			print "};"
}
