static char rcsid[] = "$Id: acro_ip470.c,v 1.2 2016/12/05 16:21:51 nickm Exp $";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "vme_util.h"
#include "acro_ip470.h"

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
  
  ipio create ?base? ?writemask?
  d set   ?bit?                  Set bit
  d clear ?bit?                  Clear bit
  d stat  ?bit?                  Get current value of bit
  d read                         Get all bits of data
 */

static unsigned int  port[8] = {PORT0, PORT1, PORT2, PORT3,
                                PORT4, PORT5, PORT4, PORT5};
int IP470_Config(unsigned int base, unsigned char writemask)
{
  unsigned int  addr;
  unsigned char cval;
  unsigned short sval;

  cval = ~writemask & 0x3F; // Only six ports available
  sval = 0xFF00 + cval;

  addr = base + RWMASK;
  printf("Creating IP470 at 0x%04x with write mask 0x%02x\n",base,cval);
  if (_vme_write(VME_A16, addr, &sval, 1, VME_D16) < 0) return -11;
  return 0; /* OK */
}

int IP470_SetByte(unsigned int base, int whichport, unsigned char byte)
{
  unsigned int addr;
  unsigned short word;
  if ((whichport < 0) || (whichport > 5)) return -10; /* Invalid arg */

  //addr = base + port[whichport]-1;
  addr = base + port[whichport];
  word = 0xFF00 + byte;
  if (_vme_write(VME_A16, addr, &word, 1, VME_D16) < 0) return -11; /*I/O err*/

  return 0; /* OK */
}

int IP470_GetByte(unsigned int base, int whichport, unsigned char *byte)
{
  unsigned int addr;
  unsigned short word;
  if ((whichport < 0) || (whichport > 5)) return -10; /* Invalid arg */
  addr = base + port[whichport];
  if (_vme_read(VME_A16, addr, &word, 1, VME_D16) < 0) return -11; /*I/O err*/
  *byte = (unsigned char) (0x00FF & word);
  return 0; /* OK */
}

int IP470_SetClrBit(unsigned int base, int whichbit, int set)
{
  unsigned int whichport;
  unsigned char outbyte, iobyte,bitval,mask;
  int bitoffset, retn;

  if ((whichbit < 0) || (whichbit > 63)) return -10; /* Invalid arg */

  whichport = whichbit / 8;
  bitoffset = whichbit % 8;
  iobyte  = 0;
  if ((retn = IP470_GetByte(base, whichport, &iobyte)) < 0) return retn;

  bitval = (0x01 << bitoffset) & 0xff;
  mask   = ~bitval;

  if (set) {
    outbyte = iobyte | bitval;
  } else {
    outbyte = iobyte & mask;
  } 

  /*
  printf("Byte(in) = 0x%02x\n",iobyte);
  printf("Mask     = 0x%02x\n",mask);
  printf("Bitval   = 0x%02x\n",bitval);
  printf("Byte(out)= 0x%02x\n",outbyte);
  */
  
  if ((retn = IP470_SetByte(base, whichport, outbyte)) < 0) return retn;
  return 0;
  
}
