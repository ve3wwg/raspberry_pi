BEGIN	{
		sum0 = sum1 = 0
		min0 = 99999; max0 = 0;
		min1 = 99999; max1 = 0;
		n    = 0;
}
	{
		if ( NR > 4 ) {
			++n;
			sum0 += $3
			sum1 += $4

			if ( $3 < min0 ) min0 = $3
			if ( $4 < min1 ) min1 = $4
		
			if ( $3 > max0 ) max0 = $3
			if ( $4 > max1 ) max1 = $4
		}
	}			

END	{
		printf("%d samples:\n",n);
		printf("0 min %d max %d avg %f\n",min0,max0,sum0/n)
		printf("1 min %d max %d avg %f\n",min1,max1,sum1/n)
	}
