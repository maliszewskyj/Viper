static char cvsid[] = "$Id: vsdummy.c,v 1.3 2009/10/02 19:25:25 nickm Exp $";
/*
 * vsdummy - code for controlling Dummy scaler
 * 
 * vsdummy create [base address]
 * s direction [direction vector]
 * s arm
 * s disarm
 * s status
 * s reset
 * s verify
 * s preload
 * s read
 *
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
#include "vme_util.h"

typedef struct {
  unsigned int base;
  int ncounters;
  unsigned int counts[64];
} vdummy;


/* Global variables */

static int vscounter=0;
extern int omsdebug;

/* Reset module */
int
vsdummy_reset(Tcl_Interp *interp, unsigned int base) {


  return TCL_OK;
}

/* Arm module */
int
vsdummy_arm(Tcl_Interp * interp, unsigned int base) {

  /* Set global count enable */

  return TCL_OK;
}

/* Disarm module */
int
vsdummy_disarm(Tcl_Interp * interp, unsigned int base) {

  return TCL_OK;
}



int
vsdummy_status(Tcl_Interp * interp, unsigned int base) {
  int val;
  val = 1;
  Tcl_SetObjResult(interp,Tcl_NewIntObj(val));
  return TCL_OK;
}

/* Read from  */
int
vsdummy_verify(Tcl_Interp * interp, unsigned int base) {
  int val;
  val = 1;
  Tcl_SetObjResult(interp,Tcl_NewIntObj(val));
  return TCL_OK;
}


/*
 * Write direction vector to direction register
 * Enable IRQ mask for the same vector - assume we're counting down for
 *   monitor
 */
int
vsdummy_direction(ClientData clientdata, Tcl_Interp *interp,
	      int objc, Tcl_Obj * objv[]) {
  int val;
  unsigned short sval;
  unsigned int addr;
  vdummy * vptr;
 
  if (objc < 3) {
    Tcl_SetResult(interp,"Usage: vd# direction vector",TCL_STATIC);
    return TCL_ERROR;
  }
  vptr = (vdummy *) clientdata;
  if (Tcl_GetIntFromObj(interp,objv[2],&val) != TCL_OK) return TCL_ERROR;
  sval = (short) (0xffff & val);

  addr = vptr->base;

  return TCL_OK;
}


/*
 * Preload counters with values
 *
 */
int
vsdummy_preload(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  int nd, i, val;
  vdummy * vptr;
  Tcl_Obj **dptr;

  if (objc < 3) {
    Tcl_SetResult(interp,"Usage: vd# preload [list]",TCL_STATIC);
    return TCL_ERROR;
  }
  vptr = (vdummy *) clientdata;
  if (Tcl_ListObjGetElements(interp,objv[2],&nd,&dptr) != TCL_OK) 
    return TCL_ERROR;

  /* Load counter[] with values from list */
  for (i=0;i<nd;i++) {
      if (Tcl_GetIntFromObj(interp,dptr[i],&val) != TCL_OK) return TCL_ERROR;
      vptr->counts[i] = val;
  }

  return TCL_OK;
}


/*
 * Read data from scalers. 
 *
 * Output can be stuffed into one of two places: result
 *                                               list
 *
 */
int
vsdummy_read(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  int i;
  unsigned int addr;
  vdummy * vptr;
  Tcl_Obj * rptr;

  vptr = (vdummy *) clientdata;
  
  memset(vptr->counts,0,sizeof(vptr->counts));

  rptr = Tcl_NewObj();
  for (i=0;i<vptr->ncounters;i++) {
    Tcl_ListObjAppendElement(interp,rptr,Tcl_NewIntObj(vptr->counts[i]));
  }
  Tcl_SetObjResult(interp,rptr);

  return TCL_OK;
}

int
vsdummy_command(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char * strval;
  int lstr;
  vdummy * vptr;

  vptr = clientdata;
  if (objc < 2) {
    Tcl_SetResult(interp,
		  "Options: arm disarm reset status verify read",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  lstr = strlen(strval);
  /* Process argument */
  if (!strncmp(strval,"arm",lstr)) {
    return vsdummy_arm(interp,vptr->base);
  } else if (!strncmp(strval,"disarm",lstr)) {
    return vsdummy_disarm(interp,vptr->base);
  } else if (!strncmp(strval,"reset",lstr)) {
    return vsdummy_reset(interp,vptr->base);
  } else if (!strncmp(strval,"status",lstr)) {
    return vsdummy_status(interp,vptr->base);
  } else if (!strncmp(strval,"verify",lstr)) {
    return vsdummy_verify(interp,vptr->base);
  } else if (!strncmp(strval,"version",lstr)) {
    Tcl_AppendResult(interp,cvsid,(char *)0);
  } else if (!strncmp(strval,"direction",lstr)) {
    return vsdummy_direction(clientdata,interp,objc,objv);
  } else if (!strncmp(strval,"preload",lstr)) {
    return vsdummy_preload(clientdata,interp,objc,objv);
  } else if (!strncmp(strval,"read",lstr)) {
    return vsdummy_read(clientdata,interp,objc-2,objv+2);
  } else {
    Tcl_SetResult(interp,
		  "Options: arm disarm reset status verify preload read",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}


/* Top level TCL wrapper */
int 
vsdummy(ClientData clientdata, Tcl_Interp *interp,
           int objc, Tcl_Obj * objv[]){
  char * strval, ctrname[80];
  int lstr;
  int address, ncounters;
  vdummy * vptr;

  if (objc < 2) {
    Tcl_SetResult(interp,
		  "Usage: vsdummy create <address>",
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
  if (objc > 3) {
    if (Tcl_GetIntFromObj(interp,objv[3],&ncounters) != TCL_OK) 
      return TCL_ERROR;
  }

  sprintf(ctrname,"vd%d",vscounter);
  vscounter++;
  vptr = (vdummy *) ckalloc(sizeof(vdummy));
  vptr->base = address;
  vptr->ncounters = 64;

  Tcl_CreateObjCommand(interp,ctrname,(Tcl_ObjCmdProc *) vsdummy_command,
		       (ClientData) vptr,NULL);
  Tcl_SetObjResult(interp,Tcl_NewStringObj(ctrname,strlen(ctrname)));
  return TCL_OK;
}

