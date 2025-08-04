static char rcsid[] = "$Id: vmic2210.c,v 1.1 2007/10/17 20:13:55 nickm Exp $";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "vme_util.h"
#include "vmic2210.h"

int VMIC2210_CheckID (unsigned int base)
{
  unsigned int addr;
  unsigned char val;

  addr = base + BDID;
  val = 0;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D8) < 0) return -11;
  if (val != 0x1B) return -1;
  
  return 0;
}

int VMIC2210_Enable(unsigned int base, int enable)
{
  unsigned int addr;
  unsigned short word;
  addr = base + CSR;
  word = (enable) ? 0xe000 : 0x0000;
  if (_vme_write(VME_A16, addr, &word, 1, VME_D16) < 0) return -11;
  return 0;
}

int VMIC2210_SetLongWord(unsigned int base, unsigned int word)
{
  unsigned int addr;
  addr = base + RELAY_CTRL_REG_4;
  if (_vme_write(VME_A16, addr, &word, 1, VME_D32) < 0) return -11;
  return 0;
}

int VMIC2210_GetLongWord(unsigned int base, unsigned int * word)
{
  unsigned int addr;
  unsigned int val;
  addr = base + RELAY_READ_REG_4;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D32) < 0) return -11;

  *word = val;
  return 0;
}

int VMIC2210_SetClrBit(unsigned int base, int bit, int set)
{
  unsigned int addr;
  unsigned int val, mask, bitval, outval;
  addr = base + RELAY_READ_REG_4;
  if ((bit < 0) || (bit > 31)) return -1;
  /* Read what's already set */
  if (_vme_read(VME_A16, addr, &val, 1, VME_D32) < 0) return -11;

  mask = ~(0x1 << bit);
  bitval = (set) ? (0x1 << bit) : 0;
  
  addr = base + RELAY_CTRL_REG_4;
  outval = (val & mask) | bitval;
  if (_vme_write(VME_A16, addr, &outval, 1, VME_D32) < 0) return -12;
  return 0;
}




