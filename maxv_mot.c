static char cvsid[] = "$Id: maxv_mot.c,v 1.23 2016/08/25 16:56:22 nickm Exp $";
/*
 * maxv_mot - code for controlling Oregon Micro Systems MAXv motor controller
 * 

 * maxv create [base address]
 *
 * Created command has the following syntax:
 *

maxv0 raw    <string>
maxv0 kill
maxv0 register
maxv0 move      <axis> <destination>
maxv0 jog       <axis> <direction>
maxv0 position  <axis> ?position?
maxv0 configure <axis> -driveres       #
                      -encres         #
                      -acceleration   #
                      -basevel        #
                      -topvel         #
		      -deadband       #            deadband in encoder pulses
                      -limits         (on/off)
                      -encmode        (on/off)     position maintenance
                      -stalldetection (on/off)
		      -posmaintenance (on/off)
maxv0 status    <axis> moving
                      limits
                      global
                      fault
		      enabled

maxv0 enable    <axis> facility
maxv0 disable   <axis> facility

General purpose I/O Configuration
Set bit direction:              BDff00;
IO0-IO7   Fault  input   Query: BX   
IO8-IO15  Enable output  Query: BX   Set: BHx or BLx
AUXX-AUXS Enable output  Query: ?AB  Set: ANx or AFx

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
#include "oms_maxv.h"
#include "vme_util.h"

#define USE_AUX 1
//#define AUTOPOWER 1
#define REGDONE 1
static int  mcounter;
extern int  maxvdebug;

void pbin(long val, int len, char * trailer){
    int j,bit;
    printf("-->");
    for(j=len;j>0;j--){
        bit = ((val & (0x00000001 << (j-1))) ? 1 : 0);
        if (!(j%4)) printf(" ");
        printf("%d",bit);
    }
    printf("%s",trailer);
}


void MAXv_axis_init(maxvmod * mptr) {
  int i;
  char label[8] = "XYZTUVRS";

  for(i=0;i<NAXIS;i++) {
    mptr->axis[i].label         = label[i];
    mptr->axis[i].driveres      = 25000;
    mptr->axis[i].encres        =  5000;
    mptr->axis[i].position      =   0.0;
    mptr->axis[i].dscale        =   1.0;
    mptr->axis[i].bscale        =   0.0;
    mptr->axis[i].vscale        =   2.0;
    mptr->axis[i].ascale        =   1.0;
    mptr->axis[i].acceleration  = 25000; 
    mptr->axis[i].basevelocity  =     0;  
    mptr->axis[i].topvelocity   = 50000;  
    mptr->axis[i].deadband      =     5; /* Kinda tight, but we'll try it */
    mptr->axis[i].is_servo      = 0;

    /* PID parameters for servo controllers */
    mptr->axis[i].kp = 32.0;
    mptr->axis[i].ki = 1.0;
    mptr->axis[i].kd = 100.0;
    mptr->axis[i].ka = 2.0;

    mptr->axis[i].enable_high   = 1; /* Enable is high-true */
    mptr->axis[i].encmode       = 0;        
    mptr->axis[i].homeparity    = 1;
    mptr->axis[i].limitparity   = 0;
    mptr->axis[i].homeencoder   = 0;
    mptr->axis[i].limits        = 1;         
    mptr->axis[i].stalldetection= 0; 
    mptr->axis[i].posmaintenance= 0; 
    mptr->axis[i].enctracking   = 0;
    mptr->axis[i].enable        = 0;
    mptr->axis[i].direction     = 1;         
    mptr->axis[i].fault         = 0;
    mptr->axis[i].athome        = 0;
    mptr->axis[i].moving        = 0;
  }
}

int MAXv_axis(Tcl_Interp *interp, char in, char *out, int * axisno) {

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
  case '4':
  case 'U': *out = 'U'; *axisno = 4; break;
  case '5':
  case 'V': *out = 'V'; *axisno = 5; break;
  case '6':
  case 'R': *out = 'R'; *axisno = 6; break;
  case '7':
  case 'S': *out = 'S'; *axisno = 7; break;
  case 'A': *out = 'A'; *axisno = -1; break; /* All axes */
  default:
    Tcl_SetResult(interp,
		  "Valid axes: X Y Z T U V R S (or 0 1 2 3 4 5 6 7)",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  return TCL_OK;
}

int MAXv_pointers(ClientData clientdata, Tcl_Interp *interp,
		  int objc, Tcl_Obj * objv[]) {
  int retn;
  unsigned int response_insert, response_process;
  unsigned int command_insert, command_process;
  maxvmod * mptr;
  mptr = (maxvmod *) clientdata;

  if ((retn = MAXv_getpointers(mptr->base,&response_insert,&response_process,
			       &command_insert, &command_process)) < 0) {
    Tcl_SetResult(interp,"Error occurred reading pointers",TCL_STATIC);
    return TCL_ERROR;
  }
  if (maxvdebug&2) {
    printf("MAXv_pointers response_ins  = 0x%04x\n",response_insert);
    printf("MAXv_pointers response_proc = 0x%04x\n",response_process);
    printf("MAXv_pointers command_ins   = 0x%04x\n",command_insert);
    printf("MAXv_pointers command_proc  = 0x%04x\n",command_process);
  }

  return TCL_OK;
}

int MAXv_register(ClientData clientdata, Tcl_Interp *interp)
{
  unsigned int stat;
  maxvmod * mptr;

  mptr = (maxvmod *) clientdata;
  stat = MAXv_GlobalStatus(mptr->base);
  printf("MAXv_register :"); pbin(stat,32,"\n"); fflush(stdout);
    
  Tcl_SetObjResult(interp,Tcl_NewIntObj(stat));
  return TCL_OK;
}

int MAXv_enabled(ClientData clientdata, Tcl_Interp *interp, int axno)
{
  maxvmod * mptr;
  char cmdbuf[80],response[80],*ptr;
  int len,ival,retn;
  int enabled;

  mptr = (maxvmod *) clientdata;
  /*
  if ((retn = MAXv_RegisterStatus(mptr->base, MAXV_GPIO_STAT, &ival)) == 0) {
    if (maxvdebug&8) printf("MAXv_enabled: GPIO = 0x%08x\n",ival); 

    enabled = (ival & (0x1 << (8 + axno))) ? 0 : 1;
    Tcl_SetObjResult(interp,Tcl_NewIntObj(enabled));
    return TCL_OK;
  }
  */

#ifdef USE_AUX
  sprintf(cmdbuf,"A%c AB?",mptr->axis[axno].label);
#else
  sprintf(cmdbuf,"BX; ");
#endif
  if ((len = MAXv_SendAndGetString(mptr->base,cmdbuf,response)) > 0) {
    //printf("MAXv_enabled (0x%04x): %s\n",mptr->base,response);
    if (strlen(response) < 4) {
      Tcl_SetResult(interp,"MAXv_Enabled() - Short response from indexer",
		    TCL_STATIC);
      return TCL_ERROR;
    }
#ifdef USE_AUX

    if ((ptr = strstr(response,"=h")) != NULL) {
      //enabled = 1;
      enabled = (mptr->axis[axno].enable_high) ? 1 : 0;
    } else {
      //enabled = 0;
      enabled = (mptr->axis[axno].enable_high) ? 0 : 1;
    }
#else
    int ival;
    ival = strtoul(response,&ptr,16);
    enabled = (ival & (0x1 << (8+axno))) ? 1 : 0;
    //if (!mptr->axis[axno].enable_high) {
    //  enabled = (enabled) ? 0 : 1;
    //}
#endif
    Tcl_SetObjResult(interp,Tcl_NewIntObj(enabled));
    return TCL_OK;
  }

  Tcl_SetResult(interp,"MAXv_Enabled() - Error communicating with indexer",TCL_STATIC);
  return TCL_ERROR;
}
		 
int MAXv_fault(ClientData clientdata,Tcl_Interp * interp, int axno)
{
  maxvmod * mptr;
  char cmdbuf[80],response[80],*ptr;
  int len, retn;
  int faulted;
  unsigned int ival;

  mptr = (maxvmod *) clientdata;
  if ((retn = MAXv_RegisterStatus(mptr->base, MAXV_GPIO_STAT, &ival)) == 0) {
    if (maxvdebug&8) printf("MAXv_fault: GPIO(%0x04x) = 0x%08x\n",mptr->base,ival); 
    faulted = (ival & (0x1 << (axno))) ? 0 : 1;
    // Short circuit - NCM 4/12/11
    // faulted = 0;
    Tcl_SetObjResult(interp,Tcl_NewIntObj(faulted));
    return TCL_OK;
  }

  /*
  mptr = (maxvmod *) clientdata;
  sprintf(cmdbuf,"BX");
  if ((len = MAXv_SendAndGetString(mptr->base,cmdbuf,response)) > 0) {
    int ival;
    ival = strtoul(response,&ptr,16);
    faulted = (ival & (0x1 << (axno))) ? 1 : 0;
    Tcl_SetObjResult(interp,Tcl_NewIntObj(faulted));
    return TCL_OK;
  }
  */
  return TCL_ERROR;
}

/*
 * Motion status - we have to maintain our own motion status
 *                 1) Set a moving[] flag if we initiate motion
 *                 2) Check the DONE register periodically to see when to clear
 *
 */
int MAXv_moving(ClientData clientdata, Tcl_Interp *interp,char ax) 
{
  int moving,i,j,len,ival;
  maxvmod * mptr;
  unsigned int addr;
  int overtravel[8], slip[8], done[8];
  char cmdbuf[80], response[80];
  
  mptr = (maxvmod *) clientdata;
  
  memset(response,0,sizeof(response));

  // Determine moving status by reading axis DONE flags
  //   - DONE flags set on initialization and cleared before each move
  //   - DONE flags should be clear if moving and set if not
#ifdef REGDONE
  addr = mptr->base + MAXV_STAT1_F;
  if (_vme_read(VME_A16, addr, &ival, 1, VME_D32) < 0) {
      return -1;
  }
  if (maxvdebug&1) {
    printf("MAXV_STAT1_F (RD): ");
    pbin(ival,32,"\n");
    fflush(stdout);
  }

  for (i=0;i<8;i++) {
    done[i]      = (ival & (0x000001 << i)) ? 1 : 0;
    overtravel[i]= (ival & (0x000100 << i)) ? 1 : 0;
    slip[i]      = (ival & (0x010000 << i)) ? 1 : 0;
    if (mptr->axis[i].moving) {
      if (done[i]) {
	mptr->axis[i].moving = 0;
      } else if (overtravel[i]) {
	printf("  DONE NOT SET; OVERTRAVEL SET [%d]\n",i);
	mptr->axis[i].moving = 0;
      } else if (slip[i]) {
	printf("  DONE NOT SET; SLIP SET [%d]\n",i);
	mptr->axis[i].moving = 0;
      }
      // Else the axis is still moving
    } 
  }

  // Reset done flags

  if (maxvdebug&1) {
    printf(" Moving: ");
    for(i=0;i<8;i++) {
      printf("%d ",mptr->axis[i].moving);
    }
    printf("\n");
    fflush(stdout);
  }

  switch (ax) {
  case 'X': j=0; moving = mptr->axis[0].moving; break;
  case 'Y': j=1; moving = mptr->axis[1].moving; break;
  case 'Z': j=2; moving = mptr->axis[2].moving; break;
  case 'T': j=3; moving = mptr->axis[3].moving; break;
  case 'U': j=4; moving = mptr->axis[4].moving; break;
  case 'V': j=5; moving = mptr->axis[5].moving; break;
  case 'R': j=6; moving = mptr->axis[6].moving; break;
  case 'S': j=7; moving = mptr->axis[7].moving; break;
  case 'A': 
    moving = 0;
    j=-1;
    for (i=0;i<8;i++) {
      moving += (0x1 << i) * mptr->axis[i].moving;
    }
    break;
  }
  // Reset flags
  ival=0;
  if (j<0) {
    for (i=0;i<8;i++) {
      ival += (overtravel[i] && (done[i]==0)) ? (0x01<<i) : 0;
      ival += (overtravel[i])? (0x100<<i) : 0;
    }
  } else {
      ival += (overtravel[j] && (done[j]==0)) ? (0x01<<j) : 0;
      ival += (overtravel[j])? (0x100<<j) : 0;
  }
  /*
  if (ival) {
    if (maxvdebug&1) {
      printf("MAXV_STAT1_F (WR): ");
      pbin(ival,32,"\n");
      fflush(stdout);
    }
    if (_vme_write(VME_A16, addr, &ival, 1, VME_D32) < 0) {
      return -1;
    }
  }
  */


#else
  sprintf(cmdbuf,"A%c RV",ax);
  if ((len = MAXv_SendAndGetString(mptr->base,cmdbuf,response)) < 0) {
    //Tcl_SetObjResult(interp,Tcl_NewStringObj(response,len));
    //Assume it's moving if we're in error
    moving = 1;
  } else {
    i = strtol(response,&ptr,0);
    moving = (i == 0) ? 0 : 1;
  }
#endif
  if (maxvdebug&1) printf(" Axis %c moving = %d\n",ax,moving);
  Tcl_SetObjResult(interp,Tcl_NewIntObj(moving));
  return TCL_OK;
}

int MAXv_limits(ClientData clientdata, Tcl_Interp *interp, int axisno) {
  maxvmod * mptr;
  int limstat, retn;
  int revstat;
  
  mptr = (maxvmod *) clientdata;

  if ((retn = MAXv_AxisLimits(mptr->base,axisno,&limstat))<0) return TCL_ERROR;

  if (mptr->axis[axisno].limitparity == 0) {
    revstat = (~limstat) & 0x03;
    limstat = revstat;
  } 

  Tcl_SetObjResult(interp,Tcl_NewIntObj(limstat));
  return TCL_OK;
}

int MAXv_onhome(ClientData clientdata, Tcl_Interp *interp, int axisno) {
   maxvmod * mptr;
   int onhome, retn;
  
   mptr = (maxvmod *) clientdata;
   if ((retn = MAXv_AxisOnHome(mptr->base,axisno,&onhome))<0) return TCL_ERROR;
   Tcl_SetObjResult(interp,Tcl_NewIntObj(onhome));
   return TCL_OK;

}

int MAXv_done(ClientData clientdata, Tcl_Interp *interp,char ax) {
  int done;
  maxvmod * mptr;
  unsigned int addr,ival;
  
  mptr = (maxvmod *) clientdata;
  // Determine moving status by reading axis DONE flags
  //   - DONE flags set on initialization and cleared before each move
  //   - DONE flags should be clear if moving and set if not
  addr = mptr->base + MAXV_STAT1_F;
  if (_vme_read(VME_A16, addr, &ival, 1, VME_D32) < 0) {
      return -1;
  }
  if (maxvdebug&1) {
    printf("MAXV_STAT1_F: ");
    pbin(ival,32,"\n");
    fflush(stdout);
  }

  /* Mask out proper bits */
  switch (ax) {
  case 'X': done = (ival & (0x01 << 0)) ? 1 : 0; break;
  case 'Y': done = (ival & (0x01 << 1)) ? 1 : 0; break;
  case 'Z': done = (ival & (0x01 << 2)) ? 1 : 0; break;
  case 'T': done = (ival & (0x01 << 3)) ? 1 : 0; break;
  case 'U': done = (ival & (0x01 << 4)) ? 1 : 0; break;
  case 'V': done = (ival & (0x01 << 5)) ? 1 : 0; break;
  case 'R': done = (ival & (0x01 << 6)) ? 1 : 0; break;
  case 'S': done = (ival & (0x01 << 7)) ? 1 : 0; break;
  case 'A': 
  default:
    done = ival & 0xFF;
    break;
  }

  if (maxvdebug&2) printf("done = %d\n",done);
  Tcl_SetObjResult(interp,Tcl_NewIntObj(done));
  return TCL_OK;
}

int MAXv_status(ClientData clientdata, Tcl_Interp *interp,
		int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval;
  int lstr, axisno;
  int onhome;

  if (objc < 2) {
    Tcl_SetResult(interp,"Syntax: maxvn status <axis> <parameter>",TCL_STATIC);
    return TCL_ERROR;
  }

  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];    
  if (MAXv_axis(interp,ax,&ax, &axisno) != TCL_OK) return TCL_ERROR;
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  if (!strncmp("moving",strval,lstr)) {
    return MAXv_moving(clientdata,interp,ax);
  } else if (!strncmp("limits",strval,lstr)) {
    return MAXv_limits(clientdata,interp,axisno);
  } else if (!strncmp("done",strval,lstr)) {
    return MAXv_done(clientdata,interp,ax);
  } else if (!strncmp("slip",strval,ax)) {
    //return MAXv_slip(clientdata,interp,ax);
    Tcl_SetObjResult(interp,Tcl_NewIntObj(0));
    return TCL_OK;
  } else if (!strncmp("enabled",strval,ax)) {
    return MAXv_enabled(clientdata,interp,axisno);
  } else if (!strncmp("fault",strval,ax)) {
    return MAXv_fault(clientdata,interp,axisno);
    //    Tcl_SetObjResult(interp,Tcl_NewIntObj(0));
    return TCL_OK;
  } else if (!strncmp("register",strval,lstr)) {
    return MAXv_register(clientdata,interp);
  } else if (!strncmp("home",strval,lstr)) {
    
    return MAXv_onhome(clientdata,interp,ax);
    //Tcl_SetObjResult(interp,Tcl_NewIntObj(0));
    return TCL_OK;
  } else {
    Tcl_SetResult(interp,"Options: moving limits",TCL_STATIC);
    return TCL_ERROR;
  }
  return TCL_OK;
}


int MAXv_raw(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char * strval;
  int lstr, len;
  maxvmod * mptr;
  char response[1024];

  mptr = (maxvmod *) clientdata;
  strval = Tcl_GetStringFromObj(objv[0],&lstr);

  memset(response,0,sizeof(response));
  printf("MAXv_raw() Send:\"%s\"\n",strval);

  if ((len = MAXv_SendAndGetString(mptr->base,strval,response)) > 0) {
    printf("MAXv_raw() Recv:\"%s\"\n",response);
    Tcl_SetObjResult(interp,Tcl_NewStringObj(response,len));
  }

  return TCL_OK;
}

int MAXv_rawtalk(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char * strval;
  int lstr, len;
  maxvmod * mptr;
  char response[1024];

  mptr = (maxvmod *) clientdata;
  strval = Tcl_GetStringFromObj(objv[0],&lstr);

  memset(response,0,sizeof(response));
  strcpy(response,strval);

  if ((len = MAXv_talk(mptr->base,response)) > 0) {
    Tcl_SetObjResult(interp,Tcl_NewStringObj(response,len));
  }
  return TCL_OK;
}

int MAXv_position(ClientData clientdata, Tcl_Interp *interp,
	     int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval;
  int lstr,len, axisno, pulses;
  double position;
  maxvmod * mptr;
  char cmdbuf[80], response[80];

  if (!objc) {
    Tcl_SetResult(interp,"Usage: maxvn position <axis> ?position?\n",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];
  if (MAXv_axis(interp,ax,&ax,&axisno) != TCL_OK) return TCL_ERROR;
  mptr = (maxvmod *) clientdata;

  if (objc > 1) { /* Set position */
    if (Tcl_GetDoubleFromObj(interp,objv[1],&position) != TCL_OK) 
      return TCL_ERROR;

    memset(cmdbuf,0,sizeof(cmdbuf));
    if (mptr->axis[axisno].is_servo) {
      pulses = (int) ((position/mptr->axis[axisno].dscale) * 
		      mptr->axis[axisno].encres);
      sprintf(cmdbuf,"A%c LP%d; CL1; ",
	      mptr->axis[axisno].label,
	      pulses);
    } else if (mptr->axis[axisno].encmode) {
      pulses = (int) ((position/mptr->axis[axisno].dscale) * 
		      mptr->axis[axisno].encres);
      sprintf(cmdbuf,"A%c ER%d,%d; LP%d;",
	      mptr->axis[axisno].label,
	      mptr->axis[axisno].encres,
	      mptr->axis[axisno].driveres,
	      pulses);      
    } else {
      pulses = (int) ((position/mptr->axis[axisno].dscale) * 
		      mptr->axis[axisno].driveres);
      sprintf(cmdbuf,"A%c LP%d;",mptr->axis[axisno].label,pulses);
    }

    if ((len = MAXv_SendAndGetString(mptr->base,cmdbuf,response)) > 0) {
      Tcl_SetObjResult(interp,Tcl_NewStringObj(response,len));
    }

  } else {        /* Get position */
    int epos, dpos;

    if (MAXv_AxisPosition( mptr->base, axisno, &dpos, &epos) < 0) {
      Tcl_SetResult(interp,"Error occurred while reading position",TCL_STATIC);
      return TCL_ERROR;
    }

    if (maxvdebug&4) {
      printf("MAXv_AxisPosition: axis=%d dpos=%8d epos=%8d\n",axisno,dpos,epos);
    }

    if (mptr->axis[axisno].encmode) {
      mptr->axis[axisno].position = (((double)epos)/
				     mptr->axis[axisno].encres) *
	mptr->axis[axisno].dscale;
    } else {
      mptr->axis[axisno].position = (((double)dpos)/
				     mptr->axis[axisno].driveres) *
	mptr->axis[axisno].dscale;
    }
    Tcl_SetObjResult(interp,Tcl_NewDoubleObj(mptr->axis[axisno].position));
  }

  return TCL_OK;
}

int MAXv_jog(ClientData clientdata, Tcl_Interp *interp,
	     int objc, Tcl_Obj * objv[]) 
{
  char ax;
  char * strval;
  int lstr,direction, axisno;
  char cmdbuf[256];
  maxvmod * mptr;

  if (objc < 2) {
    Tcl_SetResult(interp,"Syntax: omsx jog <axis> <direction>",TCL_STATIC);
    return TCL_ERROR;
  }

  direction = 0; /* No direction specified yet */
  memset(cmdbuf,0,sizeof(cmdbuf));
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];
  if (MAXv_axis(interp,ax,&ax, &axisno) != TCL_OK) return TCL_ERROR;
  if (ax == 'A') {
    Tcl_SetResult(interp,"Move one axis at a time",TCL_STATIC);
    return TCL_ERROR;
  }

  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  if (!strncmp("+",strval,lstr)) {               direction = 1;
  } else if (!strncmp("-",strval,lstr)) {        direction = -1;
  } else if (!strncmp("positive",strval,lstr)) { direction = 1;
  } else if (!strncmp("negative",strval,lstr)) { direction = -1;
  } else {
    Tcl_SetResult(interp,"Specify direction as +, -, positive, or negative",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  mptr = (maxvmod *) clientdata;
  /* 
     Note that the JG command will modify the topvelocity parameter in the
     controller, so we provide the topvelocity as a parameter to JG to make
     sure that we don't modify the value we want.
  */
  MAXv_ResetDoneFlag(mptr->base, axisno);
  MAXv_ResetOvertravelFlag(mptr->base,axisno);

  sprintf(cmdbuf,"A%c CA JG%d;",mptr->axis[axisno].label,
	  (direction * mptr->axis[axisno].topvelocity));
  //printf("COMMAND: %s\n",cmdbuf);
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while jogging axis",TCL_STATIC);
    return TCL_ERROR;
  }
  mptr->axis[axisno].direction=(direction > 0) ? 1 : 0;
  mptr->axis[axisno].moving=1;

  return TCL_OK;
}

int MAXv_move(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval;
  double destination;
  int lstr, pulses, axisno, res, epos, dpos, relpulse;
  char tmpbuf[80],cmdbuf[256];
  maxvmod * mptr;

  if (objc < 2) {
    Tcl_SetResult(interp,"Syntax: maxvn move <axis> <destination>",TCL_STATIC);
    return TCL_ERROR;
  }

  memset(cmdbuf,0,sizeof(cmdbuf));
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];
  if (MAXv_axis(interp,ax,&ax,&axisno) != TCL_OK) return TCL_ERROR;
  if (ax == 'A') {
    Tcl_SetResult(interp,"Move one axis at a time",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetDoubleFromObj(interp,objv[1],&destination) != TCL_OK) 
    return TCL_ERROR;
  mptr = (maxvmod *)clientdata;

  res = (mptr->axis[axisno].encmode || mptr->axis[axisno].is_servo) ? 
    mptr->axis[axisno].encres :
    mptr->axis[axisno].driveres;
  pulses = (int) ((destination * res)/ mptr->axis[axisno].dscale);

  if (MAXv_AxisPosition( mptr->base, axisno, &dpos, &epos) < 0) {
    Tcl_SetResult(interp,"Error occurred while reading position",TCL_STATIC);
    return TCL_ERROR;
  }

  relpulse = (mptr->axis[axisno].encmode) ? 
    (pulses - epos) : (pulses - dpos);
  
  mptr->axis[axisno].direction = (relpulse > 0) ? 1 : 0;

  sprintf(cmdbuf,"A%c %s IC VL%d; ",ax,
	((mptr->axis[axisno].direction)? "MP" : "MM"),
	mptr->axis[axisno].topvelocity);
  if (mptr->axis[axisno].is_servo) {
    sprintf(tmpbuf,"CL1; ");
    strcat(cmdbuf,tmpbuf);
  } else if (mptr->axis[axisno].encmode) {

    int basevelocity = (mptr->axis[axisno].basevelocity) ? 
      mptr->axis[axisno].basevelocity : 2000;

    if (mptr->axis[axisno].posmaintenance) {
      sprintf(tmpbuf, "HV%d;  HD%d;  HG%d;  CL1;  ",
	      basevelocity,
	      mptr->axis[axisno].deadband,
	      2 * basevelocity);
      strcat(cmdbuf,tmpbuf);
    } else {
      sprintf(tmpbuf, "CL0; ");
      strcat(cmdbuf,tmpbuf);
    }

    if (mptr->axis[axisno].stalldetection) {
      /* Set slip tolerance (ES), interrupt on stall, enable stall detection */
      sprintf(tmpbuf, "ES%d;  TN  ",mptr->axis[axisno].deadband);
      strcat(cmdbuf,tmpbuf);
    } 
  } 
  /*else {
    sprintf(tmpbuf, "CL0; ");
    strcat(cmdbuf,tmpbuf);
  }
  */
  MAXv_ResetDoneFlag(mptr->base, axisno);
  MAXv_ResetOvertravelFlag(mptr->base,axisno);
  MAXv_ResetSlipFlag(mptr->base,axisno);
  sprintf(tmpbuf,"MA%d; GN ",pulses);
  strcat(cmdbuf,tmpbuf);

  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while moving axis",TCL_STATIC);
    return TCL_ERROR;
  }
  mptr->axis[axisno].moving=1;

  return TCL_OK;
}

int MAXv_delta(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval;
  double increment;
  int lstr, pulses, axisno, res;
  char tmpbuf[80], cmdbuf[256];
  maxvmod * mptr;

  if (objc < 2) {
    Tcl_SetResult(interp,"Syntax: maxvn delta <axis> <destination>",TCL_STATIC);
    return TCL_ERROR;
  }

  memset(cmdbuf,0,sizeof(cmdbuf));
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];
  if (MAXv_axis(interp,ax,&ax,&axisno) != TCL_OK) return TCL_ERROR;
  if (ax == 'A') {
    Tcl_SetResult(interp,"Move one axis at a time",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetDoubleFromObj(interp,objv[1],&increment) != TCL_OK) 
    return TCL_ERROR;
  mptr = (maxvmod *)clientdata;

  res = (mptr->axis[axisno].encmode) ? mptr->axis[axisno].encres :
    mptr->axis[axisno].driveres;
  pulses = (int) ((increment * res)/ mptr->axis[axisno].dscale);

  mptr->axis[axisno].direction = (pulses > 0) ? 1 : 0;

  sprintf(cmdbuf,"A%c %s IC VL%d; ",ax,
	((pulses > 0) ? "MP" : "MM"),
	mptr->axis[axisno].topvelocity);

  if (mptr->axis[axisno].is_servo) {
    sprintf(tmpbuf,"CL1; ");
    strcat(cmdbuf,tmpbuf);
  } else if (mptr->axis[axisno].encmode) {

    int basevelocity = (mptr->axis[axisno].basevelocity) ? 
      mptr->axis[axisno].basevelocity : 2000;

    if (mptr->axis[axisno].posmaintenance) {
      sprintf(tmpbuf, "HV%d; HD%d; HG%d; CL1; ",
	      basevelocity,
	      mptr->axis[axisno].deadband,
	      2 * basevelocity);
      strcat(cmdbuf,tmpbuf);
    } 
    /*    else {
      sprintf(tmpbuf, "CL0; ");
      strcat(cmdbuf,tmpbuf);
      } */

    if (mptr->axis[axisno].stalldetection) {
      /* Set slip tolerance (ES), interrupt on stall, enable stall detection */
      sprintf(tmpbuf, "ES%d; TN ",mptr->axis[axisno].deadband);
      strcat(cmdbuf,tmpbuf);
    } 
  } else {
    sprintf(tmpbuf,"CL0; ");
    strcat(cmdbuf,tmpbuf);
  }

  MAXv_ResetDoneFlag(mptr->base, axisno);
  MAXv_ResetOvertravelFlag(mptr->base,axisno);
  MAXv_ResetSlipFlag(mptr->base,axisno);
  sprintf(tmpbuf,"MR%d; GN",pulses);
  strcat(cmdbuf,tmpbuf);

  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while moving axis",TCL_STATIC);
    return TCL_ERROR;
  }
  mptr->axis[axisno].moving=1;

  return TCL_OK;
}

// Need to figure out how to set the "done" flag
//
int MAXv_stop(ClientData clientdata, Tcl_Interp *interp,
	      int objc, Tcl_Obj * objv[]) {
  char * strval, ax, cmdbuf[80];
  int lstr, axisno;
  maxvmod * mptr;
  int i;

  memset(cmdbuf,0,sizeof(cmdbuf));
  if (objc) {
    strval = Tcl_GetStringFromObj(objv[0],&lstr);
    ax = strval[0];
    if (MAXv_axis(interp,ax,&ax, &axisno) != TCL_OK) ax = 'A';
    sprintf(cmdbuf,"A%c SD ID ",ax); // Clear done flag as well
  } else {
    ax = 'A';
    //sprintf(cmdbuf,"SA; ");
    sprintf(cmdbuf,"AA; SD; ID; ");
  }

  mptr = (maxvmod *) clientdata;
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while stopping axis",TCL_STATIC);
    return TCL_ERROR;
  }

  // Clear moving flags, assuming that all motion has concluded
  if (ax == 'A') {
    // Potentially dangerous
    for (i=0;i<8;i++) {
      mptr->axis[i].moving=0;      
    }
  }

  return TCL_OK;
}

int MAXv_home(ClientData clientdata, Tcl_Interp *interp,
		int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval,sense[5],howhome[24];
  int lstr, axisno, speed, direction,homepulse;
  char cmdbuf[256];
  maxvmod * mptr;

  if (objc < 2) {
    Tcl_SetResult(interp,"Syntax: maxvn home <axis> <direction>",TCL_STATIC);
    return TCL_ERROR;
  }

  memset(cmdbuf,0,sizeof(cmdbuf));
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];
  if (MAXv_axis(interp,ax,&ax,&axisno) != TCL_OK) return TCL_ERROR;
  if (ax == 'A') {
    Tcl_SetResult(interp,"Move one axis at a time",TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  if (!strncmp("+",strval,lstr)) {               direction = 1;
  } else if (!strncmp("-",strval,lstr)) {        direction = 0;
  } else if (!strncmp("positive",strval,lstr)) { direction = 1;
  } else if (!strncmp("negative",strval,lstr)) { direction = 0;
  } else {
    Tcl_SetResult(interp,"Specify direction as +, -, positive, or negative",
                  TCL_STATIC);
    return TCL_ERROR;
  }
  mptr = (maxvmod *)clientdata;
  if (mptr->axis[axisno].homeencoder == 1) {
    strcpy(howhome,"HE");
  } else {
    strcpy(howhome,"HS");
  }

  if (mptr->axis[axisno].homeparity == 1) {
    strcpy(sense,"HH");
  } else {
    strcpy(sense,"HL");
  }

  speed = mptr->axis[axisno].basevelocity+1;
  homepulse = 0;
  if (direction) {
    sprintf(cmdbuf,"A%c MP %s %s VL%d HM%d ",ax,sense,howhome,speed,homepulse);
  } else {
    sprintf(cmdbuf,"A%c MM %s %s VL%d HR%d ",ax,sense,howhome,speed,homepulse);
  }
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while moving axis",TCL_STATIC);
    return TCL_ERROR;
  }
  mptr->axis[axisno].moving=1;
  mptr->axis[axisno].direction=direction;
  return TCL_OK;
}

int MAXv_axis_option_parse(Tcl_Interp * interp,
			   int objc, Tcl_Obj * objv[], 
			   maxvaxis * ax) {
  int i, lstr, optint, lval;
  char * option, *value;
  double optdbl;
  if (maxvdebug&1) printf("MAXv_axis_option_parse:\n");
  for (i=0;i<objc;i = i + 2) {
    if (objc < i + 1) break;  
    option = Tcl_GetStringFromObj(objv[i],&lstr);
    value = Tcl_GetStringFromObj(objv[i+1],&lval);
    if(maxvdebug&1) printf("  %s = %s\n",option,value);
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
    } else if (!strcmp("-is_servo",option)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->is_servo = (optint) ? 1 : 0;
    } else if (!strcmp("-enable_high",option)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->enable_high = (optint) ? 1 : 0;
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
    } else if (!strncmp("-limitparity",option,lstr)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->limitparity = (optint) ? 1 : 0;
    } else if (!strncmp("-homeencoder",option,lstr)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->homeencoder = (optint) ? 1 : 0;
    } else if (!strncmp("-enable",option,lstr)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->enable = (optint) ? 1 : 0;
    } else if (!strncmp("-kp",option,lstr)) {
      if (Tcl_GetDoubleFromObj(interp,objv[i+1],&optdbl) != TCL_OK) 
	return TCL_ERROR;
      if (optdbl >= 0) { 
	ax->kp = optdbl; 
      }
    } else if (!strncmp("-ki",option,lstr)) {
      if (Tcl_GetDoubleFromObj(interp,objv[i+1],&optdbl) != TCL_OK) 
	return TCL_ERROR;
      if (optdbl >= 0) { 
	ax->ki = optdbl; 
      }
    } else if (!strncmp("-kd",option,lstr)) {
      if (Tcl_GetDoubleFromObj(interp,objv[i+1],&optdbl) != TCL_OK) 
	return TCL_ERROR;
      if (optdbl >= 0) { 
	ax->kd = optdbl; 
      }
    }
  }
  if (ax->stalldetection && !ax->encmode) ax->stalldetection = 0;
  if (ax->posmaintenance && !ax->encmode) ax->posmaintenance = 0;

  return TCL_OK;
}

int MAXv_axis_option_report(Tcl_Interp * interp,
			    maxvaxis ax) {
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
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-is_servo",9));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewBooleanObj(ax.is_servo));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-enable_high",12));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewBooleanObj(ax.enable_high));
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
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-limitparity",12));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewBooleanObj(ax.limitparity));
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

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-kp",3));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewDoubleObj(ax.kp));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-ki",3));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewDoubleObj(ax.ki));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-kd",3));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewDoubleObj(ax.kd));
  Tcl_ListObjAppendElement(interp,rptr,iptr);


  Tcl_SetObjResult(interp,rptr);
  return TCL_OK;
}

/* Configure all elements of an axis */
int MAXv_axis_configure(Tcl_Interp * interp, maxvmod * mptr, int axno) {
  char cmdbuf[80],tmpbuf[80];
  int lstr, res, epos, dpos;
  
  if (MAXv_AxisPosition( mptr->base, axno, &dpos, &epos) < 0) {
    Tcl_SetResult(interp,"Error occurred while reading position",TCL_STATIC);
    return TCL_ERROR;
  }

  /* All configuration is to be done on the indicated axis */
  /* Set high limits 10/21/09 */
  if (mptr->axis[axno].limitparity) {
    sprintf(cmdbuf,"A%c LTH ",mptr->axis[axno].label);
  } else {
    sprintf(cmdbuf,"A%c LTL ",mptr->axis[axno].label);
  }

  if (mptr->axis[axno].homeparity) {
    sprintf(tmpbuf,"HTH ");
  } else {
    sprintf(tmpbuf,"HTL ");
  }
  strcat(cmdbuf, tmpbuf);
  
  /* Make sure we keep our direction the same as the controller */
  if (mptr->axis[axno].direction) {
    strcpy(tmpbuf,"MP ");
  } else {
    strcpy(tmpbuf,"MM ");
  }
  strcat(cmdbuf,tmpbuf);
  if (mptr->axis[axno].is_servo) {
    // Set bipolar output
    // Take default PID parameters for the time being
    sprintf(tmpbuf,"PSM SVB1; LP%d; KP%.1f; KI%.1f; KD%.1f; KA%.1f; CL1; ", 
	    epos,
	    mptr->axis[axno].kp,
	    mptr->axis[axno].ki,
	    mptr->axis[axno].kd,
	    mptr->axis[axno].ka);
    res = mptr->axis[axno].encres;
  } else {
    if (mptr->axis[axno].encmode) {
      /* May need to also set encoder tracking (ET) on */
      sprintf(tmpbuf,"PSE ER%d,%d; LP%d; ", 
	      mptr->axis[axno].encres, 
	      mptr->axis[axno].driveres,
	      epos);
      res = mptr->axis[axno].encres; 
    } else {
      sprintf(tmpbuf,"PSO LP%d; ", dpos);
      res = mptr->axis[axno].driveres;
    }
  }
  strcat(cmdbuf,tmpbuf);
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }

  mptr->axis[axno].acceleration = (int) (mptr->axis[axno].ascale * res);
  mptr->axis[axno].basevelocity = (int) (mptr->axis[axno].bscale * res);
  mptr->axis[axno].topvelocity = (int) (mptr->axis[axno].vscale * res);
  if (mptr->axis[axno].basevelocity >= mptr->axis[axno].topvelocity) 
    mptr->axis[axno].basevelocity = mptr->axis[axno].topvelocity - 1;

  /* Position Maintenance */
  if (mptr->axis[axno].posmaintenance) {
    int basevelocity = (mptr->axis[axno].basevelocity) ? 
      mptr->axis[axno].basevelocity : 2000;
    sprintf(cmdbuf, "A%c HV%d HD%d HG%d CL1;",
	    mptr->axis[axno].label,
	    basevelocity,
	    mptr->axis[axno].deadband,
	    2 * basevelocity);
  } else {
    sprintf(cmdbuf, "A%c CL0;",mptr->axis[axno].label);
  }
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }
  vsleep(50);

  /* Need to set slip tolerance */
  if (mptr->axis[axno].stalldetection) {
    /* Set slip tolerance (ES), interrupt on stall, enable stall detection */
    sprintf(cmdbuf, "A%c ES%d TN ", mptr->axis[axno].label,
	    mptr->axis[axno].deadband);
  } else {
    sprintf(cmdbuf, "A%c TF ",mptr->axis[axno].label);
  }
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }
  vsleep(50);

  sprintf(cmdbuf,"A%c AC%d; ",mptr->axis[axno].label,
	  mptr->axis[axno].acceleration);
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }

  sprintf(cmdbuf,"VL%d; ",mptr->axis[axno].topvelocity);
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }

  sprintf(cmdbuf,"VB%d; ",mptr->axis[axno].basevelocity);
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Insert wait state */
  vsleep(50);

  if (mptr->axis[axno].limits) {
    sprintf(cmdbuf,"A%c LMH",mptr->axis[axno].label);
  } else {
    sprintf(cmdbuf,"A%c LMF",mptr->axis[axno].label);
  }
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while changing limit state",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  vsleep(50);

  res = 8 + axno;
  // Exclusive OR test
  if (!(mptr->axis[axno].enable ^ mptr->axis[axno].enable_high)) {
    //if (mptr->axis[axno].enable) {
#ifdef USE_AUX
    sprintf(cmdbuf,"A%c PH",mptr->axis[axno].label);
#else
    sprintf(cmdbuf,"BH%d; ",res);
#endif
  } else {
#ifdef USE_AUX
    sprintf(cmdbuf,"A%c PL",mptr->axis[axno].label);
#else
    sprintf(cmdbuf,"BL%d; ",res);
#endif
  }
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while changing enable state",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  vsleep(50);

  Tcl_SetResult(interp,"Axis configured",TCL_STATIC);
  return TCL_OK;
}

int MAXv_conf(ClientData clientdata, Tcl_Interp *interp,
	      int objc, Tcl_Obj * objv[]) {
  maxvmod * mptr;
  char ax, * strval;
  int lstr, axisno;
  

  mptr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Specify axis",TCL_STATIC);
    return TCL_ERROR;
  }

  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];    
  if (MAXv_axis(interp,ax,&ax, &axisno) != TCL_OK) return TCL_ERROR;

  if (axisno < 0) { 
    Tcl_SetResult(interp,"Specify single axis",TCL_STATIC);
    return TCL_ERROR;
  }

  if (objc == 1) {
    /* Print current configuration */
    return MAXv_axis_option_report(interp, mptr->axis[axisno]);
  }
  
  if (MAXv_axis_option_parse(interp,objc-1,objv+1,&(mptr->axis[axisno])) 
      != TCL_OK) return TCL_ERROR;
  return MAXv_axis_configure(interp,mptr,axisno);
}

int MAXv_enable(ClientData clientdata, Tcl_Interp *interp,
		int objc, Tcl_Obj * objv[], int state) {
  maxvmod * mptr;
  char ax, * strval, cmdbuf[80];
  int lstr, axisno, res;
  
  if (objc < 2) {
    Tcl_SetResult(interp,"Specify axis: X, Y, Z, T, U, V, R, or S",TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  ax = strval[0];    
  if (MAXv_axis(interp,ax,&ax, &axisno) != TCL_OK) {
    // MAXv_axis loads an error string in the interpreter
    return TCL_ERROR;
  }

  if (axisno < 0) { 
    Tcl_SetResult(interp,"Specify single axis",TCL_STATIC);
    return TCL_ERROR;
  }

  mptr = clientdata;

  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  if (!strncmp(strval,"axis",lstr)) {
    mptr->axis[axisno].enable = (state) ? 1 : 0;
    res = 8 + axisno;
    if (!(state ^ mptr->axis[axisno].enable_high)) {
    //if (mptr->axis[axisno].enable) {
#ifdef USE_AUX
      sprintf(cmdbuf,"A%c PH",mptr->axis[axisno].label);
#else
      sprintf(cmdbuf,"BH%d; ",res);
#endif
    } else {
#ifdef USE_AUX
      sprintf(cmdbuf,"A%c PL",mptr->axis[axisno].label);
#else
      sprintf(cmdbuf,"BL%d; ",res);
#endif
    }
  } else if (!strncmp(strval,"limits",lstr)) {
    mptr->axis[axisno].limits = (state) ? 1 : 0;
    if (state) {
      sprintf(cmdbuf,"A%c LMN ",mptr->axis[axisno].label);
    } else {
      sprintf(cmdbuf,"A%c LMF ",mptr->axis[axisno].label);
    }
  } else {
    Tcl_SetResult(interp,"options: axis limits",TCL_STATIC);
    return TCL_ERROR;
  }
  
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while changing state",TCL_STATIC);
    return TCL_ERROR;
  }  

  return TCL_OK;
}


int MAXv_command(ClientData clientdata, Tcl_Interp *interp,
		 int objc, Tcl_Obj * objv[]) 
{
  char * strval;
  int lstr, ival, retn;
  maxvmod * mptr;

  mptr = clientdata;
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
    return MAXv_status(clientdata,interp,objc-2,objv+2);
    return TCL_OK;
  } else if (!strncmp(strval,"kill",lstr)) {
    if ((lstr = MAXv_Kill(mptr->base)) < 0) {
      Tcl_SetResult(interp,"Error occurred while sending kill message",
		    TCL_STATIC);
      return TCL_ERROR;
    }
  } else if (!strncmp(strval,"stop",lstr)) {
    return MAXv_stop(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"move",lstr)) {
    return MAXv_move(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"delta",lstr)) {
    return MAXv_delta(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"position",lstr)) {
    return MAXv_position(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"jog",lstr)) {
    return MAXv_jog(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"configure",lstr)) {
    return MAXv_conf(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"enable",lstr)) {
    return MAXv_enable(clientdata,interp,objc-2,objv+2,1);
  } else if (!strncmp(strval,"disable",lstr)) {
    return MAXv_enable(clientdata,interp,objc-2,objv+2,0);
  } else if (!strncmp(strval,"home",lstr)) {
    return MAXv_home(clientdata,interp,objc-2,objv+2);    
  } else if (!strncmp(strval,"raw",lstr)) {
    return MAXv_raw(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"talk",lstr)) {
    return MAXv_rawtalk(clientdata,interp,objc-2,objv+2);
    /*
  } else if (!strncmp(strval,"oscillate",lstr)) {
    return MAXv_osc(clientdata,interp,objc-2,objv+2);
    */
  } else if (!strncmp(strval,"pointers",lstr)) {
    return MAXv_pointers(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"debug",lstr)) {
    if (objc < 3) {
      Tcl_SetObjResult(interp,Tcl_NewIntObj(maxvdebug));
      return TCL_OK;
    }
    if ((retn = Tcl_GetIntFromObj(interp,objv[2],&ival)) != TCL_OK) 
      return retn;
    maxvdebug = ival;
    return TCL_OK;
  } else if (!strncmp(strval,"reset",lstr)) {
    retn = MAXv_Reset(mptr->base);
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
int maxv(ClientData clientdata, Tcl_Interp *interp,
	 int objc, Tcl_Obj * objv[])
{
  char * strval, motname[80], cmdbuf[80];
  int lstr;
  int address;
  maxvmod * mptr;

  if (objc < 3) {
    Tcl_SetResult(interp,
		  "Usage: maxv create address",
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

  sprintf(motname,"maxv%d",mcounter);
  mcounter++;
  mptr = (maxvmod *) ckalloc(sizeof(maxvmod));
  memset(mptr,0,sizeof(maxvmod));
  mptr->base = address;
  /* Configure the axes */
  MAXv_Init(mptr->base);
  MAXv_axis_init(mptr);

  sprintf(cmdbuf,"BDFF00; ");
  if ((lstr = MAXv_SendCommand(mptr->base,cmdbuf)) < 0) {
    Tcl_SetResult(interp,"Error occurred while setting IO direction",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  vsleep(100);

  Tcl_CreateObjCommand(interp,motname,(Tcl_ObjCmdProc *) MAXv_command,
		       (ClientData) mptr,NULL);
  Tcl_SetObjResult(interp,Tcl_NewStringObj(motname,strlen(motname)));
  return TCL_OK;
}
