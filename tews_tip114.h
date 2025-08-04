#ifndef _tews_tip114_h
#define _tews_tip114_h

#define CONTROL(A)   (A*0x06 + 0x00)
#define DATAL(A)     (A*0x06 + 0x02)
#define DATAH(A)     (A*0x06 + 0x04)

#define READY        0x3C
#define PAR_ERR      0x3E
#define INTSTA       0x40
#define INTENA       0x42
#define INTVEC       0x45  /* 8 bit */
#define CONVERT      0x47  /* 8 bit */

/* Default configuration: 24 bits, 10 us clock, gray, no parity */
#define DEFCFG 0x188A

typedef struct {
  unsigned clockrate   : 4;
  unsigned parity_en   : 1; // 1 = detect parity errors
  unsigned parity      : 1; // 1 = odd, 0 = even
  unsigned withzero    : 1; // 1 = two additional clock cycles, 0 = one
  unsigned gray        : 1; // 1 = gray, 0 = binary
  unsigned databits    : 6; // 0x00 invalid, 0x21-0x3F invalid
  unsigned reserved    : 2; // Unused bits
} tip_fields;

#ifndef NTAXIS
#define NTAXIS 10
#endif

typedef union {
    tip_fields     fields;
    unsigned short word;
} tip_ctrl;

typedef struct {
  int zero;
  int direction;
  double resolution;
  double position;
  tip_ctrl ctrl;
  int signbit;
  unsigned int mask;
  unsigned int maxval;
} tip_axis;

typedef struct {
  unsigned int base;
  tip_axis axis[NTAXIS];
} tip_mod;

int TIP_Capture(unsigned int tipbase);
int TIP_WriteConfig(unsigned int tipbase, int axis, int bits,int rate,int gray);
int TIP_RawPosition(unsigned int tipbase, int axis, int * rawticks);

#endif

