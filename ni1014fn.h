#ifndef _NI1014FN_H
#define _NI1014FN_H
/*
 * Function prototypes for National Instruments 1014 GPIB Controller
 *
 *
 */
typedef struct {
  int fd;                /* File descriptor of pertinent device */
  char * base;           /* Pointer to base address */
  unsigned int addr;     /* Base address */

  /* Misc flags */
  int cic;  /* Controller in charge */
  int seoi; /* Send EOI */
  int ren;  /* Set REN  */
} ibbd; 

int  ibinit(ibbd * );
int  ibcmd(ibbd * , char * , int );
void ibsic(ibbd * );
void ibren(ibbd * , int );
int  ibcac(ibbd * , int );
int  ibclr(ibbd * , int );
int  ibtrg(ibbd * , int );
unsigned char ibstat(ibbd *);
int  ibrd(ibbd * , int , char * , int );
int  ibwrt(ibbd * , int , char * , int );

#define IB_USEC 3000

#define IB_TIMEOUT -10

#endif
