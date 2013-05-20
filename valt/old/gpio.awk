
NR == 1		{
			for ( x=1; x<=NF; ++x ) {
				hdr[x] = $x
				printf("%-16s ",$x)
			}
			print " "
}
NR > 1		{
			for ( x=1; x<=NF; ++x ) {
				fld[x] = $x
				if ( x <= 8 || x == 10 )
					printf("%-16s ",$x)
			}
			print " "
}
