/* $Id: oms58.h,v 1.20 2008/09/04 16:09:41 nickm Exp $ */
#ifndef _oms58_h
#define _oms58_h
#define INPUT_PUT   0x0000
#define INPUT_GET   0x0802
#define INPUT_BUF   0x0004
#define OUTPUT_PUT  0x0800
#define OUTPUT_GET  0x0002
#define OUTPUT_BUF  0x0804
#define MAILBOX     0x0f88

#define BUFFERLEN   256
#define WAITUSEC    30

/* Register offsets */
#define CONTROLREG  0x0fe1
#define STATUSREG   0x0fe3
#define SLIPREG     0x0fe7
#define DONEREG     0x0fe9
#define LIMITREG    0x0fed
#define HOMEREG     0x0fef
#define INTRVEC     0x0ff1
#define USER1REG    0x0fe5
#define USER2REG    0x0feb

/* Axis offsets */
#define X_OFFSET    0x0400
#define Y_OFFSET    0x0480
#define Z_OFFSET    0x0500
#define T_OFFSET    0x0580

#define ENC_POS     0x0000
#define CMD_POS     0x0004
#define CMD_VEL     0x0008
#define ACCEL       0x000c
#define MAX_VEL     0x0010
#define BASE_VEL    0x0014
#define PROP_GAIN   0x0018
#define DERIV_GAIN  0x001c
#define INT_GAIN    0x0020
#define ACC_FWD     0x0024
#define VEL_FWD     0x0028
#define OFFSET      0x002c

#ifndef NAXIS
#define NAXIS 4
#endif

typedef struct {
  char label;       /* One character mnemonic for the axis */
  int driveres;     /* Motor pulses per revolution   */
  int encres;       /* Encoder pulses per revolution */
  double position;
  double homeposition;
  double dscale;     /* Distance scale units/revolution */
  double bscale;     /* Base velocity scale units/revolution-sec */
  double vscale;     /* Velocity scale units/revolution-sec */
  double ascale;     /* Acceleration scale units/revolution-sec^2 */
  int acceleration; /* Acceleration in motor pulses/revolution-sec^2 */
  int basevelocity;  
  int topvelocity;  
  int deadband;
  /* Flags */
  int encmode;        /* Use encoder ? */
  int homeparity;     /* Home parity 1=High true 0=Low true */
  int homeencoder;    /* Use encoder index line to home 0=switch, 1=encoder */
  int limits;         /* Limits active */
  int stalldetection; /* Stall detection */
  int posmaintenance; /* Position maintenance */
  int enctracking;    /* Correct for motor counter screwups */
  int enable;         /* Enable driver */
  
  /* Status */
  int direction;      /* Current direction of motion */
  int fault;          /* Has a fault been registered? */
  int moving;         /* Is the axis moving? */
  int athome;
} omsaxis;

typedef struct {
  unsigned int base;
  omsaxis axis[NAXIS];
} omsmod;


int OMS_Kill(unsigned int);
int OMS_talk(unsigned int , char * );
int OMS_read(unsigned int , char * );
int OMS_write(unsigned int , char * );
int OMS_write_flush(unsigned int);
int OMS_flush_rx(unsigned int );
int OMS_flush_tx(unsigned int );
int OMS_pointers(unsigned int, unsigned short *, unsigned short *, unsigned short *, unsigned short *);

int OMS_RequestUpdate(unsigned int );
int OMS_AxisInquire(unsigned int , unsigned int , int []);
int OMS_AxisPosition(unsigned int , unsigned int , int *, int *);
int OMS_AxisInfo(unsigned int , int , int []);
int OMS_Done(unsigned int );
int OMS_Slip(unsigned int );
int OMS_IO(unsigned int );
int OMS_OnLimit(unsigned int );
int OMS_OnHome(unsigned int);
int OMS_GlobalStatus(unsigned int);
#endif /* _oms58_h */
