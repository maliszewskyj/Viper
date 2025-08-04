#ifndef _vmic2510
#define _vmic2510

/* Register Offsets */
#define PORT4     0x0000
#define PORT3     0x0001
#define PORT2     0x0002
#define PORT1     0x0003
#define PORT8     0x0004
#define PORT7     0x0005
#define PORT6     0x0006
#define PORT5     0x0007
#define CSR       0x00F0

/* CSR is sixteen bits wide and set by dip switches on the board */
#define PORT1_OUT 0x8000
#define PORT2_OUT 0x4000
#define PORT3_OUT 0x2000
#define PORT4_OUT 0x1000
#define PORT5_OUT 0x0800
#define PORT6_OUT 0x0400
#define PORT7_OUT 0x0200
#define PORT8_OUT 0x0100
#define P3_NORMAL 0x0080
#define P4_NORMAL 0x0020
#define LED_OFF   0x0040

#define PORT1_4_OFF 0x0000
#define PORT5_8_OFF 0x0004


int VMIC2510_Config(unsigned int base, int led_off, int P3state, int P4state,
		    unsigned char writemask);
int VMIC2510_SetByte (unsigned int base, int whichbyte, unsigned char byte);
int VMIC2510_GetByte (unsigned int base, int whichbyte, unsigned char *byte);
int VMIC2510_SetShort(unsigned int base, int whichword, unsigned short word);
int VMIC2510_GetShort(unsigned int base, int whichword, unsigned short *word);
int VMIC2510_SetWord (unsigned int base, int whichword, unsigned int word);
int VMIC2510_GetWord (unsigned int base, int whichword, unsigned int *word);
int VMIC2510_SetClrBit(unsigned int base, int whichbit, int set);

#endif
