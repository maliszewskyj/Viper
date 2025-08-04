#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#define __KERNEL__
#include <asm/page.h>
#include "ni1014rg.h"

/* 
 * Checkout test of National Instruments 1014 GPIB Controller
 * 
 */
#define VME_WINSIZE 65536
char * vdev = "/dev/vme16d16";
char * vme_mem;
char * vptr;

inline unsigned char GPIBIn(unsigned char * addr) {
  return *(addr);
}

inline void GPIBOut(unsigned char * addr, unsigned char ch) {
  *(addr) = ch;
}

int
CompVals(unsigned char exp, unsigned char recv) {
  if (recv != exp) {
    printf("\n  Expected 0x%02x received 0x%02x\n", 
	   exp, recv);
    return -1;
  }
  return 0;
}

int
main(void) {
  int fd, retn;
  int verbose=0;
  unsigned int addr;
  unsigned char ch, ich;

  addr = 0x8000;

  printf("VME Initialization\n");

  if (verbose) printf("  Opening device %s\n",vdev);
  if ((fd = open(vdev,O_RDWR)) < 0) {
    perror("Open");
    exit(1);
  }


  if (verbose) printf("  Allocating and mapping address space.\n");
  if ((vme_mem = (unsigned char *) malloc(VME_WINSIZE)) == NULL) {
    perror("Can't malloc vme_mem");
    exit(1);
  }

  /* Align the memory to a page */
  if ((vme_mem = (unsigned char *) PAGE_ALIGN((unsigned long) vme_mem))
      ==NULL){
    perror("Can't page align vme_mem");
    exit(1);
  }

  /* Map allocated memory to physical memory */
  if ((vme_mem = (unsigned char *) mmap((caddr_t) vme_mem,
					VME_WINSIZE,
					PROT_READ|PROT_WRITE,
					MAP_SHARED|MAP_FIXED,
					fd,
					0x0)) == NULL) {
    perror("Can't map physical memory.");
    exit(1);
  }    

  vptr = vme_mem + addr;

  printf("Initializing TLC:"); fflush(stdout);
  GPIBOut(vptr + AUXMR, (unsigned char) 2); /* Chip Reset */
  GPIBOut(vptr + AUXMR, (unsigned char) 0); /* Immediate execute PON */
  printf(" passed.\n");

  sleep(1);
  printf("Chip reset, compare registers to reset values:"); fflush(stdout);
  GPIBOut(vptr + AUXMR, (unsigned char) 2);
  retn = 0;
  ch = GPIBIn(vptr + ISR1);   retn += CompVals(0,ch);
  ch = GPIBIn(vptr + ISR2);   retn += CompVals(0,ch);
  ch = GPIBIn(vptr + SPSR);   retn += CompVals(0,ch);
  ch = GPIBIn(vptr + ADSR);   retn += CompVals(0x40,ch);
  ch = GPIBIn(vptr + CPTR);   retn += CompVals(0,ch);

  if (!retn) printf(" passed.\n");

  sleep(1);
  printf("Test ton, DO, ERR, CPTR, TA:"); fflush(stdout);
  retn = 0;
  GPIBOut(vptr + AUXMR, (unsigned char) 2);    /* Chip Reset */
  GPIBOut(vptr + ADMR,  (unsigned char) 0x80); /* ton */
  GPIBOut(vptr + AUXMR, (unsigned char) 0);    /* pon */
  ch = GPIBIn(vptr + ADSR);                    /* TA */
  retn += CompVals(0x42,ch);          
  ch = GPIBIn(vptr + ISR1);                    /* DO */
  retn += CompVals(2,ch);
  GPIBOut(vptr + CDOR, (unsigned char) 0x51);  /* Write date byte */
  ch = GPIBIn(vptr + CPTR);                    /* Read it back */
  retn += CompVals(0x51,ch);
  ch = GPIBIn(vptr + ISR1);                    /* DO + ERR */
  retn += CompVals(0x6,ch);
  ch = GPIBIn(vptr + ISR1);                    /* Bits cleared when read */
  retn += CompVals(0,ch);
  GPIBOut(vptr + AUXMR, (unsigned char) 2);    /* Chip Reset */
  /* GPIBOut(vptr + AUXMR, (unsigned char) 0);*/    /* pon */
  ch = GPIBIn(vptr + ADSR);                    /* Not TA */
  retn += CompVals(0x40,ch);
  if (!retn) printf(" passed\n");
  
  sleep(1);
  printf("Check lon, LA:"); fflush(stdout);
  retn = 0;
  GPIBOut(vptr + AUXMR, (unsigned char) 2);    /* Chip Reset */
  GPIBOut(vptr + IMR1,  (unsigned char) 0);    /* No interrupts */
  GPIBOut(vptr + IMR2,  (unsigned char) 0);    /* No interrupts */
  GPIBOut(vptr + ADMR,  (unsigned char) 0x40); /* lon */
  GPIBOut(vptr + AUXMR, (unsigned char) 0);    /* pon */
  ch = GPIBIn(vptr + ADSR);                    /* LA */
  retn += CompVals(0x44,ch);
  GPIBOut(vptr + AUXMR, (unsigned char) 2);    /* Chip Reset */  
  /* GPIBOut(vptr + AUXMR, (unsigned char) 0);*/    /* pon */
  ch = GPIBIn(vptr + ADSR);                    /* Not LA */
  retn += CompVals(0x40,ch);

  if (!retn) printf(" passed\n");

  sleep(1);
  printf("Test ATN, CIC, CO:"); fflush(stdout);
  retn = 0;
  GPIBOut(vptr + AUXMR, (unsigned char) 2);    /* Chip Reset */
  GPIBOut(vptr + ADMR,  (unsigned char) 0x31); /* Address mode 1 */
  GPIBOut(vptr + AUXMR, (unsigned char) 0);    /* pon */
  GPIBOut(vptr + AUXMR, (unsigned char) 0x1e); /* Set IFC */
  GPIBOut(vptr + AUXMR, (unsigned char) 0x16); /* Clear IFC */
  ch = GPIBIn(vptr + ADSR);                    /* CIC */
  retn += CompVals(0x80,ch);
  ch = GPIBIn(vptr + ISR2);                    /* CO + ADSC */
  retn += CompVals(0x09,ch);
  GPIBOut(vptr + AUXMR, (unsigned char) 0x10); /* Go to standby */
  ch = GPIBIn(vptr + ADSR);                    /* CIC + ATN* */
  retn += CompVals(0xC0,ch);
  
  if (!retn) printf(" passed\n");

  /* Cleanup */
  free(vme_mem);
  close(fd);

  return 0;
}
