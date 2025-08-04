#ifndef _vsc16_h
#define _vsc16_h
/* VSC16 scaler register map */
#define VSC_RESET   0x00
#define VSC_CONTROL 0x04
#define VSC_DIRECT  0x08
#define VSC_STATUS  0x10
#define VSC_IRQLEV  0x14
#define VSC_IRQMSK  0x18
#define VSC_IRQRST  0x1C
#define VSC_SERIAL  0x20
#define VSC_MODTYP  0x24
#define VSC_MFCID   0x28
/* Read counter contents */
#define VSC_DATAB   0x80
#define VSC_DATA(x) ((4 * x) + VSC_DATAB)
/* Read counter contents and reset */
#define VSC_DATRB   0xC0
#define VSC_DATR(x) ((4 * x) + VSC_DATRB)

typedef struct {
  unsigned int base;
  unsigned int ncounters;
  unsigned short direction;
  unsigned int counts[16];
} vscaler;



#endif /* _vsc16_h */
