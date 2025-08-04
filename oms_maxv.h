/* $Id: oms_maxv.h,v 1.9 2014/12/04 15:09:20 nickm Exp $ */
#ifndef _oms_maxv_h
#define _oms_maxv_h

/* Data transfer buffers */
#define COMMAND_INSERT  0x00f0
#define COMMAND_PROC    0x00f4
#define RESPONSE_INSERT 0x00f8
#define RESPONSE_PROC   0x00fc

#define COMMAND_BUF     0x0100
#define RESPONSE_BUF    0x0500
#define UTILITY_BUF     0x0900

#define COMMAND_BUF_SIZE  1024
#define RESPONSE_BUF_SIZE 1024
#define RESPONSE_RETRY_LIM  100
#define WAITUSEC            30

/* Register offsets */
#define MAXV_LIMIT_STAT 0x0040
#define MAXV_HOME_STAT  0x0044
#define MAXV_FIRM_STAT  0x0048
#define MAXV_DC_MBOX    0x004c
#define MAXV_PC_MBOX    0x0050 /* Position request mailbox */
#define MAXV_MSG_SEM    0x0094 /* Message semaphore */
#define MAXV_GPIO_STAT  0x009c

#define MAXV_STAT1_F    0x0fc0
#define MAXV_STAT1_IER  0x0fc4
#define MAXV_STAT2_F    0x0fc8
#define MAXV_STAT2_IER  0x0fcc
#define MAXV_IACK_IDV   0x0fd0
#define MAXV_CFG_SWITCH 0x0fd4
#define MAXV_CFG_AM     0x0fd8
#define MAXV_FIFO_CSR   0x0ff8
#define MAXV_FIFO_DATA  0x0ffc

#define MAXV_COMMAND_ERROR      0x01000000
#define MAXV_RESPONSE_AVAILABLE 0x02000000

/* 
 * Compute axis fast status position as X_OFFSET + CMD_POS_UPD
 */
/* Axis offsets */
#define X_OFFSET      0x0000
#define Y_OFFSET      0x0004
#define Z_OFFSET      0x0008
#define T_OFFSET      0x000C
#define U_OFFSET      0x0010
#define V_OFFSET      0x0014
#define R_OFFSET      0x0018
#define S_OFFSET      0x001C

/* Value offsets */
#define CMD_POS_UPD   0x0000 /* Command position available each update cycle*/
#define ENC_POS_UPD   0x0020 /* Encoder position available each update cycle*/
#define CMD_POS_REQ   0x0054 /* Command position available on request */
#define ENC_POS_REQ   0x0074 /* Encoder position available on request */

#define MAXV_DC_MBOX_ID_QUERY 1
#define MAXV_DC_MBOX_KILLMOVE 2 /* Kill all moves    */
#define MAXV_DC_MBOX_RESET    3 /* Reset controller  */
#define MAXV_DC_MBOX_REBOOT   4 /* Reboot controller */

#define NAXIS            8

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
  double kp;         /* Proportional gain */
  double ki;         /* Integral gain */
  double kd;         /* Differential gain */
  double ka;         /* Acceleration feedforward */
  int acceleration;  /* Acceleration in motor pulses/revolution-sec^2 */
  int basevelocity;  
  int topvelocity;  
  int deadband;
  /* Flags */
  int is_servo;       /* Is this a servo motor? */
  int enable_high;    /* Is the enable output high true or low true */
  int limit_high;     /* Are the limit inputs high true or low true */
  int encmode;        /* Use encoder ? */
  int homeparity;     /* Home parity 1=High true 0=Low true */
  int homeencoder;    /* Use encoder index line to home 0=switch, 1=encoder */
  int limits;         /* Limits active */
  int limitparity;    /* Limit parity 1=High true 0=Low true */
  int stalldetection; /* Stall detection */
  int posmaintenance; /* Position maintenance */
  int enctracking;    /* Correct for motor counter screwups */
  int enable;         /* Enable driver */
  
  /* Status */
  int direction;      /* Current direction of motion */
  int fault;          /* Has a fault been registered? */
  int moving;         /* Is the axis moving? */
  int athome;
} maxvaxis;

typedef struct {
  unsigned int base;
  maxvaxis axis[NAXIS];
} maxvmod;

int MAXv_Kill(unsigned int);
int MAXv_talk(unsigned int, char *);
int MAXv_getpointers(unsigned int, unsigned int *, unsigned int *, unsigned int *, unsigned int *);
int MAXv_AxisPosition(unsigned int maxvbase, unsigned int axis, int *dpos, int *epos);
int MAXv_AxisLimits (unsigned int maxvbase, unsigned int axis, int * limit);
int MAXv_AxisOnHome (unsigned int maxvbase, unsigned int axis, int * onhome);
int MAXv_GlobalStatus(unsigned int);
int MAXv_RegisterStatus(unsigned int maxvbase, unsigned int offset, unsigned int * val);
int MAXv_Reset(unsigned int);
int MAXv_ResetDoneFlag(unsigned int,unsigned int);
int MAXv_ResetOvertravelFlag(unsigned int,unsigned int);
int MAXv_ResetSlipFlag(unsigned int, unsigned int);
int MAXv_Init(unsigned int);
int MAXv_SendCommand(unsigned int maxvbase, char * pCommand);
int MAXv_SendAndGetString(unsigned int maxvbase, char * pCommand, char * pResponse);

#endif /* _oms_maxv_h */
