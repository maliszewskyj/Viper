#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#ifdef  MAPMEM
#define __KERNEL__
#include <asm/page.h>
#endif  
#include "ni1014fn.h"

/* 
 * Checkout test of National Instruments 1014 GPIB Controller
 * 
 */
char * vdev = "/dev/vme16d08";
#ifdef MAPMEM
#define VME_WINSIZE 65536
char * vme_mem;
char * vptr;
#endif
unsigned int gpib_addr = 0x8000;
extern int ibdebug;

int
main(int argc, char * argv[]) {
  int fd, retn;
  int verbose=0, init=0;
  char msgbuf[255];
  int lmsg;
  int addr;
  char ch;
  ibbd * gptr;

  gptr = (ibbd *) malloc(sizeof(ibbd));
  memset(gptr,0,sizeof(ibbd));
  memset(msgbuf,0,sizeof(msgbuf));
  sprintf(msgbuf,"*IDN?");
  addr = 1;
  while ((ch = getopt(argc, argv, "ib:m:d:vh?")) != -1)
    switch (ch) {
    case 'i':
      init=1;
      break;
    case 'b':
      if ((retn = sscanf(optarg,"%i",&gpib_addr)) != 1) {
	fprintf(stderr,"Specify address in decimal or hex (prefixed by 0x)\n");
	exit(1);
      }
      break;
    case 'd':
      if ((retn = sscanf(optarg,"%i",&addr)) != 1) {
	fprintf(stderr,"Specify address in decimal or hex (prefixed by 0x)\n");
	exit(1);
      }
      break;
    case 'm':
      strcpy(msgbuf,optarg);
      break;
    case 'v':
      verbose = 1;
      ibdebug = 1;
      break;
    case 'h':
    case '?':
    default: 
      fprintf(stderr,"Usage: ibio [[-b baseaddr] [-d addr] [-m message]]\n");
      exit(1);
    }

  if (verbose) printf("VME Initialization\n");

  if ((fd = open(vdev,O_RDWR)) < 0) {
    perror("Open");
    exit(1);
  }
  gptr->fd = fd;
  gptr->addr=gpib_addr;

#ifdef MAPMEM
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
  gptr->base = vme_mem + gpib_addr;
#endif

  if (init) {
    if (verbose) printf("Initializing GPIB Board\n");
    ibinit(gptr);

    if (verbose) printf("Sending Interface Clear\n");
    ibsic(gptr);
    ibren(gptr,1);
  }

  lmsg = strlen(msgbuf);
  if (verbose) printf("Writing message to address %d\n",addr);
  if ((retn = ibwrt(gptr,addr,msgbuf,lmsg)) < 0) {
    fprintf(stderr,"Error writing message\n");
    exit(1);
  }
		      
  if (verbose) printf("Receive message from address %d\n",addr);
  if ((retn = ibrd(gptr,addr,msgbuf,sizeof(msgbuf)-1)) < 0) {
    fprintf(stderr,"Error reading response\n");
    exit(1);
  }

  if (retn) printf("Received->%s<-\n",msgbuf);
  return 0;
}
