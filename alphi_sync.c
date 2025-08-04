static char rcsid[] = "$Id: acro_ip470.c,v 1.2 2016/12/05 16:21:51 nickm Exp $";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "vme_util.h"
#include "alphi_sync.h"

int AlphiSync_Config(unsigned int base, unsigned char writemask)
{
  unsigned int  addr;
  unsigned char cval;
  unsigned short sval;

  cval = ~writemask & 0x3F; // Only six ports available
  sval = 0xFF00 + cval;

  addr = base + RWMASK-1;
  printf("Creating IP470 at 0x%04x with write mask 0x%02x\n",base,cval);
  if (_vme_write(VME_A16, addr, &sval, 1, VME_D16) < 0) return -11;
  return 0; /* OK */
}

/*
 * Return raw encoder position
 */
int AlphiSync_RawPosition(unsigned int base, int axis, int * rawticks)
{
  unsigned int addr;
  unsigned short sval;
  
  if ((axis < 0) || (axis > 1)) return -1;

  addr = base + CDRA;
  if (_vme_read(VME_A16, addr, &sval, 1, VME_D16) < 0) return -1;
  *rawticks = (unsigned int) sval;

//printf("AlphiSync_RawPosition[0x%0x04x] Axis %d: 0x%08x\n",base,axis,*rawticks);
  return 0;
}

