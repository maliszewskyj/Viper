/*
 * Test utility for Tech80 Model 14 High-Speed encoder
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "tech8014.h"


#define  IP_CARRIER  0x6000

unsigned int ipbase = (IP_CARRIER + IP_A_IO_BASE);
int vfd = -1;
char * vdev = "/dev/vme16d16";
int eres = 0;
int verbose = 0;

void
master_reset(){
  unsigned short val;

  if (verbose) printf("MASTER RESET\n");
  val = 0x3800;
  lseek(vfd,ipbase + GLOB_MULTI,0);
  write(vfd,&val,1);
}

void
init_circ_pos() {
  unsigned short val;

  if (verbose) printf("Initialize - circ pos mode\n");

  val = 0x8108;
  lseek(vfd,ipbase + GLOB_CSR,0);
  write(vfd,&val,1);

  val = 0x0001;
  lseek(vfd,ipbase + GLOB_TSMODE,0);
  write(vfd,&val,1);

  val = 0x9000;
  lseek(vfd,ipbase + AXIS0_OFFSET + AXIS_IC,0);
  write(vfd,&val,1);

  val = 0x0524;
  lseek(vfd,ipbase + AXIS0_OFFSET + AXIS_CC,0);
  write(vfd,&val,1);
}

void
init_lin_pos() {
  unsigned short val;

  if (verbose) printf("Initialize - linear pos mode\n");

  val = 0x8108;
  lseek(vfd,ipbase + GLOB_CSR,0);
  write(vfd,&val,1);

  val = 0x0001;
  lseek(vfd,ipbase + GLOB_TSMODE,0);
  write(vfd,&val,1);

  val = 0x9000;
  lseek(vfd,ipbase + AXIS0_OFFSET + AXIS_IC,0);
  write(vfd,&val,1);

  val = 0x0504;
  lseek(vfd,ipbase + AXIS0_OFFSET + AXIS_CC,0);
  write(vfd,&val,1);
}

/*
 * Interrogate and display the registers in the ID prom
 *
 */

void
verify_ip() {
  unsigned short val;
  unsigned char ch;
  if (verbose) printf("Verifying ip\n");

  printf("   ");
  lseek(vfd,ipbase + 0x80,0);
  read(vfd,&val,1);
  ch = (unsigned char) (0x00ff & val);
  putchar(ch);

  lseek(vfd,ipbase + 0x82,0);
  read(vfd,&val,1);
  ch = (unsigned char) (0x00ff & val);
  putchar(ch);

  lseek(vfd,ipbase + 0x84,0);
  read(vfd,&val,1);
  ch = (unsigned char) (0x00ff & val);
  putchar(ch);
  
  lseek(vfd,ipbase + 0x86,0);
  read(vfd,&val,1);
  ch = (unsigned char) (0x00ff & val);
  putchar(ch);

  putchar('\n');

  lseek(vfd,ipbase + 0x88,0);
  read(vfd,&val,1);
  ch = (unsigned char) (0x00ff & val);
  printf("   MFG ID   = 0x%02x\n",ch);

  lseek(vfd,ipbase + 0x8A,0);
  read(vfd,&val,1);
  ch = (unsigned char) (0x00ff & val);
  printf("   MODEL NO = 0x%02x\n",ch);

  lseek(vfd,ipbase + 0x8C,0);
  read(vfd,&val,1);
  ch = (unsigned char) (0x00ff & val);
  printf("   REVISION = 0x%02x\n",ch);



}

void
capture_position() {
  unsigned short val;
  
  if (verbose) printf("Capture position\n");
  val = 0x0001;
  lseek(vfd,ipbase + AXIS0_OFFSET + AXIS_CSR,0);
  write(vfd,&val,1);
}

int
read_position() {
  unsigned short vals[2];
  unsigned int ival;
  unsigned char ch;

  if (verbose) printf("Reading position\n");
  lseek(vfd,ipbase + AXIS0_OFFSET + AXIS_OUT_HI,0);
  read(vfd,&vals[0],1);
  lseek(vfd,ipbase + AXIS0_OFFSET + AXIS_OUT_LO,0);
  read(vfd,&vals[1],1);  

  ival = 0xffffff & ((vals[0] << 16) + vals[1]);
  ch = (unsigned char) (vals[0] >> 8);
  

  if (verbose) {
    printf("Position = 0x%08x (%10d)\n",ival,ival);
    printf("  Status = %02x\n", ch);
  }
  return ival;
}

void
clear_counter() {
  unsigned short val;
  if (verbose) printf("Clear Counter\n");
  val = 0x000C;
  lseek(vfd,ipbase + AXIS0_OFFSET + AXIS_CSR,0);
  write(vfd,&val,1);
}

/*

tech8014

 */

int
main(int argc, char * argv[]) {
  char ch;
  /* Misc flags */
  int resetflag=0, readflag=0, clearflag = 0, initflag=-1;
  int verify = 0, captureflag=0, measurespeed=0, ncts;
  float speed;

  while ((ch = getopt(argc, argv, "RrcCbi:vVe:w")) != -1)
    switch(ch) {
    case 'R':
      resetflag = 1;
      break;
    case 'r':
      readflag = 1;
      break;
    case 'C':
      clearflag = 1;
      break;
    case 'c':
      captureflag = 1;
      break;
    case 'v':
      verbose = 1;
      break;
    case 'V':
      verify = 1;
      break;
    case 'i':
      initflag = atoi(optarg);
      if (!((initflag == 0) || (initflag == 1))) {
	fprintf(stderr,"Supported modes: circular=0, linear=1\n");
	exit(1);
      }
      break;
    case 'e':
      eres = atoi(optarg);
      if (eres <= 0) {
	fprintf(stderr,"Specify nonzero positive encoder resolution\n");
	exit(1);
      }
      break;
    case 'w':
      measurespeed = 1;
      break;
    default:
      fprintf(stderr,"Usage: tech8014 ...\n");
      exit(1);
    }
  
  if (verbose) printf("Opening device %s\n",vdev);
  if ((vfd = open(vdev,O_RDWR)) < 0) {
    perror("Open:");
    exit(1);
  }

  if (verify) {
    verify_ip();
  }

  if (resetflag) master_reset();

  if (initflag >= 0) {
    if (initflag) {
      init_lin_pos();
    } else {
      init_circ_pos();
    }
  }


  if (clearflag)   clear_counter();
  if (captureflag) capture_position();
  if (readflag)    {
    ncts = read_position();
    if (verbose) puts("Position = ");
    printf("%10d",ncts);
    if (verbose) printf("(0x%06x)",ncts);
    putchar('\n');
  }

  if (measurespeed) {
    init_lin_pos();
    clear_counter();
    sleep(10);
    capture_position();
    ncts = read_position();
    speed = 0;
    speed = (eres > 0) ? ncts/(eres * 10.0) : ncts / 10.0;
    if (verbose) {
      printf("  NCTS = %8d in 10 sec\n",ncts);
    }
    printf("Speed = %f\n",speed);
  }

  if (verbose) printf("Closing device %s\n",vdev);
  close(vfd);
  return 0;
}
