static char cvsid[] = "$Id: omsmot.c,v 1.44 2013/04/04 18:43:16 nickm Exp $";
/*
 * omsmot - code for controlling Oregon Micro Systems motor controller
 * 
 * oms58 create [base address]
 *
 * Created command has the following syntax:
 *
 * oms0 raw <string>
 * oms0 get <axis> position
 *          <axis> baserate    
 *	    <axis> acceleration 
 * oms0 set <axis> position     value
 *          <axis> baserate     value
 *          <axis> acceleration value
 * oms0 move <axis> <destination>
 * oms0 delta <axis> <increment>
 * oms0 status <axis> moving
 *             <axis> limits
 *             <axis> global
 * oms0 stop
 *

oms0 raw    <string>
oms0 kill
oms0 register
oms0 move      <axis> <destination>
oms0 jog       <axis> <direction>
oms0 position  <axis> ?position?
oms0 configure <axis> -driveres       #
                      -encres         #
                      -acceleration   #
                      -basevel        #
                      -topvel         #
		      -deadband       #            deadband in encoder pulses
                      -limits         (on/off)
                      -encmode        (on/off)     position maintenance
                      -stalldetection (on/off)
		      -posmaintenance (on/off)
oms0 status    <axis> moving
                      limits
                      global
                      fault
		      enabled

oms0 enable    <axis> facility
oms0 disable   <axis> facility

 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <tcl.h>
#include "oms58.h"
#include "vme_util.h"

#define AMMOVE 1
//#define RESETPOS 1
#define MOVEREL 1
#define HOMEPARITY_HIGH 0
/* Global variables */
static int ocounter;
static char omsbuf[BUFFERLEN];
extern int omsdebug;

/* 
 * oms_axis()
 * Determine which axis is requested
 */

int
oms_axis(Tcl_Interp *interp, char in, char *out, int * axisno) {

  *out = 'A';
  *axisno = -1;
  switch(in) {
  case '0':
  case 'X': *out = 'X'; *axisno = 0; break;
  case '1':
  case 'Y': *out = 'Y'; *axisno = 1; break;
  case '2':
  case 'Z': *out = 'Z'; *axisno = 2; break;
  case '3':
  case 'T': *out = 'T'; *axisno = 3; break;
  case 'A': *out = 'A'; *axisno = -1; break; /* All axes */
  default:
    Tcl_SetResult(interp,"Valid axes: X Y Z T (or 0 1 2 3)",TCL_STATIC);
    return TCL_ERROR;
  }
  return TCL_OK;
}

int
oms_raw(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char * strval;
  int lstr, len;
  omsmod * optr;

  optr = (omsmod *) clientdata;
  strval = Tcl_GetStringFromObj(objv[0],&lstr);

  /*
  OMS_write(optr->base,strval);
  vsleep(10); 
  memset(omsbuf,0,sizeof(omsbuf));
  if ((len = OMS_read(optr->base,omsbuf)) > 0) {
    Tcl_SetObjResult(interp,Tcl_NewStringObj(omsbuf,len));
  } 
  */ 
  strncpy(omsbuf,strval,sizeof(omsbuf)-1);
  if ((len = OMS_talk(optr->base,omsbuf)) > 0) {
    Tcl_SetObjResult(interp,Tcl_NewStringObj(omsbuf,len));
  }

  return TCL_OK;
}

int
oms_info(ClientData clientdata, Tcl_Interp *interp, int axisno) {
  int info[12], i;
  omsmod * optr;
  Tcl_Obj * wptr;
  char * regname[12] = { "EncPos",
			 "ComPos",
			 "ComVel",
			 "Accel",
			 "MaxVel",
			 "BaseVel",
			 "PropGain",
			 "DerivGain",
			 "IntegralGain",
			 "AccFeedFwd",
			 "VelFeedFwd",
			 "Offset"};

  optr = (omsmod *) clientdata;
  OMS_RequestUpdate(optr->base);
  vsleep(10);
  if (OMS_AxisInfo( optr->base, axisno, info) < 0) {
    Tcl_SetResult(interp,"Error occurred while getting axis info",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Update elements in axis structure */
  /* PAY ATTENTION TO ME!!! */

  /* Report result in a list */
  wptr = Tcl_NewObj();
  for (i=0;i<12;i++) {
    Tcl_ListObjAppendElement(interp,wptr,
			     Tcl_NewStringObj(regname[i],strlen(regname[i])));
    Tcl_ListObjAppendElement(interp,wptr,Tcl_NewIntObj(info[i]));
  }
  Tcl_SetObjResult(interp,wptr);

  return TCL_OK;
}

int
oms_position(ClientData clientdata, Tcl_Interp *interp,
	     int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval;
  int lstr, axisno, pulses;
  double position;
  omsmod * optr;

  if (!objc) {
    Tcl_SetResult(interp,"Usage: omsn position <axis> ?position?\n",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];
  if (oms_axis(interp,ax,&ax,&axisno) != TCL_OK) return TCL_ERROR;
  optr = (omsmod *) clientdata;


  if (objc > 1) { /* Set position */
    if (Tcl_GetDoubleFromObj(interp,objv[1],&position) != TCL_OK) 
      return TCL_ERROR;
    /*
    pulses = (int) ((position/optr->axis[axisno].dscale) 
		    * ((optr->axis[axisno].encmode)?
		       optr->axis[axisno].encres :
		       optr->axis[axisno].driveres));
    */
    pulses = (int) ((position/optr->axis[axisno].dscale) * 
		    optr->axis[axisno].encres);
    memset(omsbuf,0,sizeof(omsbuf));
    sprintf(omsbuf,"A%c ER%d,%d LP%d;",
	    optr->axis[axisno].label,
	    optr->axis[axisno].encres,
	    optr->axis[axisno].driveres,
	    pulses);
    if ((lstr = OMS_talk( optr->base, omsbuf)) > 0) {
      Tcl_AppendResult(interp,omsbuf,(char *) 0);
    }
    /* If we are not in encoder mode, send "ER1,1" after loading counters */
    if (!optr->axis[axisno].encmode) {
      strcat(omsbuf," ER1,1 ");
      if ((lstr = OMS_talk( optr->base, omsbuf)) > 0) {
	Tcl_AppendResult(interp,omsbuf,(char *) 0);
      }
    }
  } else {        /* Get position */
    int epos, dpos;
    OMS_RequestUpdate(optr->base);
    if (OMS_AxisPosition( optr->base, axisno, &dpos, &epos) < 0) {
      Tcl_SetResult(interp,"Error occurred while reading position",TCL_STATIC);
      return TCL_ERROR;
    }

    if (optr->axis[axisno].encmode) {
      optr->axis[axisno].position = (((double)epos)/
				     optr->axis[axisno].encres) *
	optr->axis[axisno].dscale;
    } else {
      optr->axis[axisno].position = (((double)dpos)/
				     optr->axis[axisno].driveres) *
	optr->axis[axisno].dscale;
    }
    Tcl_SetObjResult(interp,Tcl_NewDoubleObj(optr->axis[axisno].position));
  }

  return TCL_OK;
}

/*
 * Motion status - we have to maintain our own motion status
 *                 1) Set a moving[] flag if we initiate motion
 *                 2) Check the DONE register periodically to see when to clear
 *
 */
int
oms_moving(ClientData clientdata, Tcl_Interp *interp,char ax) {
  int i;
  int moving, vel[4];
  omsmod * optr;
  
  optr = (omsmod *) clientdata;
  OMS_RequestUpdate(optr->base);
  vsleep(10);
  if (OMS_AxisInquire( optr->base, CMD_VEL, vel) < 0) {
    Tcl_SetResult(interp,"Error occurred while reading velocity",TCL_STATIC);
    return TCL_ERROR;
  }

  for (i=0;i<4;i++) {
    optr->axis[i].moving = (vel[i]) ? 1 : 0;
  }
  
  /* Mask out proper bits */
  switch (ax) {
  case 'X': moving = optr->axis[0].moving; break;
  case 'Y': moving = optr->axis[1].moving; break;
  case 'Z': moving = optr->axis[2].moving; break;
  case 'T': moving = optr->axis[3].moving; break;
  case 'A': moving = optr->axis[0].moving +
	      2 * optr->axis[1].moving +
	      4 * optr->axis[2].moving +
	      8 * optr->axis[3].moving;    break;
  }
  if (!moving) {
    i = OMS_GlobalStatus( optr->base);
    // if (i != 0x02) {
    //   if (i & 0x10) printf("DONE ");
    //   if (i & 0x08) printf("OVERTRAVEL ");
    //   if (i & 0x04) printf("SLIP ");
    //   printf("\n");
    // }
  }
  if (omsdebug & 1) printf("moving = %d\n",moving);
  Tcl_SetObjResult(interp,Tcl_NewIntObj(moving));
  return TCL_OK;
}

/*
 * Determine state of limit switches: +1 positive limit actuated
 *                                     0 no limit actuated
 *                                    -1 negative limit actuated
 */
int
oms_limits(ClientData clientdata, Tcl_Interp *interp,char ax) {
  omsmod * optr;
  int limstat, axisno, i;
  
  optr = (omsmod *) clientdata;


  i = OMS_GlobalStatus( optr->base);
  //  if (i != 0x02) {
  //  if (i & 0x10) printf("DONE ");
  //  if (i & 0x08) printf("OVERTRAVEL ");
  //  if (i & 0x04) printf("SLIP ");
  //  printf("\n");
  //}

  /* Simply determine whether any limit was observed */
  if ((limstat = OMS_OnLimit( optr->base)) < 0) {
    Tcl_SetResult(interp,"Error reading from module",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Mask for the limit in question */
  switch(ax) {
  case 'X': axisno = 0; limstat &= 0x01; break;
  case 'Y': axisno = 1; limstat &= 0x02; break; 
  case 'Z': axisno = 2; limstat &= 0x04; break;
  case 'T': axisno = 3; limstat &= 0x08; break;
  }

  if ((ax == 'A') || (limstat == 0)) {
    Tcl_SetObjResult(interp, Tcl_NewIntObj(limstat));
    return TCL_OK;
  }

  /* Check Limit register */

  /*
  sprintf(omsbuf,"A%c QA ",ax);
  if ((lstr = OMS_talk( optr->base, omsbuf)) < 0) {
    Tcl_SetResult(interp,"oms_limits: no response from controller",TCL_STATIC);
    return TCL_ERROR;
  }  
  */

  /* 
   * Response is of form:
   *
   * \n\r\rPNNH\n\r\r
   */

  /*
  if (lstr != 10) {
    Tcl_SetResult(interp,"oms_limits: bad response from controller",
		  TCL_STATIC);
  }
  */

  /*
  ch = omsbuf[3];
  limstat = (ch == 'P') ? 1 : -1;
  ch = omsbuf[5];
  limstat *= (ch == 'L') ? 1 : 0;
  */

  /* 
     At this point we're sure that we've hit a limit, just return direction
     to be sure 
  */

  limstat = (optr->axis[axisno].direction) ? 2 : 1;

  Tcl_SetObjResult(interp,Tcl_NewIntObj(limstat));
  return TCL_OK;
}

int
oms_slip(ClientData clientdata, Tcl_Interp *interp,char ax) {
  omsmod * optr;
  int slipstat;
  
  optr = (omsmod *) clientdata;

  /* Simply determine whether any limit was observed */
  if ((slipstat = OMS_Slip( optr->base)) < 0) {
    Tcl_SetResult(interp,"Error reading from module",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Mask for the limit in question */
  switch(ax) {
  case 'X': slipstat &= 0x01; break;
  case 'Y': slipstat &= 0x02; break; 
  case 'Z': slipstat &= 0x04; break;
  case 'T': slipstat &= 0x08; break;
  }

  Tcl_SetObjResult(interp,Tcl_NewIntObj(slipstat));
  return TCL_OK;
}

int
oms_done(ClientData clientdata, Tcl_Interp *interp,char ax) {
  omsmod * optr;
  int donestat;
  
  optr = (omsmod *) clientdata;

  /* Simply determine whether any limit was observed */
  if ((donestat = OMS_Done( optr->base)) < 0) {
    Tcl_SetResult(interp,"Error reading from module",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Mask for the limit in question */
  switch(ax) {
  case 'X': donestat &= 0x01; break;
  case 'Y': donestat &= 0x02; break; 
  case 'Z': donestat &= 0x04; break;
  case 'T': donestat &= 0x08; break;
  }

  Tcl_SetObjResult(interp,Tcl_NewIntObj(donestat));
  return TCL_OK;
}

/*

  User I/O:
           Fault    : Bits 0-3 (input)
	   Run/Reset: Bits 4-7 (output)
	   Enable   : Bits 8-11(output)
 */

int
oms_fault(ClientData clientdata, Tcl_Interp *interp,char ax) {
  omsmod * optr;
  int iostat;
  
  optr = (omsmod *) clientdata;

  /* Simply determine whether any limit was observed */
  if ((iostat = OMS_IO( optr->base)) < 0) {
    Tcl_SetResult(interp,"Error reading from module",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Mask for the limit in question */
  switch(ax) {
  case 'X': iostat &= 0x01; break;
  case 'Y': iostat &= 0x02; break; 
  case 'Z': iostat &= 0x04; break;
  case 'T': iostat &= 0x08; break;
  }

  Tcl_SetObjResult(interp,Tcl_NewIntObj(iostat));
  return TCL_OK;
}

int
oms_enabled(ClientData clientdata, Tcl_Interp *interp,int axisno) {
  omsmod * optr;
  long addr;
  unsigned short sch;
  unsigned char ch;
  
  optr = (omsmod *) clientdata;

  addr = optr->base + USER2REG - 1;
  if (_vme_read(VME_A16, addr, &sch, 1, VME_D16) < 0) return TCL_ERROR;
  ch = (unsigned char) (0x00ff & sch);

  /* Mask for the limit in question */
  switch(axisno) {
  case 0: ch &= 0x01; break;
  case 1: ch &= 0x02; break; 
  case 2: ch &= 0x04; break;
  case 3: ch &= 0x08; break;
  }

  optr->axis[axisno].enable = (ch) ? 1 : 0;
  Tcl_SetObjResult(interp,Tcl_NewIntObj(optr->axis[axisno].enable));
  return TCL_OK;
}

int oms_onhome(ClientData clientdata, Tcl_Interp *interp,char ax)
{
  omsmod * optr;
  int homestat, axisno;
  int onval, offval;

  onval = 1;
  offval = 0;
  optr = (omsmod *) clientdata;

  /* Simply determine whether any limit was observed */
  if ((homestat = OMS_OnHome( optr->base)) < 0) {
    Tcl_SetResult(interp,"Error reading from module",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Mask for the limit in question */
  switch(ax) {
  case 'X': axisno = 0; homestat &= 0x01; break;
  case 'Y': axisno = 1; homestat &= 0x02; break; 
  case 'Z': axisno = 2; homestat &= 0x04; break;
  case 'T': axisno = 3; homestat &= 0x08; break;
  }
  
  if (optr->axis[axisno].homeparity) {
    onval = 0;
    offval = 1;
  }
  optr->axis[axisno].athome = (homestat) ? onval : offval;
  
  Tcl_SetObjResult(interp,Tcl_NewIntObj(optr->axis[axisno].athome));
  return TCL_OK;
}

int
oms_status(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval;
  int lstr, axisno;

  if (objc < 2) {
    Tcl_SetResult(interp,"Syntax: omsn status <axis> <parameter>",TCL_STATIC);
    return TCL_ERROR;
  }

  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];    
  if (oms_axis(interp,ax,&ax, &axisno) != TCL_OK) return TCL_ERROR;
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  if (!strncmp("moving",strval,lstr)) {
    return oms_moving(clientdata,interp,ax);
  } else if (!strncmp("limits",strval,lstr)) {
    return oms_limits(clientdata,interp,ax);
  } else if (!strncmp("done",strval,lstr)) {
    return oms_done(clientdata,interp,ax);
  } else if (!strncmp("slip",strval,ax)) {
    return oms_slip(clientdata,interp,ax);
  } else if (!strncmp("enabled",strval,ax)) {
    return oms_enabled(clientdata,interp,axisno);
  } else if (!strncmp("fault",strval,ax)) {
    return oms_fault(clientdata,interp,ax);
  } else if (!strncmp("info",strval,lstr)) {
    return oms_info(clientdata,interp,axisno);
  } else if (!strncmp("home",strval,lstr)) {
    return oms_onhome(clientdata,interp,ax);
  } else {
    Tcl_SetResult(interp,"Options: moving limits",TCL_STATIC);
    return TCL_ERROR;
  }
  return TCL_OK;
}

int
oms_jog(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval;
  int lstr,direction, axisno;
  omsmod * optr;

  if (objc < 2) {
    Tcl_SetResult(interp,"Syntax: omsx jog <axis> <direction>",TCL_STATIC);
    return TCL_ERROR;
  }

  direction = 0; /* No direction specified yet */
  memset(omsbuf,0,sizeof(omsbuf));
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];
  if (oms_axis(interp,ax,&ax, &axisno) != TCL_OK) return TCL_ERROR;
  if (ax == 'A') {
    Tcl_SetResult(interp,"Move one axis at a time",TCL_STATIC);
    return TCL_ERROR;
  }

  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  if (omsdebug & 1) printf("Direction=%s\n",strval);
  if (!strncmp("+",strval,lstr)) {               direction = 1;
  } else if (!strncmp("-",strval,lstr)) {        direction = -1;
  } else if (!strncmp("positive",strval,lstr)) { direction = 1;
  } else if (!strncmp("negative",strval,lstr)) { direction = -1;
  } else {
    Tcl_SetResult(interp,"Specify direction as +, -, positive, or negative",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  optr = (omsmod *) clientdata;

  /* 
     Note that the JG command will modify the topvelocity parameter in the
     controller, so we provide the topvelocity as a parameter to JG to make
     sure that we don't modify the value we want.
  */
  sprintf(omsbuf,"A%c JG%d;",optr->axis[axisno].label,
	  (direction * optr->axis[axisno].topvelocity));
  if ((lstr = OMS_talk(optr->base, omsbuf)) > 0) {
    Tcl_AppendResult(interp,omsbuf,(char *) 0);
  }
  optr->axis[axisno].direction=(direction > 0) ? 1 : 0;
  optr->axis[axisno].moving=1;
  return TCL_OK;

}

int
oms_move(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval;
  double destination;
  int lstr, pulses, axisno, res, epos, dpos, relpulse;
  char tmpbuf[80];
  omsmod * optr;

  if (objc < 2) {
    Tcl_SetResult(interp,"Syntax: omsn move <axis> <destination>",TCL_STATIC);
    return TCL_ERROR;
  }

  memset(omsbuf,0,sizeof(omsbuf));
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];
  if (oms_axis(interp,ax,&ax, &axisno) != TCL_OK) return TCL_ERROR;
  if (ax == 'A') {
    Tcl_SetResult(interp,"Move one axis at a time",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetDoubleFromObj(interp,objv[1],&destination) != TCL_OK) 
    return TCL_ERROR;
  optr = (omsmod *)clientdata;

  res = (optr->axis[axisno].encmode) ? optr->axis[axisno].encres :
    optr->axis[axisno].driveres;
  pulses = (int) ((destination * res)/ optr->axis[axisno].dscale);

  OMS_RequestUpdate(optr->base);
  vsleep(10);
  OMS_AxisPosition(optr->base, axisno, &dpos, &epos);

  relpulse = (optr->axis[axisno].encmode) ? 
    (pulses - epos) : (pulses - dpos);
  
  optr->axis[axisno].direction = (relpulse > 0) ? 1 : 0;

  sprintf(omsbuf,"A%c IC VL%d ",ax,optr->axis[axisno].topvelocity);

  if (optr->axis[axisno].encmode) {

    int basevelocity = (optr->axis[axisno].basevelocity) ? 
      optr->axis[axisno].basevelocity : 2000;

    if (optr->axis[axisno].posmaintenance) {
      sprintf(tmpbuf, "HV%d HD%d HG%d HN ",
	      basevelocity,
	      optr->axis[axisno].deadband,
	      2 * basevelocity);
      strcat(omsbuf,tmpbuf);
    } else {
#ifdef RESETPOS
      sprintf(tmpbuf,"ER%d,%d LP%d ", 
	      optr->axis[axisno].encres, 
	      optr->axis[axisno].driveres,
	      epos);
      strcat(omsbuf,tmpbuf);
#endif
    }

    if (optr->axis[axisno].stalldetection) {
      /* Set slip tolerance (ES), interrupt on stall, enable stall detection */
      sprintf(tmpbuf, "ES%d TN ",optr->axis[axisno].deadband);
      strcat(omsbuf,tmpbuf);
    } 
  }

#ifdef MOVEREL
#  ifdef AMMOVE
  switch(axisno) {
  case 0: sprintf(tmpbuf,"AM MR%d; GO ",relpulse); break;
  case 1: sprintf(tmpbuf,"AM MR,%d; GO ",relpulse); break;
  case 2: sprintf(tmpbuf,"AM MR,,%d; GO ",relpulse); break;
  case 3: sprintf(tmpbuf,"AM MR,,,%d; GO ",relpulse); break;
  default: return TCL_ERROR; 
  }

#  else
  sprintf(tmpbuf,"MR%d GO ",relpulse);
#  endif
#else
#  ifdef AMMOVE
  switch(axisno) {
  case 0: sprintf(tmpbuf,"AM MA%d; GO ",pulses); break;
  case 1: sprintf(tmpbuf,"AM MA,%d; GO ",pulses); break;
  case 2: sprintf(tmpbuf,"AM MA,,%d; GO ",pulses); break;
  case 3: sprintf(tmpbuf,"AM MA,,,%d; GO ",pulses); break;
  default: return TCL_ERROR; 
  }
#  else
  sprintf(tmpbuf,"MA%d GO ",pulses);
#  endif
#endif
  strcat(omsbuf,tmpbuf);

  if ((lstr = OMS_talk(optr->base, omsbuf)) > 0) {
    Tcl_AppendResult(interp,omsbuf,(char *) 0);
  }
  optr->axis[axisno].moving=1;

  return TCL_OK;
}

int oms_osc(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval;
  double center=0, halfamp=1, lo, hi;
  int lstr, axisno, res, lopulse, hipulse, speed;
  char tmpbuf[80];
  omsmod * optr;

  if (objc < 3) {
    Tcl_SetResult(interp,"Syntax: omsn oscillate <axis> <center> <amplitude> ?<speed>?",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  memset(omsbuf,0,sizeof(omsbuf));
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];
  if (oms_axis(interp,ax,&ax, &axisno) != TCL_OK) return TCL_ERROR;
  if (ax == 'A') {
    Tcl_SetResult(interp,"Move one axis at a time",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetDoubleFromObj(interp,objv[1],&center) != TCL_OK) 
    return TCL_ERROR;
  if (Tcl_GetDoubleFromObj(interp,objv[2],&halfamp) != TCL_OK)
    return TCL_ERROR;
  optr = (omsmod *)clientdata;
  res = (optr->axis[axisno].encmode) ? optr->axis[axisno].encres :
    optr->axis[axisno].driveres;

  speed = optr->axis[axisno].topvelocity;
  /*
  if (objc >= 3) {
    if (Tcl_GetDoubleFromObj(interp,objv[3],&vel) != TCL_OK)
      return TCL_ERROR;
    speed = (int) (vel * res);
    if (speed < optr->axis[axisno].basevelocity) {
      speed = optr->axis[axisno].basevelocity;
    }
  }
  */

  lo = center - halfamp;
  hi = center + halfamp;
  lopulse = (int) ((lo * res)/ optr->axis[axisno].dscale);
  hipulse = (int) ((hi * res)/ optr->axis[axisno].dscale);

  sprintf(omsbuf,"A%c IC VL%d ",ax,speed);
  sprintf(tmpbuf,"WH MA%d; GO MA%d; GO WG ",lopulse,hipulse);
  strcat(omsbuf,tmpbuf);

  if ((lstr = OMS_talk(optr->base, omsbuf)) > 0) {
    Tcl_SetResult(interp,"Oscillation started",TCL_STATIC);
  }
  optr->axis[axisno].moving=1;
  
  return TCL_OK;
}

int
oms_delta(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval;
  double increment;
  int lstr, pulses, axisno, res;
  char tmpbuf[80];
  omsmod * optr;

  if (objc < 2) {
    Tcl_SetResult(interp,"Syntax: omsn delta <axis> <destination>",TCL_STATIC);
    return TCL_ERROR;
  }

  memset(omsbuf,0,sizeof(omsbuf));
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];
  if (oms_axis(interp,ax,&ax, &axisno) != TCL_OK) return TCL_ERROR;
  if (ax == 'A') {
    Tcl_SetResult(interp,"Move one axis at a time",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetDoubleFromObj(interp,objv[1],&increment) != TCL_OK) 
    return TCL_ERROR;
  optr = (omsmod *)clientdata;

  res = (optr->axis[axisno].encmode) ? optr->axis[axisno].encres :
    optr->axis[axisno].driveres;
  pulses = (int) ((increment * res)/ optr->axis[axisno].dscale);

  optr->axis[axisno].direction = (pulses > 0) ? 1 : 0;

  sprintf(omsbuf,"A%c IC VL%d ",ax,optr->axis[axisno].topvelocity);

  if (optr->axis[axisno].encmode) {

    int basevelocity = (optr->axis[axisno].basevelocity) ? 
      optr->axis[axisno].basevelocity : 2000;

    if (optr->axis[axisno].posmaintenance) {
      sprintf(tmpbuf, "HV%d HD%d HG%d HN ",
	      basevelocity,
	      optr->axis[axisno].deadband,
	      2 * basevelocity);
      strcat(omsbuf,tmpbuf);
    } 

    if (optr->axis[axisno].stalldetection) {
      /* Set slip tolerance (ES), interrupt on stall, enable stall detection */
      sprintf(tmpbuf, "ES%d TN ",optr->axis[axisno].deadband);
      strcat(omsbuf,tmpbuf);
    } 
  }

  sprintf(tmpbuf,"MR%d GO ",pulses);
  strcat(omsbuf,tmpbuf);

  if ((lstr = OMS_talk(optr->base, omsbuf)) > 0) {
    Tcl_AppendResult(interp,omsbuf,(char *) 0);
  }
  optr->axis[axisno].moving=1;

  return TCL_OK;
}


int
oms_stop(ClientData clientdata, Tcl_Interp *interp,
	 int objc, Tcl_Obj * objv[]) {
  char * strval, ax;
  int lstr, axisno;
  omsmod * optr;

  memset(omsbuf,0,sizeof(omsbuf));
  if (objc) {
    strval = Tcl_GetStringFromObj(objv[0],&lstr);
    ax = strval[0];
    if (oms_axis(interp,ax,&ax, &axisno) != TCL_OK) ax = 'A';
  } else {
    ax = 'A';
  }

  if (ax == 'A') {
    sprintf(omsbuf,"SA ");
  } else {
    sprintf(omsbuf,"A%c ST ",ax); // Clear while flag as well
  }
  optr = (omsmod *) clientdata;
  if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while stopping axis",TCL_STATIC);
    return TCL_ERROR;
  }
  return TCL_OK;
}

int
oms_flush(ClientData clientdata, Tcl_Interp *interp,
	  int objc, Tcl_Obj * objv[]) {
  int retn;
  char * strval;
  omsmod * optr;
  int flshrd=0, flshwr=0, lstr;
  optr = (omsmod *) clientdata;
  if (objc) {
    strval = Tcl_GetStringFromObj(objv[0],&lstr);
    if (!strncmp(strval,"read",lstr)) {
      flshrd = 1;
    } else if (!strncmp(strval,"write",lstr)) {
      flshwr = 1;
    } else if (!strncmp(strval,"both",lstr)) {
      flshrd = flshwr = 1;
    }
  } else {
    flshrd = 1;
    flshwr =0;
  }
  if (flshrd) {
    OMS_flush_rx(optr->base); /* Flush "receive" buffer */
  }
  if (flshwr) {
    if ((retn = OMS_write_flush(optr->base)) < 0) {
      Tcl_SetResult(interp,"Error occurred flushing comm buffer",TCL_STATIC);
      return TCL_ERROR;
    }
  }

  return TCL_OK;
}

int
oms_pointers(ClientData clientdata, Tcl_Interp *interp,
	  int objc, Tcl_Obj * objv[]) {
  int retn;
  unsigned short inputput, inputget, outputput, outputget;
  omsmod * optr;
  optr = (omsmod *) clientdata;

  if ((retn = OMS_pointers(optr->base,&inputput,&inputget,&outputput,&outputget)) < 0) {
    Tcl_SetResult(interp,"Error occurred reading pointers",TCL_STATIC);
    return TCL_ERROR;
  }
  if (omsdebug) {
    printf("oms_pointers  inputput = 0x%04x\n", inputput);
    printf("oms_pointers  inputget = 0x%04x\n", inputget);
    printf("oms_pointers outputput = 0x%04x\n",outputput);
    printf("oms_pointers outputget = 0x%04x\n",outputget);
  }

  return TCL_OK;
}



int
oms_clear(ClientData clientdata, Tcl_Interp *interp,
	 int objc, Tcl_Obj * objv[]) {
  char * strval, ax;
  int lstr, axisno;
  omsmod * optr;

  memset(omsbuf,0,sizeof(omsbuf));
  if (objc) {
    strval = Tcl_GetStringFromObj(objv[0],&lstr);
    ax = strval[0];
    if (oms_axis(interp,ax,&ax, &axisno) != TCL_OK) ax = 'A';
  } else {
    ax = 'A';
  }

  if (ax == 'A') {
    Tcl_SetResult(interp,"Specify a single axis only",TCL_STATIC);
    return TCL_ERROR;
  } else {
    /* sprintf(omsbuf,"A%c CA",ax); */
    sprintf(omsbuf,"A%c IC ",ax);
  }
  optr = (omsmod *) clientdata;
  if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while clearing axis",TCL_STATIC);
    return TCL_ERROR;
  }
  return TCL_OK;
}

int oms_update(ClientData clientdata, Tcl_Interp *interp)
{
  omsmod * optr;

  optr = (omsmod *) clientdata;
  OMS_RequestUpdate(optr->base);
  return TCL_OK;
}

int
oms_register(ClientData clientdata, Tcl_Interp *interp,
	     int objc, Tcl_Obj * Objv[]) {
  unsigned int addr;
  unsigned char ch;
  unsigned short sch;
  char buf[10];
  Tcl_Obj * rptr;

  omsmod * optr;
  optr = clientdata;

  rptr = Tcl_NewObj();

  addr = optr->base + CONTROLREG - 1;
  _vme_read(VME_A16, addr, &sch, 1, VME_D16);
  ch = (unsigned char) (0x00ff & sch);
  sprintf(buf,"0x%02x",ch);
  if (omsdebug & 1) printf("Control 0x%02x\n",ch);
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj("control",7));
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj(buf,strlen(buf)));

  addr = optr->base + STATUSREG - 1;
  _vme_read(VME_A16, addr, &sch, 1, VME_D16);
  ch = (unsigned char) (0x00ff & sch);
  sprintf(buf,"0x%02x",ch);
  if (omsdebug & 1) printf("Status 0x%02x\n",ch);
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj("status",6));
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj(buf,strlen(buf)));

  addr = optr->base + SLIPREG - 1;
  _vme_read(VME_A16, addr, &sch, 1, VME_D16);
  ch = (unsigned char) (0x00ff & sch);
  sprintf(buf,"0x%02x",ch);
  if (omsdebug & 1) printf("Slip 0x%02x\n",ch);
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj("slip",4));
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj(buf,strlen(buf)));

  addr = optr->base + DONEREG - 1;
  _vme_read(VME_A16, addr, &sch, 1, VME_D16);
  ch = (unsigned char) (0x00ff & sch);
  sprintf(buf,"0x%02x",ch);
  if (omsdebug & 1) printf("Done 0x%02x\n",ch);
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj("done",4));
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj(buf,strlen(buf)));

  addr = optr->base + LIMITREG - 1;
  _vme_read(VME_A16, addr, &sch, 1, VME_D16);
  ch = (unsigned char) (0x00ff & sch);
  sprintf(buf,"0x%02x",ch);
  if (omsdebug & 1) printf("Limit 0x%02x\n",ch);
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj("limit",5));
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj(buf,strlen(buf)));

  addr = optr->base + HOMEREG - 1;
  _vme_read(VME_A16, addr, &sch, 1, VME_D16);
  ch = (unsigned char) (0x00ff & sch);
  sprintf(buf,"0x%02x",ch);
  if (omsdebug & 1) printf("Home 0x%02x\n",ch);
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj("home",4));
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj(buf,strlen(buf)));

  addr = optr->base + INTRVEC - 1;
  _vme_read(VME_A16, addr, &sch, 1, VME_D16);
  ch = (unsigned char) (0x00ff & sch);
  sprintf(buf,"0x%02x",ch);
  if (omsdebug & 1) printf("Interrupt 0x%02x\n",ch);
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj("interrupt",9));
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj(buf,strlen(buf)));

  addr = optr->base + USER1REG - 1;
  _vme_read(VME_A16, addr, &sch, 1, VME_D16);
  ch = (unsigned char) (0x00ff & sch);
  sprintf(buf,"0x%02x",ch);
  if (omsdebug & 1) printf("User1reg  0x%02x\n",ch);
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj("user1reg",8));
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj(buf,strlen(buf)));

  addr = optr->base + USER2REG - 1;
  _vme_read(VME_A16, addr, &sch, 1, VME_D16);
  ch = (unsigned char) (0x00ff & sch);
  sprintf(buf,"0x%02x",ch);
  if (omsdebug & 1) printf("User2reg  0x%02x\n",ch);
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj("user2reg",8));
  Tcl_ListObjAppendElement(interp, rptr, Tcl_NewStringObj(buf,strlen(buf)));

  Tcl_SetObjResult(interp,rptr);
  return TCL_OK;
}

void
oms_axis_init(omsmod * optr) {
  int i;
  char label[8] = "XYZTUVRS";

  for(i=0;i<NAXIS;i++) {
    optr->axis[i].label         = label[i];
    optr->axis[i].driveres      = 25000;
    optr->axis[i].encres        =  2000;
    optr->axis[i].position      =   0.0;
    optr->axis[i].dscale        =   1.0;
    optr->axis[i].bscale        =   0.0;
    optr->axis[i].vscale        =   2.0;
    optr->axis[i].ascale        =   1.0;
    optr->axis[i].acceleration  = 25000; 
    optr->axis[i].basevelocity  =     0;  
    optr->axis[i].topvelocity   = 50000;  
    optr->axis[i].deadband      =     5; /* Kinda tight, but we'll try it */
    optr->axis[i].encmode       = 0;        
    optr->axis[i].homeparity    = HOMEPARITY_HIGH;
    optr->axis[i].homeencoder   = 0;
    optr->axis[i].limits        = 1;         
    optr->axis[i].stalldetection= 0; 
    optr->axis[i].posmaintenance= 0; 
    optr->axis[i].enctracking   = 0;
    optr->axis[i].enable        = 0;
    optr->axis[i].direction     = 1;         
    optr->axis[i].fault         = 0;
    optr->axis[i].athome        = 0;
    optr->axis[i].moving        = 0;
  }
}

int
oms_axis_option_parse(Tcl_Interp * interp,
		      int objc, Tcl_Obj * objv[], 
		      omsaxis * ax) {
  int i, lstr, optint;
  char * option;
  double optdbl;
  for (i=0;i<objc;i = i + 2) {
    if (objc < i + 1) break;  
    option = Tcl_GetStringFromObj(objv[i],&lstr);
    if (!strncmp("-driveres",option, lstr)) {
      if (Tcl_GetIntFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      if (optint >= 0) { ax->driveres = optint; }
    } else if (!strncmp("-encres",option,lstr)) {
      if (Tcl_GetIntFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      if (optint >= 0) { 
	ax->encres = optint; 
      }
    } else if (!strncmp("-dscale",option,lstr)) {
      if (Tcl_GetDoubleFromObj(interp,objv[i+1],&optdbl) != TCL_OK) 
	return TCL_ERROR;
      if (optdbl >= 0) { 
	ax->dscale = optdbl; 
      }
    } else if (!strncmp("-bscale",option,lstr)) {
      if (Tcl_GetDoubleFromObj(interp,objv[i+1],&optdbl) != TCL_OK) 
	return TCL_ERROR;
      if (optdbl >= 0) { 
	ax->bscale = optdbl; 
      }
    } else if (!strncmp("-vscale",option,lstr)) {
      if (Tcl_GetDoubleFromObj(interp,objv[i+1],&optdbl) != TCL_OK) 
	return TCL_ERROR;
      if (optdbl >= 0) { 
	ax->vscale = optdbl; 
      }
    } else if (!strncmp("-ascale",option,lstr)) {
      if (Tcl_GetDoubleFromObj(interp,objv[i+1],&optdbl) != TCL_OK) 
	return TCL_ERROR;
      if (optdbl >= 0) { 
	ax->ascale = optdbl; 
      }
    } else if (!strncmp("-acceleration",option,lstr)) {
      if (Tcl_GetIntFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      if (optint >= 0) { 
	ax->acceleration = optint; 
      }
    } else if (!strncmp("-basevelocity",option,lstr)) {
      if (Tcl_GetIntFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      if (optint >= 0) { ax->basevelocity = optint; }
    } else if (!strncmp("-topvelocity",option,lstr)) {
      if (Tcl_GetIntFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      if (optint >= 0) { ax->topvelocity = optint; }
    } else if (!strncmp("-deadband",option,lstr)) {
      if (Tcl_GetIntFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      if (optint >= 0) { ax->deadband = optint; }
    } else if (!strncmp("-encmode",option,lstr)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->encmode = (optint) ? 1 : 0;
    } else if (!strncmp("-stalldetection",option,lstr)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->stalldetection = (optint) ? 1 : 0;
    } else if (!strncmp("-posmaintenance",option,lstr)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->posmaintenance = (optint) ? 1 : 0;
    } else if (!strncmp("-limits",option,lstr)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->limits = (optint) ? 1 : 0;
    } else if (!strncmp("-homeparity",option,lstr)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->homeparity = (optint) ? 1 : 0;
    } else if (!strncmp("-homeencoder",option,lstr)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->homeencoder = (optint) ? 1 : 0;
    } else if (!strncmp("-enable",option,lstr)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->enable = (optint) ? 1 : 0;
    }
  }

  if (ax->stalldetection && !ax->encmode) ax->stalldetection = 0;
  if (ax->posmaintenance && !ax->encmode) ax->posmaintenance = 0;

  return TCL_OK;
}

int
oms_axis_option_report(Tcl_Interp * interp,
		       omsaxis ax) {
  Tcl_Obj * rptr, * iptr;
  rptr = Tcl_NewObj();

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-label",6));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj(&ax.label,1));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-driveres",9));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewIntObj(ax.driveres));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-encres",7));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewIntObj(ax.encres));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-deadband",9));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewIntObj(ax.deadband));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-dscale",7));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewDoubleObj(ax.dscale));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-bscale",7));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewDoubleObj(ax.bscale));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-vscale",7));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewDoubleObj(ax.vscale));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-ascale",7));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewDoubleObj(ax.ascale));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-deadband",9));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewIntObj(ax.deadband));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-encmode",8));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewBooleanObj(ax.encmode));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-limits",7));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewBooleanObj(ax.limits));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-stalldetection",15));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewBooleanObj(ax.stalldetection));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-posmaintenance",15));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewBooleanObj(ax.posmaintenance));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-enable",7));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewBooleanObj(ax.enable));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-homeparity",11));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewBooleanObj(ax.homeparity));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-homeencoder",12));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewBooleanObj(ax.homeencoder));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  Tcl_SetObjResult(interp,rptr);
  return TCL_OK;
}

/* Configure all elements of an axis */
int
oms_axis_configure(Tcl_Interp * interp, omsmod * optr, int axno) {
  char tmpbuf[80];
  int lstr, res, epos, dpos;
  
  OMS_RequestUpdate(optr->base);
  vsleep(10);
  if (OMS_AxisPosition( optr->base, axno, &dpos, &epos) < 0) {
    Tcl_SetResult(interp,"Error occurred while reading position",TCL_STATIC);
    return TCL_ERROR;
  }

  /* All configuration is to be done on the indicated axis */
  sprintf(omsbuf,"A%c ",optr->axis[axno].label);

  /* Make sure we keep our direction the same as the controller */
  if (optr->axis[axno].direction) {
    strcpy(tmpbuf,"MP ");
  } else {
    strcpy(tmpbuf,"MM ");
  }
  strcat(omsbuf,tmpbuf);
  if (optr->axis[axno].encmode) {
    /* May need to also set encoder tracking (ET) on */
    sprintf(tmpbuf,"ER%d,%d LP%d; ", 
	    optr->axis[axno].encres, 
	    optr->axis[axno].driveres,
	    epos);
    res = optr->axis[axno].encres; 
  } else {
    sprintf(tmpbuf,"ER1,1 LP%d ", dpos);
    res = optr->axis[axno].driveres;
  }
  strcat(omsbuf,tmpbuf);
  if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }

  optr->axis[axno].acceleration = (int) (optr->axis[axno].ascale * res);
  optr->axis[axno].basevelocity = (int) (optr->axis[axno].bscale * res);
  optr->axis[axno].topvelocity = (int) (optr->axis[axno].vscale * res);
  if (optr->axis[axno].basevelocity >= optr->axis[axno].topvelocity) 
    optr->axis[axno].basevelocity = optr->axis[axno].topvelocity - 1;

  /* Position Maintenance */
  if (optr->axis[axno].posmaintenance) {
    int basevelocity = (optr->axis[axno].basevelocity) ? 
      optr->axis[axno].basevelocity : 2000;
    sprintf(omsbuf, "A%c HV%d HD%d HG%d HN",
	    optr->axis[axno].label,
	    basevelocity,
	    optr->axis[axno].deadband,
	    2 * basevelocity);
  } else {
    sprintf(omsbuf, "A%c HF",optr->axis[axno].label);
  }
  if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }
  vsleep(50);

  /* Need to set slip tolerance */
  if (optr->axis[axno].stalldetection) {
    /* Set slip tolerance (ES), interrupt on stall, enable stall detection */
    sprintf(omsbuf, "A%c ES%d TN ", optr->axis[axno].label,
	    optr->axis[axno].deadband);
  } else {
    sprintf(omsbuf, "A%c TF ",optr->axis[axno].label);
  }
  if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }
  vsleep(50);

  sprintf(omsbuf,"A%c AC%d ",optr->axis[axno].label,
	  optr->axis[axno].acceleration);
  if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }

  sprintf(omsbuf,"VL%d ",optr->axis[axno].topvelocity);
  if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }

  sprintf(omsbuf,"VB%d ",optr->axis[axno].basevelocity);
  if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Insert wait state */
  vsleep(50);

  if (optr->axis[axno].limits) {
    sprintf(omsbuf,"A%c HH LN",optr->axis[axno].label);
  } else {
    sprintf(omsbuf,"A%c LF",optr->axis[axno].label);
  }
  if (optr->axis[axno].homeparity == 1) {
    strcat(omsbuf," HH");
  } else {
    strcat(omsbuf," HL");
  }

  if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while changing limit state",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  res = 8 + axno;
  if (optr->axis[axno].enable) {
    sprintf(omsbuf,"BH%d; ",res);
  } else {
    sprintf(omsbuf,"BL%d; ",res);
  }
  if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while changing enable state",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  Tcl_SetResult(interp,"Axis configured",TCL_STATIC);
  return TCL_OK;
}

int
oms_enable(ClientData clientdata, Tcl_Interp *interp,
	      int objc, Tcl_Obj * objv[], int state) {
  omsmod * optr;
  char ax, * strval;
  int lstr, axisno, res;
  
  if (objc < 2) {
    Tcl_SetResult(interp,"Specify axis: X, Y, Z, or T",TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];    
  if (oms_axis(interp,ax,&ax, &axisno) != TCL_OK) return TCL_ERROR;

  if (axisno < 0) { 
    Tcl_SetResult(interp,"Specify single axis",TCL_STATIC);
    return TCL_ERROR;
  }

  optr = clientdata;

  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  if (!strncmp(strval,"axis",lstr)) {
    optr->axis[axisno].enable = (state) ? 1 : 0;
    res = 8 + axisno;
    if (state) {
      sprintf(omsbuf,"BH%d;",res);
    } else {
      sprintf(omsbuf,"BL%d;",res);
    }
  } else if (!strncmp(strval,"limits",lstr)) {
    optr->axis[axisno].limits = (state) ? 1 : 0;
    if (state) {
      sprintf(omsbuf,"A%c LN",optr->axis[axisno].label);
    } else {
      sprintf(omsbuf,"A%c LF",optr->axis[axisno].label);
    }
  } else {
    Tcl_SetResult(interp,"options: axis limits",TCL_STATIC);
    return TCL_ERROR;
  }
  
  if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while changing state",TCL_STATIC);
    return TCL_ERROR;
  }  

  return TCL_OK;
}

int
oms_home(ClientData clientdata, Tcl_Interp *interp,
	      int objc, Tcl_Obj * objv[]) {
  omsmod * optr;
  char ax, * strval, sense[5], howhome[24];
  int lstr, axisno, direction, speed, res, homepulse;
  
  if (objc < 2) {
    Tcl_SetResult(interp,"Specify axis: X, Y, Z, or T",TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];    
  if (oms_axis(interp,ax,&ax, &axisno) != TCL_OK) return TCL_ERROR;

  if (axisno < 0) { 
    Tcl_SetResult(interp,"Specify single axis",TCL_STATIC);
    return TCL_ERROR;
  }

  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  if (omsdebug & 1) printf("Direction=%s\n",strval);
  if (!strncmp("+",strval,lstr)) {               direction = 1;
  } else if (!strncmp("-",strval,lstr)) {        direction = 0;
  } else if (!strncmp("positive",strval,lstr)) { direction = 1;
  } else if (!strncmp("negative",strval,lstr)) { direction = 0;
  } else {
    Tcl_SetResult(interp,"Specify direction as +, -, positive, or negative",
                  TCL_STATIC);
    return TCL_ERROR;
  }
  optr = (omsmod *) clientdata;
  

  /* Note: parameter supplied to HM or HR is the position to which the
     counter will be initialized once the home position has been established */
  if (optr->axis[axisno].homeencoder == 1) {
    strcpy(howhome,"HE");
  } else {
    strcpy(howhome,"HS");
  }

  if (optr->axis[axisno].homeparity == 1) {
    strcpy(sense,"HH");
  } else {
    strcpy(sense,"HL");
  }

  speed = optr->axis[axisno].basevelocity + 1;
  res = (optr->axis[axisno].encmode) ? optr->axis[axisno].encres :
    optr->axis[axisno].driveres;
  homepulse = (int) ((optr->axis[axisno].homeposition * res)/ 
		     optr->axis[axisno].dscale);
  /*
    Note: to home without resetting position counter, use KM and KR
   */
  if (direction) {
    sprintf(omsbuf,"A%c %s %s VL%d HM%d ",ax,sense,howhome,speed,homepulse);
  } else {
    sprintf(omsbuf,"A%c %s %s VL%d HR%d ",ax,sense,howhome,speed,homepulse);
  }
  
  if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while starting home",TCL_STATIC);
    return TCL_ERROR;
  }  

  optr->axis[axisno].direction=direction;
  optr->axis[axisno].moving=1;
  return TCL_OK;
}

int
oms_conf(ClientData clientdata, Tcl_Interp *interp,
	      int objc, Tcl_Obj * objv[]) {
  omsmod * optr;
  char ax, * strval;
  int lstr, axisno;
  

  optr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Specify axis",TCL_STATIC);
    return TCL_ERROR;
  }

  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];    
  if (oms_axis(interp,ax,&ax, &axisno) != TCL_OK) return TCL_ERROR;

  if (axisno < 0) { 
    Tcl_SetResult(interp,"Specify single axis",TCL_STATIC);
    return TCL_ERROR;
  }

  if (objc == 1) {
    /* Print current configuration */
    return oms_axis_option_report(interp, optr->axis[axisno]);
  }
  
  if (oms_axis_option_parse(interp,objc-1,objv+1,&(optr->axis[axisno])) 
      != TCL_OK) return TCL_ERROR;
  return oms_axis_configure(interp,optr,axisno);
}

int
oms_command(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char * strval;
  int lstr;
  omsmod * optr;

  optr = clientdata;
  if (objc < 2) {
    Tcl_SetResult(interp,
		  "Options: status kill stop move position configure jog register raw version",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  lstr = strlen(strval);

  /* Process argument */
  if (!strncmp(strval,"status",lstr)) {
    return oms_status(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"kill",lstr)) {
    if ((lstr = OMS_Kill(optr->base)) < 0) {
      Tcl_SetResult(interp,"Error occurred while sending kill message",
		    TCL_STATIC);
      return TCL_ERROR;
    }
  } else if (!strncmp(strval,"stop",lstr)) {
    return oms_stop(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"update",lstr)) {
    return oms_update(clientdata, interp);
  } else if (!strncmp(strval,"move",lstr)) {
    return oms_move(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"delta",lstr)) {
    return oms_delta(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"position",lstr)) {
    return oms_position(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"register",lstr)) {
    return oms_register(clientdata, interp, objc-2, objv+2);
  } else if (!strncmp(strval,"configure",lstr)) {
    return oms_conf(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"jog",lstr)) {
    return oms_jog(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"clear",lstr)) {
    return oms_clear(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"flush",lstr)) {
    return oms_flush(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"enable",lstr)) {
    return oms_enable(clientdata,interp,objc-2,objv+2,1);
  } else if (!strncmp(strval,"disable",lstr)) {
    return oms_enable(clientdata,interp,objc-2,objv+2,0);
  } else if (!strncmp(strval,"home",lstr)) {
    return oms_home(clientdata,interp,objc-2,objv+2);    
  } else if (!strncmp(strval,"raw",lstr)) {
    return oms_raw(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"oscillate",lstr)) {
    return oms_osc(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"pointers",lstr)) {
    return oms_pointers(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"debug",lstr)) {
    if (objc < 3) {
      Tcl_SetObjResult(interp,Tcl_NewIntObj(omsdebug));
      return TCL_OK;
    }
    int ival,retn;
    if ((retn = Tcl_GetIntFromObj(interp,objv[2],&ival)) != TCL_OK) 
      return retn;
    omsdebug = ival;
    return TCL_OK;
  } else if (!strncmp(strval,"reset",lstr)) {
    memset(omsbuf,0,sizeof(omsbuf));
    strcpy(omsbuf,"RS");
    if ((lstr = OMS_talk(optr->base,omsbuf)) < 0) {
      Tcl_SetResult(interp,"Error occurred while resetting module",TCL_STATIC);
      return TCL_ERROR;
    }
    OMS_flush_tx(optr->base);
    OMS_flush_rx(optr->base);
    Tcl_SetObjResult(interp,Tcl_NewStringObj("Module reset",12));
  } else if (!strncmp(strval,"version",lstr)) {
    Tcl_SetObjResult(interp,Tcl_NewStringObj(cvsid,strlen(cvsid)));
    return TCL_OK;
  } else {
    Tcl_SetResult(interp,
		  "Options: status kill stop move position configure jog register raw version",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/* Top level TCL wrapper */
int 
oms(ClientData clientdata, Tcl_Interp *interp,
           int objc, Tcl_Obj * objv[]){
  char * strval, motname[80];
  int lstr;
  int address;
  omsmod * optr;

  if (objc < 3) {
    Tcl_SetResult(interp,
		  "Usage: oms create address",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  if (strcmp("create",strval)) {
    Tcl_SetResult(interp, "Options: create", TCL_STATIC);
    return TCL_ERROR;
  }

  /* Get base address */
  if (Tcl_GetIntFromObj(interp,objv[2],&address) != TCL_OK) return TCL_ERROR;

  sprintf(motname,"oms%d",ocounter);
  ocounter++;
  optr = (omsmod *) ckalloc(sizeof(omsmod));
  memset(optr,0,sizeof(omsmod));
  optr->base = address;
  /* Configure the axes */
  oms_axis_init(optr);

  Tcl_CreateObjCommand(interp,motname,(Tcl_ObjCmdProc *) oms_command,
		       (ClientData) optr,NULL);
  Tcl_SetObjResult(interp,Tcl_NewStringObj(motname,strlen(motname)));
  return TCL_OK;
}
