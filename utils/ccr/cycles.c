	
#include <stdio.h>
#include <stdint.h>
 
static inline uint32_t ccnt_read (void)
{
	  uint32_t cc = 0;
	    __asm__ volatile ("mrc p15, 0, %0, c9, c13, 0":"=r" (cc));
	      return cc;
}
 
int main() {
	  uint32_t t0 = ccnt_read();
	    uint32_t t1 = ccnt_read();       
	      printf("%u\n", t1-t0);
	        volatile uint64_t n = 100000000;
		  while(n > 0) n--;
		    t1 = ccnt_read();
		      printf("%u\n", t1-t0);
}
