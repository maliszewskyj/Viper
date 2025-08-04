static char rcsid[] = "$Id: tews_tip114.c,v 1.4 2013/04/08 15:35:16 nickm Exp $";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "tews_tip114.h"
#include "vme_util.h"

int TIP_WriteConfig(unsigned int tipbase, int axis, int bits,int rate,int gray)
{
  tip_ctrl regcfg;
  unsigned short sval;
  unsigned int addr;
  /* Set defaults */
  regcfg.word = DEFCFG;

  if ((bits == 0) || (bits > 0x21)) return -1; // Invalid configuration
  regcfg.fields.databits = bits;
  regcfg.fields.clockrate = rate;
  regcfg.fields.parity_en = 0;
  regcfg.fields.parity = 0;
  regcfg.fields.withzero = 0;
  regcfg.fields.gray = gray;

  sval = regcfg.word;
  //printf("TIP_WriteConfig: 0x%04x:%04x\n",addr,sval);
  addr = tipbase + CONTROL(axis);
  if (_vme_write(VME_A16, addr, &sval, 1, VME_D16) < 0) return -2;
  return 0;
}

/*
 * Trigger position capture
 */
int TIP_Capture(unsigned int tipbase) 
{
  unsigned char bval;
  unsigned short sval;
  unsigned int addr, Timeout;

  /* Start conversion */
  //printf("TIP_Capture() - start conversion\n");
  bval = 0xff;
  sval = 0xffff;
  addr = tipbase + CONVERT-1;
  if (_vme_write(VME_A16, addr, &sval, 1, VME_D16) < 0) return -1;

  //printf("TIP_Capture() - wait for conversion\n");
  Timeout = 10;
  addr = tipbase + READY;
  while(Timeout-- > 0) {
    vsleep(50);
    if (_vme_read(VME_A16, addr, &sval, 1, VME_D16) < 0) return -2;
    //printf("TIP_Capture() - 0x%04x:0x%02x\n",addr,sval);
    if (sval == 0x3FF) return 0; // Success
  }
  //if (Timeout > 0) { printf("TIP_Capture() - successful\n"); } else { printf("TIP_Capture() - timed out\n"); }
  return -10; // Timeout
}

/*
 * Return raw encoder position
 */
int TIP_RawPosition(unsigned int tipbase, int axis, int * rawticks)
{
  unsigned int addr;
  unsigned short sval;
  
  if ((axis < 0) || (axis > 9)) return -1;

  addr = tipbase + DATAH(axis);
  if (_vme_read(VME_A16, addr, &sval, 1, VME_D16) < 0) return -1;
  *rawticks = ((unsigned int) sval) << 16;

  addr = tipbase + DATAL(axis);
  if (_vme_read(VME_A16, addr, &sval, 1, VME_D16) < 0) return -1;
  *rawticks += sval;
//printf("TIP_RawPosition[0x%0x04x] Axis %d: 0x%08x\n",tipbase,axis,*rawticks);
  return 0;
}

