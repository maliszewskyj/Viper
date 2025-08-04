#ifndef _vmic2210
#define _vmic2210

/* Register map */
#define BDID              0x0000
#define CSR               0x0002
#define RELAY_CTRL_REG_0  0x0010
#define RELAY_CTRL_REG_1  0x0011
#define RELAY_CTRL_REG_2  0x0012
#define RELAY_CTRL_REG_3  0x0013
#define RELAY_CTRL_REG_4  0x0014
#define RELAY_CTRL_REG_5  0x0015
#define RELAY_CTRL_REG_6  0x0016
#define RELAY_CTRL_REG_7  0x0017
#define RELAY_READ_REG_0  0x0018
#define RELAY_READ_REG_1  0x0019
#define RELAY_READ_REG_2  0x001A
#define RELAY_READ_REG_3  0x001B
#define RELAY_READ_REG_4  0x001C
#define RELAY_READ_REG_5  0x001D
#define RELAY_READ_REG_6  0x001E
#define RELAY_READ_REG_7  0x001F

int VMIC2210_CheckID (unsigned int base);
int VMIC2210_Enable  (unsigned int base, int enable);
int VMIC2210_SetLongWord(unsigned int base, unsigned int word);
int VMIC2210_GetLongWord(unsigned int base, unsigned int * word);
int VMIC2210_SetClrBit(unsigned int base, int bit, int set);


#endif
