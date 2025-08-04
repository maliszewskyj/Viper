#ifndef _NI1014RG_H
#define _NI1014RG_H
/*
 * Register definitions for National Instruments 1014 GPIB Controller
 * 
 * Controller properties: A16D08 VME slave
 *                        Responds to AM codes 2D and 29
 *
 */



/* Read registers  */
#define DIR    0x1
#define ISR1   0x3
#define ISR2   0x5
#define SPSR   0x7
#define ADSR   0x9
#define CPTR   0xB
#define ADR0   0xD
#define ADR1   0xF

/* Write registers */
#define CDOR   0x1
#define IMR1   0x3
#define IMR2   0x5
#define SPMR   0x7
#define ADMR   0x9
#define AUXMR  0xB
#define ADR    0xD
#define EOSR   0xF

/* Bit definitions */

#define GTL    0x01
#define SDC    0x04
#define GET    0x08
#define TCT    0x09
#define DCL    0x14
#define SPE    0x18
#define SPD    0x19
#define UNT    0x5f
#define UNL    0x3f
#define MTA(x) (0x40 + x)
#define MLA(x) (0x20 + x)

/* ISR1 Bits */
#define ISR1_DI     0x01
#define ISR1_DO     0x02
#define ISR1_ERR    0x04
#define ISR1_ENDRX  0x10

/* ISR2 Bits */
#define ISR2_CO     0x08

/* IMR1 Bits */
#define IMR2_DMA0   0x20

/* ADR */
#define ADR_DT1     0x80
#define ADR_DL1     0x10

/* ADSR Bits */
#define NATN        0x80

/* ADMR Bits */
#define ADMR_MODE1  0x01
#define ADMR_TRM    0x30

/* AUXMR commands */
#define EIPON       0x00
#define RST         0x02
#define FH          0x04
#define RTL         0x05
#define SEOI        0x06
#define CPPF        0x01
#define SPPF        0x09
#define TCA         0x11
#define TCS         0x12
#define TCSE        0x1A
#define GTS         0x10
#define LTN         0x13
#define LTNC        0x1B
#define LUN         0x1C
#define EPP         0x1D
#define SIFC        0x1E
#define CIFC        0x16
#define SREN        0x1F
#define CREN        0x17
#define DSC         0x14

/* AUXMR Hidden Registers */
#define ICR         0x20
#define PPR         0x60
#define AUXRA       0x80
#define AUXRB       0xA0
#define AUXRE       0xC0

/* User Specified Constants */
#define SEL0        0x00
#define SEL1        0x80
#define MA          0x00
#define SC          0x09

#endif
