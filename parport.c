/*
 * Use parallel port as a simple digital I/O device
 *
 * Version = $Id: parport.c,v 1.1 2001/09/25 18:07:53 nickm Exp $
 */
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/io.h>
#include <sys/stat.h>

int debug = 0;
unsigned long lp0base = 0x378;

/* Set up parallel port by giving us permission to access it */
void
PARPORTSETUP() {
  if (ioperm(lp0base,2,1)) {
    fprintf(stderr,"Need root privs to read printer port\n");
    exit(1);
  }
}

/*
 * Read status value from parallel port and mangle to get bits right
 *
 * Pin 10 (Acknowledge)  -> Status Bit 6 -> Output Bit 0
 * Pin 12 (Out of Paper) -> Status Bit 5 -> Output Bit 1
 * Pin 13 (Device Sel)   -> Status Bit 4 -> Output Bit 2
 * Pin 15 (Error)        -> Status Bit 3 -> Output Bit 3
 */
void
PARPORTSTATUS(int * inval) {
  unsigned char ch;
  unsigned char outch;

  ch = inb(lp0base+1);

  outch = (((ch & 0x40) >> 6) |
	   ((ch & 0x20) >> 4) |
	   ((ch & 0x10) >> 2) |
	   (ch & 0x08));
  if (debug) {
    printf("PARPORTSTATUS: raw = 0x%02x --> conv = 0x%02x\n",ch,outch);
  }
  *inval = (int) outch;
}

int
main(int argc, char * argv[]) {
  int val;
  PARPORTSETUP();
  PARPORTSTATUS(&val);
  printf("%d\n",val);
  return 0;
}
