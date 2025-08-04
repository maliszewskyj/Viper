static char rcsid[] = "$Id: oms_maxv.c,v 1.13 2016/02/08 13:32:46 nickm Exp $";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "oms_maxv.h"
#include "vme_util.h"

#define RETRIES 100
int maxvdebug=0;

int MAXv_flush_tx(unsigned int maxvbase) {
  unsigned int addr;
  unsigned int idxput, idxget;

  if (maxvdebug&2) printf("MAXv_flush_tx\n");

  addr = maxvbase + COMMAND_INSERT;
  if (_vme_read(VME_A16, addr, &idxput, 1, VME_D32) < 0) return -1;
  if (maxvdebug&8) printf("idxput[0x%04x] = %04x\n",addr, idxput);
  idxput &= 0x03ff;/* This number must be less than the size of the buffer */
  
  idxget = idxput;

  addr = maxvbase + COMMAND_PROC;
  if (_vme_write(VME_A16, addr, &idxget, 1, VME_D32) < 0) return -1;  

  return 0;

}

int MAXv_flush_rx(unsigned int maxvbase) {
  unsigned int addr;
  unsigned int idxput, idxget;

  if (maxvdebug&2) printf("MAXv_flush_rx\n");

  addr = maxvbase + RESPONSE_INSERT;
  if (_vme_read(VME_A16, addr, &idxput, 1, VME_D32) < 0) return -1;
  if (maxvdebug&4) printf("idxput[0x%04x] = %04x\n",addr, idxput);
  idxput &= 0x03ff; /* This number must be less than the size of the buffer */

  idxget = idxput;

  addr = maxvbase + RESPONSE_PROC;
  if (_vme_write(VME_A16, addr, &idxget, 1, VME_D32) < 0) return -1;  

  if (maxvdebug&4) printf("idxget[0x%04x] = %04x\n",addr, idxget);
  idxget &= 0x03ff; /* This number must be less than the size of the buffer */
  return 0;
}

int MAXv_putc(unsigned int maxvbase, char ch) 
{
  unsigned int idx_insert, idx_process, idx_temp;
  unsigned int addr;
  //unsigned char cval;
  unsigned short sval;
  
  addr = maxvbase + COMMAND_INSERT;
  if (_vme_read(VME_A16, addr, &idx_insert, 1, VME_D32) < 0) return -1;
  if (maxvdebug&4) printf("idx_ins [0x%04x] = %04x\n",addr, idx_insert);

  addr = maxvbase + COMMAND_PROC;
  if (_vme_read(VME_A16, addr, &idx_process, 1, VME_D32) < 0) return -2;
  if (maxvdebug&4) printf("idx_proc[0x%04x] = %04x\n",addr, idx_process);

  idx_temp = idx_insert;
  if (++idx_temp >= COMMAND_BUF_SIZE) idx_temp = 0;

  if (idx_temp != idx_process) {
    addr = maxvbase + COMMAND_BUF + idx_insert;
    //cval = (unsigned char) ch;
    sval = (0x00ff & (unsigned short) ch);
    if (_vme_write(VME_A16, addr, &sval, 1, VME_D16) < 0) return -3;
    if (maxvdebug&4) printf("out[%04x] = %04x (%c)\n",addr,sval,
			    (isprint(ch) ? ch : ' '));

    addr = maxvbase + COMMAND_INSERT;
    if (_vme_write(VME_A16, addr, &idx_temp, 1, VME_D32) < 0) return -4;
    return 1; // A single character was sent
  }

  return 0;  // No characters were sent
}

int MAXv_getc( unsigned int maxvbase, char * ch) 
{
  unsigned idx_insert, idx_process;
  unsigned int addr;
  //unsigned char cval;
  unsigned short sval;

  addr = maxvbase + RESPONSE_INSERT;
  if (_vme_read(VME_A16, addr, &idx_insert, 1, VME_D32) < 0) return -1;
  if (maxvdebug&4) printf("idx_ins [0x%04x] = %04x\n",addr, idx_insert);

  addr = maxvbase + RESPONSE_PROC;
  if (_vme_read(VME_A16, addr, &idx_process, 1, VME_D32) < 0) return -1;
  if (maxvdebug&4) printf("idx_proc[0x%04x] = %04x\n",addr, idx_process);

  if (idx_insert == idx_process) {
    *ch = 0;
    return 0; /* No characters to read */
  }

  addr = maxvbase + RESPONSE_BUF + idx_process;
  if (_vme_read(VME_A16, addr, &sval, 1, VME_D16) < 0) return -1;
  *ch = (char) (0x00ff & sval);
  if ((idx_process++) >= RESPONSE_BUF_SIZE) idx_process = 0;

  // Update process register
  addr = maxvbase + RESPONSE_PROC;
  if (_vme_write(VME_A16, addr, &idx_process, 1, VME_D32) < 0) return -1;  
  return 1;
}

int MAXv_puts( unsigned int maxvbase, char * msg) 
{
  int len, i, retn, nwritten;
  unsigned char ch;

  if (maxvdebug&1) printf("MAXv_puts: %s\n", msg);

  len = strlen(msg);
  nwritten = 0;
  for(i=0;i<len;i++) {
    ch = (unsigned char) *(msg + i);
    retn = MAXv_putc( maxvbase, ch);
    nwritten++;
  }
  // Add terminating null character
  return nwritten;
}

int MAXv_gets( unsigned int maxvbase, char * msg) 
{
  int nread;
  char ch;

  msg[0] = 0;
  nread = 0;
  while(MAXv_getc( maxvbase, &ch) > 0) {
    msg[nread++] = ch;
  }
  msg[nread] = 0;
  if (maxvdebug&1) printf("MAXv_gets: %s\n",msg);
  return nread;
}

int MAXv_getpointers( unsigned int maxvbase, unsigned int *response_insert, unsigned int *response_process, unsigned int *command_insert, unsigned int *command_process )
{
  unsigned int addr;

  addr = maxvbase + RESPONSE_INSERT;
  if (_vme_read(VME_A16, addr, response_insert, 1, VME_D32) < 0) return -1;

  addr = maxvbase + RESPONSE_PROC;
  if (_vme_read(VME_A16, addr, response_process, 1, VME_D32) < 0) return -2;

  addr = maxvbase + COMMAND_INSERT;
  if (_vme_read(VME_A16, addr, command_insert, 1, VME_D32) < 0) return -1;

  addr = maxvbase + COMMAND_PROC;
  if (_vme_read(VME_A16, addr, command_process, 1, VME_D32) < 0) return -2;

  return 0;

}

int MAXv_SendCommandByte(unsigned int maxvbase, char * pCommand)
{						
  unsigned int InsertIdx;
  unsigned int ProcessIdx;
  unsigned int addr;
  int FreeBytes;
  int CharsSent;
  int CmdLength;
  char * Command,ch;

  addr = maxvbase + COMMAND_INSERT;
  if (_vme_read(VME_A16, addr, &InsertIdx,  1, VME_D32) < 0) return -1;
  if (maxvdebug&2) printf("InsertIdx  [0x%04x] = %04x\n",addr, InsertIdx);
  
  addr = maxvbase + COMMAND_PROC;
  if (_vme_read(VME_A16, addr, &ProcessIdx, 1, VME_D32) < 0) return -1;
  if (maxvdebug&2) printf("ProcessIdx [0x%04x] = %04x\n",addr, ProcessIdx);

  CharsSent = 0;
  Command = pCommand;
  CmdLength = strlen(pCommand);
  if (CmdLength > 0) {
    if (InsertIdx == ProcessIdx) {
      FreeBytes = COMMAND_BUF_SIZE - 1;
    } else if (InsertIdx > ProcessIdx) {
      FreeBytes = COMMAND_BUF_SIZE - (InsertIdx - ProcessIdx) - 1;
    } else {
      FreeBytes = (ProcessIdx - InsertIdx) - 1;
    }

    if (FreeBytes >= CmdLength) {
      while (*Command != (char) 0) {
	addr = maxvbase + COMMAND_BUF + InsertIdx;
	ch = *Command++;
	if (_vme_write(VME_A16, addr, &ch, 1, VME_D8) < 0) {
	  fprintf(stderr,"MAXv_SendCommand: could not write character\n");
	  return -2;
	}
	CharsSent++;
	if (++InsertIdx >= COMMAND_BUF_SIZE) InsertIdx = 0;
      }

      /* Store updated insert pointer */
      addr = maxvbase + COMMAND_INSERT;
      if (_vme_write(VME_A16, addr, &InsertIdx, 1, VME_D32) < 0) return -1;
    }
  }
  return CharsSent;
}

// Write string to MAXv command buffer as packed short integers
// Pad strings with SPACE character
int MAXv_SendCommandShort(unsigned int maxvbase, char * pCommand)
{						
  unsigned int InsertIdx;
  unsigned int ProcessIdx;
  unsigned int addr;
  int FreeBytes;
  int CharsSent;
  int CmdLength;
  char * Command,ch;
  unsigned short sval;

  addr = maxvbase + COMMAND_INSERT;
  if (_vme_read(VME_A16, addr, &InsertIdx,  1, VME_D32) < 0) return -1;
  if (maxvdebug&4) printf("InsertIdx  [0x%04x] = %04x\n",addr, InsertIdx);
  
  addr = maxvbase + COMMAND_PROC;
  if (_vme_read(VME_A16, addr, &ProcessIdx, 1, VME_D32) < 0) return -1;
  if (maxvdebug&4) printf("ProcessIdx [0x%04x] = %04x\n",addr, ProcessIdx);

  CharsSent = 0;

  Command = pCommand;
  CmdLength = strlen(pCommand);
  if (strlen(pCommand) % 2) CmdLength++;

  if (CmdLength > 0) {
    if (InsertIdx == ProcessIdx) {
      FreeBytes = COMMAND_BUF_SIZE - 1;
    } else if (InsertIdx > ProcessIdx) {
      FreeBytes = COMMAND_BUF_SIZE - (InsertIdx - ProcessIdx) - 1;
    } else {
      FreeBytes = (ProcessIdx - InsertIdx) - 1;
    }

    if (FreeBytes >= CmdLength) {
      if (strlen(pCommand) % 2) {
	addr = maxvbase + COMMAND_BUF + InsertIdx;
	ch = *Command++;
	sval = (' ' << 8) + ch;
	if (_vme_write(VME_A16, addr, &sval, 1, VME_D16) < 0) {
	  fprintf(stderr,"MAXv_SendCommand: could not write character\n");
	  return -2;
	}
	InsertIdx+=2;
	CharsSent++;
	if (InsertIdx >= COMMAND_BUF_SIZE) InsertIdx = 0;
      }
      while (*Command != (char) 0) {
	addr = maxvbase + COMMAND_BUF + InsertIdx;
	ch = *Command++;
	sval = (((short)ch) << 8);
	ch = *Command++;
	sval += ch;
	if (_vme_write(VME_A16, addr, &sval, 1, VME_D16) < 0) {
	  fprintf(stderr,"MAXv_SendCommand: could not write character\n");
	  return -2;
	}
	CharsSent+=2;
	InsertIdx+=2;
	if (InsertIdx >= COMMAND_BUF_SIZE) InsertIdx = 0;
      }

      /* Store updated insert pointer */
      addr = maxvbase + COMMAND_INSERT;
      if (_vme_write(VME_A16, addr, &InsertIdx, 1, VME_D32) < 0) return -1;
    }
  }
  return CharsSent;
}

// Write string to MAXv command buffer as packed 32 bit integers
// Pad strings with SPACE character
int MAXv_SendCommandLong(unsigned int maxvbase, char * pCommand)
{						
  unsigned int InsertIdx;
  unsigned int ProcessIdx;
  unsigned int addr;
  int FreeBytes;
  int CharsSent;
  int CmdLength;
  char * Command,ch;
  unsigned int ival;

  addr = maxvbase + COMMAND_INSERT;
  if (_vme_read(VME_A16, addr, &InsertIdx,  1, VME_D32) < 0) return -1;
  if (maxvdebug&4) printf("InsertIdx  [0x%04x] = %04x\n",addr, InsertIdx);
  
  addr = maxvbase + COMMAND_PROC;
  if (_vme_read(VME_A16, addr, &ProcessIdx, 1, VME_D32) < 0) return -1;
  if (maxvdebug&4) printf("ProcessIdx [0x%04x] = %04x\n",addr, ProcessIdx);

  CharsSent = 0;

  Command = pCommand;
  CmdLength = strlen(pCommand);
  switch((strlen(pCommand) % 4)) {
  case 3: CmdLength+=3;
  case 2: CmdLength+=2;
  case 1: CmdLength+=1;
  }

  if (CmdLength > 0) {
    if (InsertIdx == ProcessIdx) {
      FreeBytes = COMMAND_BUF_SIZE - 1;
    } else if (InsertIdx > ProcessIdx) {
      FreeBytes = COMMAND_BUF_SIZE - (InsertIdx - ProcessIdx) - 1;
    } else {
      FreeBytes = (ProcessIdx - InsertIdx) - 1;
    }

    if (FreeBytes >= CmdLength) {
      addr = maxvbase + COMMAND_BUF + InsertIdx;
      if (strlen(pCommand) % 4) {
	  switch((strlen(pCommand) % 4)) {
	  case 3:
	    ival = (' '<<24) + (' '<<16) + (' '<<8);
	    ch = *Command++;
	    CharsSent++;
	    break;
	  case 2:
	    ival = (' '<<24) + (' '<<16);
	    ch = *Command++;
	    ival += (((int)ch) << 8);
	    ch = *Command++;
	    ival += ch;
	    CharsSent+=2;
	    break;
	  case 1:
	    ival = (' '<<24);
	    ch = *Command++;
	    ival += (((int)ch) << 16);
	    ch = *Command++;
	    ival += (((int)ch) << 8);
	    ch = *Command++;
	    ival += ch;
	    CharsSent+=3;
	  }
      	if (_vme_write(VME_A16, addr, &ival, 1, VME_D32) < 0) {
	  fprintf(stderr,"MAXv_SendCommand: could not write character\n");
	  return -2;
	}
	InsertIdx+=4;
      }
      while (*Command != (char) 0) {
	addr = maxvbase + COMMAND_BUF + InsertIdx;
	ch = *Command++;
	ival = ((int) ch) << 24;
	ch = *Command++;
	ival += ((int) ch) << 16;
	ch = *Command++;
	ival += ((int) ch) << 8;
	ch = *Command++;
	ival += ch;
	if (_vme_write(VME_A16, addr, &ival, 1, VME_D32) < 0) {
	  fprintf(stderr,"MAXv_SendCommand: could not write character\n");
	  return -2;
	}
	CharsSent+=4;
	InsertIdx+=4;
	if (InsertIdx >= COMMAND_BUF_SIZE) InsertIdx = 0;
      }

      /* Store updated insert pointer */
      addr = maxvbase + COMMAND_INSERT;
      if (_vme_write(VME_A16, addr, &InsertIdx, 1, VME_D32) < 0) return -1;
    }
  }
  return CharsSent;
}

int MAXv_SendCommand(unsigned int maxvbase, char *pCommand)
{
  if (maxvdebug&2) printf("MAXv_SendCommand: %s\n",pCommand);
  // return MAXv_SendCommandByte(maxvbase,pCommand);
  // return MAXv_SendCommandLong(maxvbase,pCommand);
  return MAXv_SendCommandShort(maxvbase,pCommand);
}

int MAXv_SendAndGetString(unsigned int maxvbase, char * pCommand, char * pResponse)
{
  unsigned int addr;
  int Length,ResponseLength;
  int TimeOut, CharsSent;
  unsigned int ProcessIdx, msgsem, status;
  short ResponseShort;
  char * Response, ResponseChar;

  Response = pResponse;
  *Response = (char) 0; /* If response fails, return null string */
  ResponseLength = 0;

  /* Clear message semaphore */
  addr = maxvbase + MAXV_MSG_SEM;
  msgsem = 0;
  if (_vme_write(VME_A16, addr, &msgsem, 1, VME_D32) < 0) return -1;  
  if (maxvdebug&1) printf("MAXv_SendAndGetString[send]: %s\n",pCommand);  
  CharsSent = MAXv_SendCommand(maxvbase,pCommand);
  /* If the command string was sent successfully */
  if (CharsSent == strlen(pCommand)) {
    vsleep(50);
    TimeOut = RESPONSE_RETRY_LIM;
    do {
      status=0;
      addr = maxvbase + MAXV_STAT1_F;
      if (_vme_read(VME_A16, addr, &status, 1, VME_D32) < 0) return -2;
      if (status & MAXV_COMMAND_ERROR) {
	printf("MAXv Command Error: offending command =\"%s\"\n",pCommand);

	// Reset the error
	status = MAXV_COMMAND_ERROR;
	if (_vme_write(VME_A16, addr, &status, 1, VME_D32) < 0) return -1;  
	return 0;
      }
      vsleep(50);
    } while((--TimeOut > 0) && ((status & MAXV_RESPONSE_AVAILABLE) == 0));
    if (TimeOut > 0) {
      /* Check message semaphore */
      addr = maxvbase + MAXV_MSG_SEM;
      if (_vme_read(VME_A16, addr, &msgsem, 1, VME_D32) < 0) return -3; 

      // ASCII_RESPONSE_CODE NOT DEFINED
      if ((msgsem & 0xff) != 0) {
	ProcessIdx = (msgsem >> 8) & 0xFFF;
	Length = (msgsem >> 20) & 0xFFF;
	if (maxvdebug&4) printf("ProcessIdx [0x%04x] = %04x\n",addr, ProcessIdx);
	if (maxvdebug&4) printf("Length              = %d\n",Length);
	while ((Length-=2) > 0) {
	  addr = maxvbase + RESPONSE_BUF + ProcessIdx;
	  if (_vme_read(VME_A16, addr, &ResponseShort, 1, VME_D16) < 0) return -4;
	  if ((ProcessIdx+=2) >= RESPONSE_BUF_SIZE) ProcessIdx = 0;

	  ResponseChar = (char) (((0xff00) & ResponseShort) >> 8);
	  if (ResponseChar == '\n') ResponseChar = '\0';
	  *Response++ = ResponseChar;
	  ResponseLength++;
	  ResponseChar = (char) (0x00ff & ResponseShort);
	  if (ResponseChar == '\n') ResponseChar = '\0';
	  *Response++ = ResponseChar;
	  ResponseLength++;
	}
	addr = maxvbase + RESPONSE_PROC;
	if (_vme_write(VME_A16, addr, &ProcessIdx, 1, VME_D32) < 0) return -5;	
      }
    }
  }

  if (maxvdebug&1) printf("MAXv_SendAndGetString[resp]: %s\n",pResponse);
  return ResponseLength;
}

int MAXv_talk( unsigned int maxvbase, char * msg)
{
  char iobuf[COMMAND_BUF_SIZE+1];
  int len,val;
  int timeout;
  unsigned int addr,stat;

  memset(iobuf,0,sizeof(iobuf));
  strncpy(iobuf,msg,COMMAND_BUF_SIZE);

  /* Clear Message Semaphore */
  addr = maxvbase + MAXV_MSG_SEM;
  val = 0;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;  

  /* Clear response available flag */
  addr = maxvbase + MAXV_STAT1_F;
  val = MAXV_RESPONSE_AVAILABLE; 
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;  

  if ((len = MAXv_puts(maxvbase, iobuf)) != strlen(iobuf)) return 0;

  /* Check for success of command */
  timeout=50;
  addr = maxvbase + MAXV_STAT1_F;
  stat = 0;
  while (timeout>0) {
    if (_vme_read(VME_A16, addr, &stat, 1, VME_D32) < 0) return -2;
    if (stat & MAXV_COMMAND_ERROR) {
      // Reset command error flag
      int temp = MAXV_COMMAND_ERROR;
      _vme_write(VME_A16, addr, &temp, 1, VME_D32);
      fprintf(stderr,"Command Error!\n");
      break;
    }
    if (stat & MAXV_RESPONSE_AVAILABLE) break;
    timeout--;
    vsleep(50);
  }

  if ((len = MAXv_gets(maxvbase,iobuf)) > 0) {
    strncpy(msg,iobuf,COMMAND_BUF_SIZE);
  }

  return len;
}

int MAXv_AxisPosition( unsigned int maxvbase, unsigned int axis, int *dpos, int *epos) 
{
  unsigned int addr;
  int offset;
  int ival;

  switch(axis) {
  case 0:  offset = X_OFFSET; break;
  case 1:  offset = Y_OFFSET; break;
  case 2:  offset = Z_OFFSET; break;
  case 3:  offset = T_OFFSET; break;
  case 4:  offset = U_OFFSET; break;
  case 5:  offset = V_OFFSET; break;
  case 6:  offset = R_OFFSET; break;
  case 7:  offset = S_OFFSET; break;
  default: return -1;
  }
  
  /* Read drive position */
  addr = maxvbase + offset + CMD_POS_UPD;
  if (_vme_read(VME_A16, addr, &ival, 1, VME_D32) < 0) return -1;
  *dpos = ival;

  addr = maxvbase + offset + ENC_POS_UPD; 
  if (_vme_read(VME_A16, addr, &ival, 1, VME_D32) < 0) return -1;
  *epos = ival;

  return 0;
}

/*
 * Determine state of axis limits. Because the MAXv is actually smart enough
 * to know about both limits, we'll return a code:
 *    00 (0) - No limits
 *    01 (1) - Negative limit actuated
 *    10 (2) - Positive limit actuated
 */
int MAXv_AxisLimits (unsigned int maxvbase, unsigned int axis, int * limit)
{
  unsigned int addr;
  unsigned int ival, poslim, neglim;

  *limit = 0;

  addr = maxvbase + MAXV_LIMIT_STAT;
  if (_vme_read(VME_A16, addr, &ival, 1, VME_D32) < 0) return -1;
  if (maxvdebug&4) printf("MAXv_AxisLimits: %08x\n",ival);
  // Reverse state of limits to work with SD1 units
  /*
  neglim = (ival & (0x01 << axis)) ? 0 : 1;
  poslim = (ival & (0x01 << (axis+8))) ? 0 : 1;
  */
  // Now report raw value of inputs
  neglim = (ival & (0x01 << axis)) ? 1 : 0;
  poslim = (ival & (0x01 << (axis+8))) ? 1: 0;

  *limit = (poslim * 2 + neglim * 1);
  return 0;
}

/*
 * Determine state of home input
 */
int MAXv_AxisOnHome (unsigned int maxvbase, unsigned int axis, int * onhome)
{
  unsigned int addr;
  unsigned int ival;
  *onhome = 0;

  addr = maxvbase + MAXV_HOME_STAT;
  if (_vme_read(VME_A16, addr, &ival, 1, VME_D32) < 0) return -1;
  //printf("MAXv_AxisHome: %08x\n",ival);

  *onhome = (ival & (0x1 << axis)) ? 0 : 1;
  return 0;
}

/*
 * Read global status from status register
 *
 * Bit 7 - Command error
 * Bit 6 - Initialized
 * Bit 5 - Encoder slip
 * Bit 4 - Overtravel detected
 * Bit 3 - Done
 * Bit 2 - Direct Interrupt status request to OMS58
 * Bit 1 - Unused
 * Bit 0 - Interrupt Request Status
 *
 */

int MAXv_GlobalStatus(unsigned int maxvbase) 
{
  unsigned int addr;
  unsigned int val;

  addr = maxvbase + MAXV_FIRM_STAT;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;
  if ((val & 0x00000007) != 0x00000004) {
    printf("MAXv Application program not running!\n");
  }

  addr = maxvbase + MAXV_MSG_SEM;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D32) < 0) return -2;  

  addr = maxvbase + MAXV_STAT1_F;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;

  if (maxvdebug) printf("MAXv_GlobalStatus: 0x%08x\n",val);
  return val;   
}

int MAXv_RegisterStatus(unsigned int maxvbase, unsigned int offset, unsigned int * val) 
{
  unsigned int addr;

  *val = 0xffffffff; // Invalid
  addr = maxvbase + offset;

  if (_vme_read(VME_A16, addr, val, 1, VME_D32) < 0) {
    return -1;
  }  
  return 0;
}

int MAXv_Reset(unsigned int maxvbase)
{
  unsigned int addr, val;
  int TimeOut;
  addr = maxvbase + MAXV_DC_MBOX;
  val = MAXV_DC_MBOX_REBOOT;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;
  vsleep(10);

  addr = maxvbase + MAXV_FIRM_STAT;
  TimeOut = 500;

  do {
    if (_vme_read(VME_A16, addr, &val, 1, VME_D32) < 0) {
      return -1;
    }
    if((TimeOut > 0) && ((val & 0x0007) != 4))
      vsleep(10);
  } while ((TimeOut-- > 0) && ((val & 0x0007) != 4));
   
  if(TimeOut > 0)
    return 0;
  else
    return -1; // Timed out
  
}
#define COMMAND_SUCCESS 1
int MAXv_Kill(unsigned int maxvbase)
{
  unsigned int addr, val;
  int TimeOut;

  addr = maxvbase + MAXV_DC_MBOX;
  val = MAXV_DC_MBOX_KILLMOVE;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;

  TimeOut = 500;
  do {
    if (_vme_read(VME_A16, addr, &val, 1, VME_D32) < 0) {
      return -1;
    }
    if((TimeOut > 0) && (val != COMMAND_SUCCESS))
      vsleep(10);
  } while ((TimeOut-- > 0) && (val != COMMAND_SUCCESS));
   
  if(TimeOut > 0)
    return 0;
  else
    return -1; // Timed out
}

int MAXv_SetDoneFlags(unsigned int maxvbase)
{
  char cmdbuf[80];

  sprintf(cmdbuf,"AA ID");
  return MAXv_SendCommand(maxvbase,cmdbuf);
}

int MAXv_ResetDoneFlag(unsigned int maxvbase, unsigned int axis)
{
  unsigned int addr,val;
  addr = maxvbase + MAXV_STAT1_F;
  val = 0x1 << axis;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;
  return 0;
}

int MAXv_ResetOvertravelFlag(unsigned int maxvbase, unsigned int axis)
{
  unsigned int addr,val;
  addr = maxvbase + MAXV_STAT1_F;
  val = 0x100 << axis;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;
  return 0;
}

int MAXv_ResetSlipFlag(unsigned int maxvbase, unsigned int axis)
{
  unsigned int addr,val;
  addr = maxvbase + MAXV_STAT1_F;
  val = 0x10000 << axis;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;
  return 0;
}


int MAXv_Init(unsigned int maxvbase)
{
  unsigned int addr, val;

  addr = maxvbase + MAXV_IACK_IDV;
  val = 0x00000067;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;

  addr = maxvbase + MAXV_STAT1_F;
  val = 0xffffffff;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;

  addr = maxvbase + MAXV_STAT2_F;
  val = 0xffffffff;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;

  addr = maxvbase + MAXV_STAT1_IER;
  val = 0x0;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;

  addr = maxvbase + MAXV_STAT2_IER;
  val = 0x0;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;

  addr = maxvbase + MAXV_MSG_SEM;
  val = 0;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D32) < 0) return -1;  

  addr = maxvbase + MAXV_FIRM_STAT;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D32) < 0) {
    return -1;
  }
  if ((val & 0x00000007) != 0x00000004) {
    printf("MAXv Application program not running!\n");
    return -2;
  }

  MAXv_SetDoneFlags(maxvbase);

  return 0;
}
