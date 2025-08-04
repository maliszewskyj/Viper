#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "vme_util.h"
#include "ni1014rg.h"
#include "ni1014fn.h"

/* Global variables */
int ibdebug=0;
int ibfd   =0;
int ibaddr =0;
char *ibptr=NULL;


unsigned char GPIBIn(unsigned int offset) {
  unsigned char ch;

  _vme_read(VME_A16, ibaddr + offset, &ch, 1, VME_D8);
  if (ibdebug) printf("GPIBIn  0x%04x = 0x%02x %c\n",
		      offset,ch,(isprint(ch))? ch:' ');

  return ch;
}

void GPIBOut(unsigned int offset, unsigned char ch) {
  if (ibdebug) printf("GPIBOut 0x%04x = 0x%02x %c\n", 
		      offset, ch,(isprint(ch))? ch:' ');
  _vme_write(VME_A16, ibaddr + offset, &ch, 1, VME_D8);
}

/* Setup global variables */
void
_ibfind(ibbd * gptr) {
  ibfd   = gptr->fd;
  ibaddr = gptr->addr;
  ibptr  = gptr->base;
}

/*
 * Initialize TLC interface to a known state
 */
int
ibinit(ibbd * gptr) {
  unsigned char ch;

  _ibfind(gptr);
  if (ibdebug) printf("ibinit\n");
  /* Initialize and enable TCL functions */
  GPIBOut(AUXMR, EIPON);

  /* Disable TCL interrupts */
  ch = 0;
  GPIBOut(IMR1, ch);
  GPIBOut(IMR2, ch);

  /* Clear status bits by reading registers */
  ch = GPIBIn(ISR1);
  ch = GPIBIn(ISR2);

  /* Set address mode, Talker/Listener inactive and properT/R signal mode */
  GPIBOut(ADMR, (unsigned char) (ADMR_MODE1 + ADMR_TRM));

  /* Set GPIB addresss (mode1 primary only), with Talker/Listener enabled */
  GPIBOut(ADR0, (unsigned char) (MA + SEL1));
  
  /* Disable secondary address recognition */
  GPIBOut(ADR0, (unsigned char) (ADR_DT1 + ADR_DL1 + SEL1));

  /* Set clock divider for 8MHz, low speed */
  GPIBOut(AUXMR, (unsigned char) (ICR + 8));
  return 0;
}

int
ibcmd(ibbd * gptr, char * cmd, int nbytes) {
  int i, tries;
  unsigned char ch;

  _ibfind(gptr);
  if (ibdebug) printf("ibcmd - begin\n");
  GPIBOut(AUXMR, TCA); /* Take control of bus at standby */

  for (i=0;i<nbytes;i++) {
    /* Check to see that CDOR is empty */
    tries = 0;
    while((ch = GPIBIn(ISR2)) & ISR2_CO) {
      tries++;
      if (tries > 10) break;
      usleep(10);
    }

    /* Write command */
    ch = (unsigned char) cmd[i];
    
    GPIBOut(CDOR, ch);

    /* If there are no listeners, return -1 */
    if ((ch = GPIBIn(ISR1)) & ISR1_ERR) return -1;
  }
  if (ibdebug) printf("ibcmd - end\n");
  return 0;
}

/*
 * Send Interface Clear (IFC)
 */
void
ibsic(ibbd * gptr) {
  _ibfind(gptr);
  if (ibdebug) printf("ibsic\n");
  GPIBOut(AUXMR, SIFC);
  usleep(100);		
  GPIBOut(AUXMR, CIFC);
}

/*
 * Set/clear remote enable signal
 */
void
ibren(ibbd * gptr, int bool) {
  _ibfind(gptr);
  if (ibdebug) printf("ibren\n");
  /* Turn on REN signal if sre is nonzero */
  if (bool) {
    GPIBOut(AUXMR, SREN);
  } else {		  
    GPIBOut(AUXMR, CREN);
  }
}

int
ibcac(ibbd * gptr, int bool) {
  char ch, cmdbuf[10];
  
  _ibfind(gptr);
  if (bool) {
    GPIBOut(AUXMR, TCA);
    return 0;
  } else {
    GPIBOut(AUXMR, TCA);
    cmdbuf[0] = (ch = UNT);
    cmdbuf[1] = (ch = UNL);
    cmdbuf[2] = (ch = TCT); /* Release control */
    return ibcmd(gptr, cmdbuf, 3);
  }
}


/*
 * ibclr
 * Clear the device with primary address dev. This function sends:
 *     TAD of the GPIB interface, 
 *     UNL, 
 *     LAD of the device,
 *     SDC  
 */
int
ibclr(ibbd * gptr, int dev) {
  char ch, cmdbuf[10];
  
  if (ibdebug) printf("ibclr\n");
  if ((dev < 0) || (dev > 31)) return -1; /* Address out of bounds */
  cmdbuf[0] = (ch = MTA(0));
  cmdbuf[1] = (ch = UNL);
  cmdbuf[2] = (ch = MLA(dev));
  cmdbuf[3] = (ch = SDC);
  return ibcmd(gptr,cmdbuf,4);
}

/*
 * ibtrg
 * Trigger the device with primary address dev. This function sends:
 *     TAD of the GPIB interface, 
 *     UNL, 
 *     LAD of the device,
 *     SDC  
 */
int
ibtrg(ibbd * gptr, int dev) {
  char ch, cmdbuf[10];
  
  if (ibdebug) printf("ibclr\n");
  if ((dev < 0) || (dev > 31)) return -1; /* Address out of bounds */
  cmdbuf[0] = (ch = MTA(0));
  cmdbuf[1] = (ch = UNL);
  cmdbuf[2] = (ch = MLA(dev));
  cmdbuf[3] = (ch = GET);
  return ibcmd(gptr,cmdbuf,4);
}

unsigned char 
ibstat(ibbd * gptr) {
  unsigned char sta;
  _ibfind(gptr);
  sta = GPIBIn(ADSR);
  return sta;
}

int
_ibrcv(ibbd * gptr, char * s, int nbytes) {
  unsigned char ch;
  int nrd;

  if (ibdebug) printf("_ibrcv - begin\n");
  GPIBOut(AUXMR, FH); /* Release any handshake holdoff in prog */

  /* Make sure we're the controller in charge */
  if ((ch = GPIBIn(ADSR)) & 0x80) { /* CIC */
    /* Set HLDE and BIN in AUXRA */
    GPIBOut(AUXMR, (unsigned char) (0x92)); 
  } else {
    /* Clear any HLDE or HLDA in effect */
    GPIBOut(AUXMR, (unsigned char) AUXRA);
  }

  /* Check to make sure we've actually got something to read */
  /*
  nrd=0;
  while(!((ch = GPIBIn(ISR1)) & (ISR1_DI|ISR1_ENDRX))) { 
    usleep(1);
    nrd++;
    if (nrd > IB_USEC) return -IB_TIMEOUT;
  }
  */
  nrd=0;
  do {
    int ntries=0;
    while(!((ch = GPIBIn(ISR1)) & (ISR1_DI|ISR1_ENDRX))) {
      usleep(1);
      if ((ntries++)>100) return -IB_TIMEOUT;
    }
    if (ch & ISR1_ENDRX) break;
    ch = GPIBIn(DIR);
    s[nrd++] = ch;
  } while(nrd <= nbytes);

  /* Send HLDA */
  GPIBOut(AUXMR, AUXRA + 1);

  if (ibdebug) printf("_ibrcv - received %d bytes\n",nrd);
  return nrd;
}

/* 
 * Read bytes from device at address addr,
 * nbytes is the maximum number of bytes to read
 * return number of bytes read
 *
 */
int
ibrd(ibbd * gptr, int addr, char * s, int nbytes) {
  int retn;
  int tout;
  char cmdbuf[10];
  unsigned char ch;

  _ibfind(gptr);
  if (ibdebug) printf("ibrd - begin\n");
  if ((addr < 0) || (addr > 31)) return -1; /* Address out of bounds */
  cmdbuf[0] = (ch = UNT);
  cmdbuf[1] = (ch = UNL);
  cmdbuf[2] = (ch = MTA(addr));
  ibcmd(gptr,cmdbuf,3);
  
  GPIBOut(AUXMR, LTN);
  GPIBOut(AUXMR, GTS); /* Program NI1014 to be the listener */

  /* Actually read the data */
  if ((retn = _ibrcv(gptr,s,nbytes)) < 0) {
    if (ibdebug) printf("ibrd - read failure\n");
    return retn;
  }

  GPIBOut(AUXMR, TCS); /* Take control again */

  /* Wait for ATN */
  tout = 0;
  while(!((ch = GPIBIn(ADSR)) & NATN)) {
    usleep(1); 
    if ((tout++) >= 20) break;
  }

  /* Unaddress talkers and listeners */
  ibcmd(gptr,cmdbuf,2);
  GPIBOut(AUXMR, LUN); /* Local unlisten */

  if (ibdebug) printf("ibrd - end\n");
  return retn;
}

/*
 * Send data 
 */ 
int
_ibdsend(ibbd * gptr, char * s, int nbytes) {
  int nwr, seoi;
  unsigned char ch;

  if (nbytes <= 0) return 0;

  /* Wait for CDOR or ERR */
  seoi=1;
  nwr=0;
  do {
    while(!((ch = GPIBIn(ISR1)) & (ISR1_DO | ISR1_ERR))) usleep(1);
    if (ch & ISR1_ERR) return -1; /* Error encountered */
    ch = s[nwr++];
    if ((nwr == nbytes) && seoi) GPIBOut(AUXMR, SEOI);
    GPIBOut(CDOR, ch);     /* Write data byte */
  } while(nwr < nbytes);

  /* GPIBOut(IMR2, DMA0); */ /* Not sure we even need to do this */

  return nwr;
}


int
ibwrt(ibbd * gptr, int addr, char * s, int nbytes) {
  unsigned char ch;
  int retn, nwr;
  char cmdbuf[10];

  _ibfind(gptr);
  if (ibdebug) printf("ibwrt - begin (%d bytes)\n",nbytes);
  if ((addr < 0) || (addr > 31)) return -1; /* Address out of bounds */
  
  cmdbuf[0] = (ch = UNT);       /* Untalk   */
  cmdbuf[1] = (ch = UNL);       /* Unlisten */
  cmdbuf[2] = (ch = MTA(0));    /* Set talk address */
  cmdbuf[3] = (ch = MLA(addr)); /* Set listen address */
  retn = ibcmd(gptr,cmdbuf,4);

  GPIBOut(AUXMR, GTS);   /* Go to standby */

  nwr = _ibdsend(gptr, s, nbytes);
  

  /* Wait until last byte has been sent */
  while((ch = GPIBIn(ISR1)) & ISR1_DO) usleep(1);
  
  GPIBOut(AUXMR, TCA);    /* Take control */

  /* Unaddress talkers and listeners */
  retn = ibcmd(gptr, cmdbuf,2);
  return nwr;
}
