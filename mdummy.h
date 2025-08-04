#ifndef _mdummy_h
#define _mdummy_h

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
} mdumaxis;

typedef struct {
  unsigned int base;
  mdumaxis axis[NAXIS];
} mdummod;


#endif
