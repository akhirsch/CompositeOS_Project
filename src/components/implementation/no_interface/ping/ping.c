#include <cos_component.h>
#include <print.h>

#include <sched.h>
#include <pong.h>
 
#define ITER (1024*128)
u64_t meas[ITER];
int data[4096];
int data_uninit[4096];

void cos_init(void)
{


  /*//BEGIN TESTING CODE
	
	int cnt;
	for(cnt = 0; cnt < 4096; cnt++){
	  data_uninit[cnt] = 0;
	}

	for(cnt = 0; cnt < 4096; cnt++){
	  data[cnt] = 2;
	  data_uninit[cnt] = 3;
	}

	call();	
	printc("Starting Invocations.\n");

	//TODO: get spd of normal booter component, make sure that you only fork if you're the first component and not the second.
	//If we could boot, we could get the SPD of the first ping component. Unfortunately, we can't boot.
	if(cos_spd_id() == ADDME){
	boot_fork();
	}
	printc("Fork from spd %ld\n", cos_spd_id());
//END TESTING CODE
*/
  u64_t start, end, avg, tot = 0, dev = 0;
  int i, j;
	call();	
	printc("Starting Invocations.\n");
	
	for (i = 0 ; i < ITER ; i++) {
		rdtscll(start);
		call();
		rdtscll(end);
		meas[i] = end-start;
	}

	for (i = 0 ; i < ITER ; i++) tot += meas[i];
	avg = tot/ITER;
	printc("avg %lld\n", avg);
	for (tot = 0, i = 0, j = 0 ; i < ITER ; i++) {
		if (meas[i] < avg*2) {
			tot += meas[i];
			j++;
		}
	}
	printc("avg w/o %d outliers %lld\n", ITER-j, tot/j);

	for (i = 0 ; i < ITER ; i++) {
		u64_t diff = (meas[i] > avg) ?
			meas[i] - avg :
			avg - meas[i];
		dev += (diff*diff);
	}
	dev /= ITER;
	printc("deviation^2 = %lld\n", dev);
	
	printc("%d invocations took %lld\n", ITER, end-start);
	return;
}
