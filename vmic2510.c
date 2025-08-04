static char rcsid[] = "$Id: vmic2510.c,v 1.2 2008/12/31 16:59:05 nickm Exp $";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "vme_util.h"
#include "vmic2510.h"

/*
  Author: Dr. N. C. Maliszewskyj, NIST Center for Neutron Research 2008

  The VMIC 2510B 64 bit TTL Digital I/O board consists of eight eight-bit
  I/O ports which can be individually configured as TTL inputs or outputs.

  This extension is written so that the bit identified as 0 will correspond
  to the appropriate bit in the VMIC2510B hardware reference. This requires
  some contortions in the bit/port assignment to get this mapping right. For
  the sake of this metaphor, we'll remap port0 to bits 0-7 and port7 to bits
  56-63.

  Extension usage:
  
  dio create ?base? ?writemask?
  d set   ?bit?                  Set bit
  d clear ?bit?                  Clear bit
  d stat  ?bit?                  Get current value of bit
  d read                         Get all bits of data
 */

static unsigned int  port[8] = {PORT5, PORT6, PORT7, PORT8,
				PORT1, PORT2, PORT3, PORT4};
int VMIC2510_Config(unsigned int base, int led_off, int P3state, int P4state,
		    unsigned char writemask)
{
  unsigned int  addr;
  unsigned short val;
  int i;
  int mask[8] = {0x0800, 0x0400, 0x0200, 0x0100,
		 0x8000, 0x4000, 0x2000, 0x1000};
  val = 0;
  if (led_off) val += LED_OFF;
  if (P3state) val += P3_NORMAL;
  if (P4state) val += P4_NORMAL;

  /* Set write mask portion of CSR */
  for (i=0;i<8;i++) {
    if (writemask & (0x1 << i)) {
      val += mask[i];
    }
  }

  addr = base + CSR;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D16) < 0) return -11;
  return 0; /* OK */
}

int VMIC2510_SetByte(unsigned int base, int whichport, unsigned char byte)
{
  unsigned int addr;
  if ((whichport < 0) || (whichport > 7)) return -10; /* Invalid arg */

  addr = base + port[whichport];
  if (_vme_write(VME_A16, addr, &byte, 1, VME_D8) < 0) return -11; /*I/O err*/

  return 0; /* OK */
}

int VMIC2510_GetByte(unsigned int base, int whichport, unsigned char *byte)
{
  unsigned int addr;
  if ((whichport < 0) || (whichport > 7)) return -10; /* Invalid arg */
  addr = base + port[whichport];
  if (_vme_read(VME_A16, addr, byte, 1, VME_D8) < 0) return -11; /*I/O err*/
  return 0; /* OK */
}

int VMIC2510_SetShort(unsigned int base, int whichword, unsigned short word)
{
  unsigned int addr, offset;
  switch(whichword) {
  case 0: offset = PORT6; break;
  case 1: offset = PORT8; break;
  case 2: offset = PORT2; break;
  case 3: offset = PORT4; break;
  default:
    return -10; /* Invalid arg */
  }
  addr = base + offset;

  if (_vme_write(VME_A16, addr, &word, 1, VME_D16) < 0) return -11;
  return 0; /* OK */
}

int VMIC2510_GetShort(unsigned int base, int whichword, unsigned short *word)
{
  unsigned int addr, offset;
  switch(whichword) {
  case 0: offset = PORT6; break;
  case 1: offset = PORT8; break;
  case 2: offset = PORT2; break;
  case 3: offset = PORT4; break;
  default:
    return -10; /* Invalid arg */
  }
  addr = base + offset;

  if (_vme_read(VME_A16, addr, word, 1, VME_D16) < 0) return -11;
  return 0; /* OK */

}

int VMIC2510_SetWord(unsigned int base, int whichword, unsigned int word)
{
  unsigned int addr;
  if (whichword) {
    addr = base + PORT4;
  } else {
    addr = base + PORT8;
  }

  if (_vme_write(VME_A16, addr, &word, 1, VME_D32) < 0) return -11; /*I/O err*/

  return 0; /* OK */
}

int VMIC2510_GetWord(unsigned int base, int whichword, unsigned int *word)
{
  unsigned int addr;
  if (whichword) {
    addr = base + PORT4;
  } else {
    addr = base + PORT8;
  }

  if (_vme_read(VME_A16, addr, word, 1, VME_D32) < 0) return -11; /*I/O err*/

  return 0; /* OK */
}


int VMIC2510_SetClrBit(unsigned int base, int whichbit, int set)
{
  unsigned int addr, whichport;
  unsigned short word, outword, mask, bitval;
  int bitoffset, retn;
  if ((whichbit < 0) || (whichbit > 63)) return -10; /* Invalid arg */

  whichport = whichbit / 16;
  bitoffset = whichbit % 16;
  word = 0;
  if ((retn = VMIC2510_GetShort(base, whichport, &word)) < 0) return retn;

  bitval = (0x0001 << bitoffset) & 0xffff;
  mask   = ~bitval;

  if (set) {
    outword = word | bitval;
  } else {
    outword = word & mask;
  } 

  /*
  printf("Word(in) = 0x%04x\n",word);
  printf("Mask     = 0x%04x\n",mask);
  printf("Bitval   = 0x%04x\n",bitval);
  printf("Word(out)= 0x%04x\n",outword);
  */
  if ((retn = VMIC2510_SetShort(base, whichport, outword)) < 0) return retn;
  return 0;
  
}
