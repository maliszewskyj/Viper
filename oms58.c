static char rcsid[] = "$Id: oms58.c,v 1.25 2008/07/15 22:11:46 nickm Exp $";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "oms58.h"
#include "vme_util.h"
int omsdebug=0;

#define RETRIES 100
//#define USEGETSPUTS 1

int OMS_flush_tx(unsigned int omsbase) {
  unsigned int addr;
  unsigned short idxput, idxget;

  if (omsdebug&1) printf("OMS_flush_tx\n");

  addr = omsbase + OUTPUT_PUT;
  if (_vme_read(VME_A16, addr, &idxput, 1, VME_D16) < 0) return -1;
  if (omsdebug&2) printf("idxput[0x%04x] = %04x\n",addr, idxput);
  idxput &= 0x00ff;/* This number must be less than the size of the buffer */
  
  idxget = idxput;

  addr = omsbase + OUTPUT_GET;
  if (_vme_write(VME_A16, addr, &idxget, 1, VME_D16) < 0) return -1;  

  return 0;

}

int OMS_flush_rx(unsigned int omsbase) {
  unsigned int addr;
  unsigned short idxput, idxget;

  if (omsdebug&1) printf("OMS_flush_rx\n");

  addr = omsbase + INPUT_PUT;
  if (_vme_read(VME_A16, addr, &idxput, 1, VME_D16) < 0) return -1;
  if (omsdebug&2) printf("idxput[0x%04x] = %04x\n",addr, idxput);
  idxput &= 0x00ff; /* This number must be less than the size of the buffer */

  idxget = idxput;

  addr = omsbase + INPUT_GET;
  if (_vme_write(VME_A16, addr, &idxget, 1, VME_D16) < 0) return -1;  

  if (omsdebug&2) printf("idxget[0x%04x] = %04x\n",addr, idxget);
  idxget &= 0x00ff; /* This number must be less than the size of the buffer */
  return 0;
}

/* 
 *
 */
int OMS_putc(unsigned int omsbase, char ch) {
  unsigned short sval, idxput, idxget;
  unsigned int addr;
  int i;
  
  addr = omsbase + OUTPUT_PUT;
  if (_vme_read(VME_A16, addr, &idxput, 1, VME_D16) < 0) return -1;
  if (omsdebug&4) printf("idxput[0x%04x] = %04x\n",addr, idxput);
  idxput &= 0x00ff;/* This number must be less than the size of the buffer */

  sval = (0x00ff & (unsigned short) ch);
  
  addr = omsbase + OUTPUT_BUF + 2 * idxput;
  if (_vme_write(VME_A16, addr, &sval, 1, VME_D16) < 0) return -1;
  if (omsdebug&2) printf("out[%04x] = %04x (%c)\n",addr,sval,
			 (isprint(ch) ? ch : ' '));

  if (++idxput > 0xff) idxput = 0;
  
  addr = omsbase + OUTPUT_PUT;
  if (_vme_write(VME_A16, addr, &idxput, 1, VME_D16) < 0) return -1;

  /* Check to make sure the character has been written */

  addr = omsbase + OUTPUT_GET;
  for (i=0;i<RETRIES;i++) {
    _vme_read(VME_A16, addr, &idxget, 1, VME_D16);
    if (omsdebug&4) printf(" idxput=%04x idxget=%04x\n",idxput,idxget);
    if (idxput == idxget) break;
  }
  return 1;
}

int
OMS_getc( unsigned int omsbase, char * ch) {
  unsigned short sval, idxget, idxput;
  unsigned int addr;

  addr = omsbase + INPUT_PUT;
  if (_vme_read(VME_A16, addr, &idxput, 1, VME_D16) < 0) return -1;

  if (omsdebug&4) printf("idxput[0x%04x] = %04x\n",addr, idxput);
  idxput &= 0x00ff; /* This number must be less than the size of the buffer */

  addr = omsbase + INPUT_GET;
  if (_vme_read(VME_A16, addr, &idxget, 1, VME_D16) < 0) return -1;

  if (omsdebug&4) printf("idxget[0x%04x] = %04x\n",addr, idxget);
  idxget &= 0x00ff; /* This number must be less than the size of the buffer */

  if (idxput == idxget) return 0; /* No characters to read */

  addr = omsbase + INPUT_BUF + 2 * idxget;
  if (_vme_read(VME_A16, addr, &sval, 1, VME_D16) < 0) return -1;

  if (++idxget > 0xff) idxget = 0;

  *ch = (unsigned char) (0x00ff & sval);
  if (omsdebug & 2) printf(" in[%04x] = %04x (%c)\n",addr,sval,
			   (isprint(*ch) ? *ch : ' '));

  /* Now update put index */
  addr = omsbase + INPUT_GET;
  if (_vme_write(VME_A16, addr, &idxget, 1, VME_D16) < 0) return -1;

  return 1;
}

int
OMS_puts( unsigned int omsbase, char * msg) {
  int len, i, retn, nwritten;
  unsigned char ch;

  if (omsdebug&1) printf("OMS_puts: %s\n", msg);

  len = strlen(msg);
  nwritten = 0;
  for(i=0;i<len;i++) {
    ch = (unsigned char) *(msg + i);
    retn = OMS_putc( omsbase, ch);
    nwritten++;
  }
  return nwritten;
}

int
OMS_gets( unsigned int omsbase, char * msg) {
  int nread;
  char ch;

  msg[0] = 0;
  nread = 0;
  while(OMS_getc( omsbase, &ch) > 0) {
    msg[nread++] = ch;
  }
  msg[nread] = 0;
  if (omsdebug&1) printf("OMS_gets: %s\n",msg);
  return nread;
}

/*
 * Flush the receive buffer of the OMS58 controller
 */
int
OMS_write_flush( unsigned int omsbase) {
  unsigned short outchar[BUFFERLEN];
  unsigned short idxget, idxput;
  char ch;
  unsigned int addr;
  int len, i;

  if (omsdebug&1) printf("OMS_write_flush:\n");

  addr = omsbase + OUTPUT_PUT;
  if (_vme_read(VME_A16, addr, &idxput, 1, VME_D16) < 0) return -1;
  if (omsdebug&2) printf("idxput[0x%04x] = %04x\n",addr, idxput);
  idxput &= 0x00ff; /* This number must be less than the size of the buffer */

  if (!idxput) return 0; /* We're already at the beginning of the buffer */

  addr = omsbase + OUTPUT_GET;
  if (_vme_read(VME_A16, addr, &idxget, 1, VME_D16) < 0) return -1;
  if (omsdebug&2) printf("idxget[0x%04x] = %04x\n",addr, idxget);
  idxget &= 0x00ff; /* This number must be less than the size of the buffer */

  len = 0xff - idxput + 1;

  ch = ' ';
  for (i=0;i<len;i++) {
    outchar[i] = (0x00ff & (unsigned short) ch);
  }

  /* Write data to VME bus */
  addr = omsbase + OUTPUT_BUF + 2 * idxput;
  if (_vme_write(VME_A16, addr, &outchar[0], len, VME_D16) < 0) return -3;

  /* Increment write pointer */
  addr = omsbase + OUTPUT_PUT;
  idxput = (idxput + len) & 0xff;
  if (_vme_write(VME_A16, addr, &idxput, 1, VME_D16) < 0) return -3;

  /* At this point, poll the idxget register to make sure the controller's
     actually read the input string */
  addr = omsbase + OUTPUT_GET;
  for (i=0;i<RETRIES;i++) {
    _vme_read(VME_A16, addr, &idxget, 1, VME_D16);
    if (omsdebug&2) {
      printf(" idxput=%04x idxget=%04x\n",idxput,idxget);
    }
    if (idxput == idxget) break;
    vsleep(10);
  }

  return len;
}

/* 

Indicate to the controller that we've already read everything it
has for us.

 */
int OMS_pointers( unsigned int omsbase, unsigned short *inputput, unsigned short *inputget, unsigned short *outputput, unsigned short *outputget )
{
  unsigned int addr;

  addr = omsbase + INPUT_PUT;
  if (_vme_read(VME_A16, addr, inputput, 1, VME_D16) < 0) return -1;

  addr = omsbase + INPUT_GET;
  if (_vme_read(VME_A16, addr, inputget, 1, VME_D16) < 0) return -2;

  addr = omsbase + OUTPUT_PUT;
  if (_vme_read(VME_A16, addr, outputput, 1, VME_D16) < 0) return -1;

  addr = omsbase + OUTPUT_GET;
  if (_vme_read(VME_A16, addr, outputget, 1, VME_D16) < 0) return -2;


  return 0;

}

/* 
 * Recipe for writing to the communication channel of the OMS58
 *
 * 1. Read output put index
 * 2. Read output get index
 *    Compare put and get to see if there is space available for writing
 *    Value of output put index is offset to start writing
 * 3. Write words to output buffer + output put index
 * 4. Increment output put index by number of words written
 *
 * TODO: buffer wraparound testing!
 */
int
OMS_write( unsigned int omsbase, char * msg) {
  unsigned short outchar[BUFFERLEN];
  unsigned short idxget, idxput;
  char ch;
  unsigned int addr;
  int len[2], nwrites, i;

  idxput = 0;
  idxget = 0;
  if (omsdebug&1) printf("OMS_write: %s\n", msg);


  /* Test the length of the message to see if we need to wrap around buffer */
  len[0] = strlen(msg);
  len[1] = 0;
  nwrites = 1;

  if (len[0] == 0) return 0;
  if (len[0] > BUFFERLEN) return -1; /* Message is too big to send */
  for (i=0;i<2;i++) {
  
    addr = omsbase + OUTPUT_PUT;
    if (_vme_read(VME_A16, addr, &idxput, 1, VME_D16) < 0) return -1;
    if (omsdebug&2) printf("idxput[0x%04x] = %04x\n",addr, idxput);
    idxput &= 0x00ff;/* This number must be less than the size of the buffer */
    
    addr = omsbase + OUTPUT_GET;
    if (_vme_read(VME_A16, addr, &idxget, 1, VME_D16) < 0) return -1;
    if (omsdebug&2) printf("idxget[0x%04x] = %04x\n",addr, idxget);
    idxget &= 0x00ff;/* This number must be less than the size of the buffer */

    if ((len[0] + idxget) < BUFFERLEN) break;
    /* Flush the buffer if we can't write it all*/
    OMS_write_flush( omsbase); 
  }


  if ((len[0] + idxget) > BUFFERLEN) {
    nwrites = 2;
    len[1] = len[0] + idxget - BUFFERLEN;
    len[0] = len[0] - len[1];
  }
  
  for (i=0;i<(len[0]+len[1]);i++) {
    outchar[i] = (0x00ff & (unsigned short) (ch = msg[i]));
  }

  if (omsdebug&2) {

    for (i=0;i<len[0];i++) {
      addr = omsbase + OUTPUT_BUF + 2 * (idxput + i);
      ch = (unsigned char) (0x00ff & outchar[i]);
      printf("out[%04x] = %04x (%c)\n",addr,outchar[i],
	     (isprint(ch) ? ch : ' '));
    }
    if (nwrites > 1) {
      for (i=0;i<len[1];i++) {
	addr = omsbase + OUTPUT_BUF + 2 * i;
	ch = (unsigned char) (0x00ff & outchar[i+len[0]]);
	printf("out[%04x] = %04x (%c)\n",addr,outchar[i+len[0]],
	       (isprint(ch) ? ch : ' '));
      }
    }
  }

  /* Write data to VME bus */
  addr = omsbase + OUTPUT_BUF + 2 * idxput;
  if (_vme_write(VME_A16, addr, &outchar[0], len[0], VME_D16) < 0) {
      fprintf(stderr,"OMS_write: could not write message\n");
      return -4;
  }

  if (nwrites > 1) {
    addr = omsbase + OUTPUT_BUF;
    if (_vme_write(VME_A16, addr, &outchar[len[0]], len[1], VME_D16) < 0) {
	fprintf(stderr,"OMS_write: could not write message\n");
	return -4;
    }
  }

  /* Increment write pointer */
  addr = omsbase + OUTPUT_PUT;
  idxput = (nwrites > 1) ? len[1] - 1 : idxput + len[0];
  if (_vme_write(VME_A16, addr, &idxput, 1, VME_D16) < 0) return -5;

  /* At this point, poll the idxget register to make sure the controller's
     actually read the input string */
  addr = omsbase + OUTPUT_GET;
  for (i=0;i<(5 * (len[0]+len[1]));i++) {
    _vme_read(VME_A16, addr, &idxget, 1, VME_D16);
    if (omsdebug&2) {
      printf(" idxput=%04x idxget=%04x\n",idxput,idxget);
    }
    if (idxput == idxget) break;
    vsleep(10);
  }

  return (len[0] + len[1]);
}

/* 
 * Recipe for reading from the communication channel of the OMS58
 *
 * 1. Read input put index
 * 2. Read input get index
 *    Difference of get and put is number of characters to be read
 *    Value of input put index is offset to start reading
 * 3. Read words from input buffer + output put index
 * 4. Write number of characters read to input get index
 *
 * TODO: buffer wraparound testing!
 */

int
OMS_read( unsigned int omsbase, char * msg) {
  unsigned short inchar[BUFFERLEN];
  unsigned short idxget, idxput;
  unsigned int addr;
  char ch;
  int len[2], nreads, i;

  if (omsdebug&2) printf("OMS_read: \n");

  addr = omsbase + INPUT_PUT;
  if (_vme_read(VME_A16, addr, &idxput, 1, VME_D16) < 0) return -1;

  if (omsdebug&2) printf("idxput[0x%04x] = %04x\n",addr, idxput);
  idxput &= 0x00ff; /* This number must be less than the size of the buffer */

  addr = omsbase + INPUT_GET;
  if (_vme_read(VME_A16, addr, &idxget, 1, VME_D16) < 0) return -1;

  if (omsdebug&2) printf("idxget[0x%04x] = %04x\n",addr, idxget);
  idxget &= 0x00ff; /* This number must be less than the size of the buffer */

  len[0] = idxput - idxget;
  len[1] = 0;
  nreads = 1;
  if (len[0] == 0) return 0;
  if (len[0] < 0) {
    /* We wrap around the ends of the buffer */
    nreads = 2;
    len[0] = BUFFERLEN - 1 - idxget;
    len[1] = idxput + 1;
  }

  /* Read data from VME bus */
  addr = omsbase + INPUT_BUF + 2 * idxget;
  if (_vme_read(VME_A16, addr, &inchar[0], len[0], VME_D16) < 0) return -3;

  if (nreads > 1) {
    addr = omsbase + INPUT_BUF;
    if (_vme_read(VME_A16, addr, &inchar[len[0]], len[1], VME_D16) < 0) 
      return -3;
  }

  /* Reconvert data from shorts to chars */
  for (i=0;i<(len[0] + len[1]); i++) {
    ch = (unsigned char) (0x00ff & inchar[i]);
    msg[i] = ch;
  }
  msg[(len[0]+len[1])] = 0;
  
  if (omsdebug&2) {
    addr = omsbase + INPUT_BUF + 2 * idxget;
    for (i=0;i<len[0];i++) {
      addr += 2 * i;
      ch = (unsigned char) (0x00ff & inchar[i]);
      printf(" in[%04x] = %04x (%c)\n",addr,inchar[i],
	     (isprint(ch) ? ch : ' '));
    }
    if (nreads > 1) {
      addr = omsbase + INPUT_BUF;
      for (i=0;i<len[1];i++) {
	addr += 2 * i;
	ch = (unsigned char) (0x00ff & inchar[i+len[0]]);
	printf(" in[%04x] = %04x (%c)\n",addr,inchar[i+len[0]],
	       (isprint(ch) ? ch : ' '));
      }
    }
  }

  /* Now tell the controller that we've finished reading */
  addr = omsbase + INPUT_GET;
  idxget = (nreads > 1) ? len[1] - 1 : idxget + len[0];
  if (_vme_write(VME_A16, addr, &idxget, 1, VME_D16) < 0) return -4;

  if (omsdebug&1) printf("OMS_read: %s\n",msg);
  return (len[0] + len[1]);
}

int
OMS_talk( unsigned int omsbase, char * msg){
  char iobuf[BUFFERLEN+1];
  int len,stat;

  memset(iobuf,0,sizeof(iobuf));
  strncpy(iobuf,msg,BUFFERLEN);

#ifdef USEGETSPUTS
  OMS_puts(omsbase,iobuf);
#else
  OMS_write(omsbase,iobuf);
#endif
  vsleep(4000); // Allow time to process command 

  /* Check for success of command */
  stat = OMS_GlobalStatus(omsbase);
  if (stat & 0x01) {
    fprintf(stderr,"Command error! Offending command \"%s\"\n",iobuf);
    fflush(stderr);
  }

  /* Now read any response */
#ifdef USEGETSPUTS
  if ((len = OMS_gets(omsbase,iobuf)) > 0) {
#else
  if ((len = OMS_read(omsbase,iobuf)) > 0) {
#endif
    strncpy(msg,iobuf,BUFFERLEN);
  }

  return len;
}

/* 
 * Request update of information in the shared memory area 
 */
int
OMS_RequestUpdate( unsigned int omsbase) {
  unsigned int addr;
  int i;
  short val;

  addr = omsbase + CONTROLREG - 1;
  val = 0x0001;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D16) < 0) return -1;

  /* Now poll the same register to make sure that bit has changed */
  for (i=0;i<1000;i++) {
    if (_vme_read(VME_A16, addr, &val, 1, VME_D16) < 0) return -3;
    if (!(val & 0x0001)) break; 
    vsleep(10);
  }

  return 0;
}

/*
 * Get all info for specified axis. 
 * Assume fd points to /dev/vmexxd32.
 */

int
OMS_AxisInfo( unsigned int omsbase, int axis, int info[]) {
  unsigned int addr;
  int i;
  unsigned short sinfo[24];

  addr = omsbase + X_OFFSET + axis * 0x80;
  if (_vme_read(VME_A16, addr, sinfo, 24, VME_D16) < 0) return -1;
  for (i=0;i<12;i++) {
    info[i] = (int)(((unsigned int) sinfo[2*i] << 16) + sinfo[2*i + 1]);
  }

  return 0;
}

/*
 * Extract same info for all axes
 */
int
OMS_AxisInquire( unsigned int omsbase, unsigned int offset, int val[]) {
  unsigned int addr;
  unsigned short sval[2];

  addr = omsbase + X_OFFSET + offset;
  if (_vme_read(VME_A16, addr, sval, 2, VME_D16) < 0) return -1;
  val[0] = (int) ((((unsigned int) sval[0]) << 16) + sval[1]);

  addr = omsbase + Y_OFFSET + offset;
  if (_vme_read(VME_A16, addr, sval, 2, VME_D16) < 0) return -1;
  val[1] = (int) ((((unsigned int) sval[0]) << 16) + sval[1]);

  addr = omsbase + Z_OFFSET + offset;
  if (_vme_read(VME_A16, addr, sval, 2, VME_D16) < 0) return -1;
  val[2] = (int) ((((unsigned int) sval[0]) << 16) + sval[1]);

  addr = omsbase + T_OFFSET + offset;
  if (_vme_read(VME_A16, addr, sval, 2, VME_D16) < 0) return -1;
  val[3] = (int) ((((unsigned int) sval[0]) << 16) + sval[1]);

  return 0;
}

/* 
 * Read position from both motor and encoder registers 
 */
int
OMS_AxisPosition( unsigned int omsbase, unsigned int axis, int *dpos, int *epos) {
  unsigned int addr;
  int offset;
  unsigned short sval[2];

  switch(axis) {
  case 0:  offset = X_OFFSET; break;
  case 1:  offset = Y_OFFSET; break;
  case 2:  offset = Z_OFFSET; break;
  case 3:  offset = T_OFFSET; break;
  default: return -1;
  }
  
  /* Read drive position */
  addr = omsbase + offset + CMD_POS; 
  if (_vme_read(VME_A16, addr, sval, 2, VME_D16) < 0) return -1;
  *dpos = (int) ((((unsigned int) sval[0]) << 16) + sval[1]);

  addr = omsbase + offset + ENC_POS; 
  if (_vme_read(VME_A16, addr, sval, 2, VME_D16) < 0) return -1;
  *epos = (int) ((((unsigned int) sval[0]) << 16) + sval[1]);

  if (omsdebug & 1) printf("OMS_AxisPosition: motor=%d encoder=%d\n",
			   *dpos,*epos);
  return 0;
}

int OMS_Done(unsigned int omsbase) {
  unsigned int addr;
  unsigned short val;

  addr = omsbase + DONEREG - 1;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D16) < 0) return -1;
  if (omsdebug & 1) printf("Donereg = 0x%04x\n",val);
  return (int) val;
}

/*
 * Determine whether any of the axes is in a limit condition
 */
int OMS_OnLimit(unsigned int omsbase) {
  unsigned int addr;
  short val;

  addr = omsbase + LIMITREG - 1;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D16) < 0) return -1;
  val &= 0x000f;
  return val;  
}

/*
 * Determine home state of axes
 */
int OMS_OnHome(unsigned int omsbase)
{
  unsigned int addr;
  short val;


  addr = omsbase + HOMEREG - 1;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D16) < 0) return -1;
  val &= 0x000f;
  return val;    
}

int
OMS_Slip( unsigned int omsbase) {
  unsigned int addr;
  unsigned short val;

  addr = omsbase + SLIPREG - 1;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D16) < 0) return -1;
  val &= 0x000f;
  if (omsdebug & 1) printf("Slipreg = 0x%04x\n",val);
  return (int) val;
}

/* Read general purpose I/O registers */
int
OMS_IO( unsigned int omsbase) {
  unsigned int addr;
  unsigned short val0, val1;

  addr = omsbase + USER1REG - 1;
  if (_vme_read(VME_A16, addr, &val0, 1, VME_D16) < 0) return -1;
  val0 &= 0x00ff;
  if (omsdebug & 1) printf("User1reg = 0x%04x\n",val0);

  addr = omsbase + USER2REG - 1;
  if (_vme_read(VME_A16, addr, &val1, 1, VME_D16) < 0) return -1;
  val1 &= 0x003f;
  if (omsdebug & 1) printf("User2reg = 0x%04x\n",val1);
  
  return (int) ((val1<<8) | val0);
}

/*
 * Emergency stop, flush command queue
 */
int
OMS_Kill( unsigned int omsbase) {
  unsigned int addr;
  short val;

  /* Write Kill message to mailbox */
  addr = omsbase + MAILBOX;
  val = 1;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D16) < 0) return -1;

  /* Signal that a message is ready by writing to control register */
  addr = omsbase + CONTROLREG - 1;
  val = 0x10;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D16) < 0) return -1;

  return 0;
}

/*
 * Read global status from status register
 *
 * Bit 0 - Command error
 * Bit 1 - Initialized
 * Bit 2 - Encoder slip
 * Bit 3 - Overtravel detected
 * Bit 4 - Done
 * Bit 5 - Direct Interrupt status request to OMS58
 * Bit 6 - Unused
 * Bit 7 - Interrupt Request Status
 *
 */

int
OMS_GlobalStatus(unsigned int omsbase) {
  unsigned int addr;
  short val;

  addr = omsbase + STATUSREG - 1;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D16) < 0) return -1;
  val &= 0x00ff;
  if (omsdebug) printf("OMS_GlobalStatus: 0x%02x\n",val);
  return val;   
}
