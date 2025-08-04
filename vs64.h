#ifndef _vs64_h
#define _vs64_h

#define VS64_XFER     0x0000
#define VS64_XFERCLR  0x0100
#define VS64_XFERFLY  0x0200
#define VS64_STATUS   0x0400
#define VS64_CONTROL  0x0402
#define VS64_A32BASE1 0x0404
#define VS64_A32BASE2 0x0406
#define VS64_INTSET   0x040E
#define VS64_CLKPROG  0x0410
#define VS64_GATESZ   0x0412
#define VS64_REFCLK   0x0414
#define VS64_A24BASE  0x0416
#define VS64_ID       0x041E
#define VS64_RESET    0x0420
#define VS64_CLKXFER  0x0422
#define VS64_CNT_EN   0x0424
#define VS64_CNT_DIS  0x0426
#define VS64_GLBRST   0x0428
#define VS64_ARM      0x042A
#define VS64_DISARM   0x042C
#define VS64_TRGGATE  0x042E
#define VS64_GENPULSE 0x0430
#define VS64_CLR_IRQ  0x0432

#define VS64_GRP1_RST_EN 0x0304
#define VS64_GRP2_RST_EN 0x0344
#define VS64_GRP3_RST_EN 0x0384
#define VS64_GRP4_RST_EN 0x03C4
#define VS64_GRP_CTR_RST 0x0316

#define VS64_NCOUNTERS 64

typedef struct {
  unsigned int base;
  unsigned int counts[64];
} vs;


#endif
