/*
 * excelfore-gptp - Implementation of gPTP(IEEE 802.1AS)
 * Copyright (C) 2019 Excelfore Corporation (https://excelfore.com)
 *
 * This file is part of excelfore-gptp.
 *
 * excelfore-gptp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * excelfore-gptp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with excelfore-gptp.  If not, see
 * <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
 */
#include <time.h>
#include <stdio.h>
#include <inttypes.h>
#include "gptpmasterclock.h"

int main(int argc, char *argv[])
{
	int i, runtime;
	clock_t t1;
	double rt;
	double rsum=0.0;
	double rmin=1.0;
	double rmax=0.0;

	if(argc<2) {
		runtime=1000;
	}else{
		runtime=atoi(argv[1]);
	}

	for(i=0;i<runtime;i++){
		t1=clock();
		gptpmasterclock_getts64();
		t1=clock()-t1;
		rt = ((double)t1)/CLOCKS_PER_SEC;
		if(rt<rmin) rmin=rt;
		if(rt>rmax) rmax=rt;
		rsum+=rt;
	}
	printf("rmin=%.06f, rmax=%.06f, ravg=%.06f\n", rmin, rmax, rsum/(double)runtime);
}
