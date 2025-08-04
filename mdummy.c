#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <tcl.h>
#include "mdummy.h"

static int mcounter;
static int mdumdebug=0;

void mdum_axis_init(mdummod * mptr) {
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

int MAXv_status(ClientData clientdata, Tcl_Interp *interp,
		int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval;
  int lstr, axisno;

  if (objc < 2) {
    Tcl_SetResult(interp,"Syntax: mdumn status <axis> <parameter>",TCL_STATIC);
    return TCL_ERROR;
  }

  Tcl_GetIntFromObj(interp,objv[0],&axisno);
  if (axisno < 0 || axisno > 7) {
    Tcl_SetResult(interp,"Axis out of bounds",TCL_STATIC);
    return TCL_ERROR;
  }

  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  if (!strncmp("moving",strval,lstr)) {
    Tcl_SetObjResult(interp,Tcl_NewIntObj(0));
    return TCL_OK;
  } else if (!strncmp("limits",strval,lstr)) {
    Tcl_SetObjResult(interp,Tcl_NewIntObj(0));
    return TCL_OK;
  } else if (!strncmp("done",strval,lstr)) {
    Tcl_SetObjResult(interp,Tcl_NewIntObj(1));
    return TCL_OK;
  } else if (!strncmp("slip",strval,ax)) {
    Tcl_SetObjResult(interp,Tcl_NewIntObj(0));
    return TCL_OK;
  } else if (!strncmp("enabled",strval,ax)) {
    Tcl_SetObjResult(interp,Tcl_NewIntObj(mptr->axis[axisno].enable));
    return TCL_OK;
  } else if (!strncmp("fault",strval,ax)) {
    Tcl_SetObjResult(interp,Tcl_NewIntObj(0));
    return TCL_OK;
  } else if (!strncmp("register",strval,lstr)) {
    return MAXv_register(clientdata,interp);
  } else if (!strncmp("home",strval,lstr)) {
    Tcl_SetObjResult(interp,Tcl_NewIntObj(0));
    return TCL_OK;
  } else {
    Tcl_SetResult(interp,"Options: moving limits",TCL_STATIC);
    return TCL_ERROR;
  }
  return TCL_OK;
}



int mdum_position(ClientData clientdata, Tcl_Interp *interp,
		  int objc, Tcl_Obj * objv[]) {
  char ax;
  char * strval;
  int lstr,len, axisno, pulses;
  double position;
  mdummod * mptr;
  char cmdbuf[80], response[80];

  if (!objc) {
    Tcl_SetResult(interp,"Usage: maxvn position <axis> ?position?\n",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  Tcl_GetIntFromObj(interp,objv[0],&axisno);

  mptr = (mdummod *) clientdata;

  if (objc > 1) { /* Set position */
    if (Tcl_GetDoubleFromObj(interp,objv[1],&position) != TCL_OK) 
      return TCL_ERROR;

    mptr->axis[axisno].position = position;
  } else {        /* Get position */
    Tcl_SetObjResult(interp,Tcl_NewDoubleObj(mptr->axis[axisno].position));
  }

  return TCL_OK;
}


int mdum_command(ClientData clientdata, Tcl_Interp *interp,
		 int objc, Tcl_Obj * objv[]) 
{
  char * strval;
  int lstr, ival, retn;
  mdummod * mptr;

  mptr = clientdata;
  if (objc < 2) {
    Tcl_SetResult(interp,
		  "Options: status stop move delta position configure raw version",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  lstr = strlen(strval);

  /* Process argument */
  if (!strncmp(strval,"status",lstr)) {
    return mdum_status(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"stop",lstr)) {
    return mdum_stop(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"move",lstr)) {
    return mdum_move(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"delta",lstr)) {
    return mdum_delta(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"position",lstr)) {
    return mdum_position(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"configure",lstr)) {
    return mdum_conf(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval,"enable",lstr)) {
    return mdum_enable(clientdata,interp,objc-2,objv+2,1);
  } else if (!strncmp(strval,"disable",lstr)) {
    return mdum_enable(clientdata,interp,objc-2,objv+2,0);
  } else if (!strncmp(strval,"debug",lstr)) {
    if (objc < 3) {
      Tcl_SetObjResult(interp,Tcl_NewIntObj(maxvdebug));
      return TCL_OK;
    }
    if ((retn = Tcl_GetIntFromObj(interp,objv[2],&ival)) != TCL_OK) 
      return retn;
    maxvdebug = ival;
    return TCL_OK;
  } else if (!strncmp(strval,"version",lstr)) {
    Tcl_SetObjResult(interp,Tcl_NewStringObj(cvsid,strlen(cvsid)));
    return TCL_OK;
  } else {
    Tcl_SetResult(interp,
		  "Options: status stop move delta position configure raw version",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}


/* Top level TCL wrapper */
int mdummy(ClientData clientdata, Tcl_Interp *interp,
	 int objc, Tcl_Obj * objv[])
{
  char * strval, motname[80], cmdbuf[80];
  int lstr;
  int address;
  mdummod * mptr;

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

  sprintf(motname,"mdum%d",mcounter);
  mcounter++;
  mptr = (mdummod *) ckalloc(sizeof(mdummod));
  memset(mptr,0,sizeof(mdummod));
  mptr->base = address;
  /* Configure the axes */
  //  MAXv_Init(mptr->base);
  //MAXv_axis_init(mptr);

  Tcl_CreateObjCommand(interp,motname,(Tcl_ObjCmdProc *) mdum_command,
		       (ClientData) mptr,NULL);
  Tcl_SetObjResult(interp,Tcl_NewStringObj(motname,strlen(motname)));
  return TCL_OK;
}
