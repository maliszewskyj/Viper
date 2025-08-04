#ifndef _acro_ip470_h
#define _acro_ip470_h

#define PORT0  0x00
#define PORT1  0x02
#define PORT2  0x04
#define PORT3  0x06
#define PORT4  0x08
#define PORT5  0x0a
#define RWMASK 0x0e  // Read/Write mask register: 1=read only, 0=read/write


int IP470_Config(unsigned int base, unsigned char writemask);
int IP470_SetByte (unsigned int base, int whichbyte, unsigned char byte);
int IP470_GetByte (unsigned int base, int whichbyte, unsigned char *byte);
int IP470_SetClrBit(unsigned int base, int whichbit, int set);

#endif
