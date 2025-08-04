static char cvsid[] = "$Id: vscaler.c,v 1.9 2006/11/17 16:05:10 nickm Exp $";
/*
 * vscaler - code for controlling Joerger VSC16 VME scaler
 * 
 * vsc16 create [base address]
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
#include "vsc16.h"
#include "vme_util.h"

/* Global variables */

int vcounter=0;

/* Reset module */
int
vsc_reset(Tcl_Interp *interp, unsigned int base) {
  unsigned int addr;
  unsigned short val;

  addr = base + VSC_RESET;
  val = 1;  

  if (_vme_write(VME_A32, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_reset:write RESET",TCL_STATIC);
    return TCL_ERROR;
  }

  addr = base + VSC_CONTROL;
  val = 0;

  if (_vme_write(VME_A32, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_reset:write CONTROL",TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/* Arm module */
int
vsc_arm(Tcl_Interp * interp, unsigned int base) {
  unsigned int addr;
  unsigned short val;

  addr = base + VSC_CONTROL;
  val = 1;
  if (_vme_write(VME_A32, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_reset:write CONTROL",TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/* Disarm module */
int
vsc_disarm(Tcl_Interp * interp, unsigned int base) {
  unsigned int addr;
  unsigned short val;

  addr = base + VSC_CONTROL;
  val = 0;
  if (_vme_write(VME_A32, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_disarm:write CONTROL",TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/* Get counter status */
int
vsc_status(Tcl_Interp * interp, unsigned int base) {
  unsigned int addr;
  unsigned short val;

  addr = base + VSC_CONTROL;
  if (_vme_read(VME_A32, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_status:read CONTROL",TCL_STATIC);
    return TCL_ERROR;
  }

  /* If (val & 1) = 1, then we're armed and counting */
  val = (val & 1) ? 1 : 0;
  Tcl_SetObjResult(interp,Tcl_NewIntObj(val));
  return TCL_OK;
}

/* Read from  */
int
vsc_verify(Tcl_Interp * interp, unsigned int base) {
  unsigned int addr;
  unsigned short val;
  char ch;

  addr = base + VSC_MFCID;
  if (_vme_read(VME_A32, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_status:read CONTROL",TCL_STATIC);
    return TCL_ERROR;
  }

  ch = (char) (val & 0xff);

  /* The low byte of what we read from the MFCID register should be 'J' */
  val = (ch == 'J') ? 1 : 0;
  Tcl_SetObjResult(interp,Tcl_NewIntObj(val));
  return TCL_OK;
}

/*
 * Write direction vector to direction register
 * Enable IRQ mask for the same vector - assume we're counting down for
 *   monitor
 */
int
vsc_direction(ClientData clientdata, Tcl_Interp *interp,
	      int objc, Tcl_Obj * objv[]) {
  int val;
  unsigned short sval;
  unsigned int addr;
  vscaler * vptr;
 
  if (objc < 3) {
    Tcl_SetResult(interp,"Usage: vsc# direction vector",TCL_STATIC);
    return TCL_ERROR;
  }
  vptr = (vscaler *) clientdata;
  if (Tcl_GetIntFromObj(interp,objv[2],&val) != TCL_OK) return TCL_ERROR;
  sval = (short) (0xffff & val);

  addr = vptr->base + VSC_DIRECT;
  if (_vme_write(VME_A32, addr, &sval, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_direction:write DIRECT",TCL_STATIC);
    return TCL_ERROR;
  }

  addr = vptr->base + VSC_IRQMSK;
  if (_vme_write(VME_A32, addr, &sval, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_direction:write DIRECT",TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 * Preload counters with values
 *
 */
int
vsc_preload(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  int nd, i, val;
  unsigned int addr;
  vscaler * vptr;
  Tcl_Obj **dptr;

  if (objc < 3) {
    Tcl_SetResult(interp,"Usage: vsc# preload [list]",TCL_STATIC);
    return TCL_ERROR;
  }
  vptr = (vscaler *) clientdata;
  if (Tcl_ListObjGetElements(interp,objv[2],&nd,&dptr) != TCL_OK) 
    return TCL_ERROR;

  nd = (nd > vptr->ncounters) ? vptr->ncounters : nd;
  /* Load counter[] with values from list */
  for (i=0;i<nd;i++) {
      if (Tcl_GetIntFromObj(interp,dptr[i],&val) != TCL_OK) return TCL_ERROR;
      vptr->counts[i] = val;
  }

  addr = vptr->base + VSC_DATRB;
  if (_vme_write(VME_A32, addr, vptr->counts, nd, VME_D32) < 0) {
    Tcl_SetResult(interp,"vsc_preload:write",TCL_STATIC);
    return TCL_ERROR;
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
vsc_read(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  int i;
  unsigned int addr;
  vscaler * vptr;
  Tcl_Obj * rptr;

  vptr = (vscaler *) clientdata;
  addr = vptr->base + VSC_DATAB;
  
  memset(vptr->counts,0,sizeof(vptr->counts));
  if (_vme_read(VME_A32, addr, vptr->counts, vptr->ncounters, VME_D32) < 0) {
    Tcl_SetResult(interp,"vsc_read:read",TCL_STATIC);
    return TCL_ERROR;
  }

  rptr = Tcl_NewObj();
  for (i=0;i<vptr->ncounters;i++) {
    Tcl_ListObjAppendElement(interp,rptr,Tcl_NewIntObj(vptr->counts[i]));
  }
  Tcl_SetObjResult(interp,rptr);

  return TCL_OK;
}

int
vsc_command(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char * strval;
  int lstr;
  vscaler * vptr;

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
    return vsc_arm(interp,vptr->base);
  } else if (!strncmp(strval,"disarm",lstr)) {
    return vsc_disarm(interp,vptr->base);
  } else if (!strncmp(strval,"reset",lstr)) {
    return vsc_reset(interp,vptr->base);
  } else if (!strncmp(strval,"status",lstr)) {
    return vsc_status(interp,vptr->base);
  } else if (!strncmp(strval,"verify",lstr)) {
    return vsc_verify(interp,vptr->base);
  } else if (!strncmp(strval,"version",lstr)) {
    Tcl_AppendResult(interp,cvsid,(char *)0);
  } else if (!strncmp(strval,"direction",lstr)) {
    return vsc_direction(clientdata,interp,objc,objv);
  } else if (!strncmp(strval,"preload",lstr)) {
    return vsc_preload(clientdata,interp,objc,objv);
  } else if (!strncmp(strval,"read",lstr)) {
    return vsc_read(clientdata,interp,objc-2,objv+2);
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
vsc(ClientData clientdata, Tcl_Interp *interp,
           int objc, Tcl_Obj * objv[]){
  char * strval, ctrname[80];
  int lstr;
  int address, ncounters;
  vscaler * vptr;

  if (objc < 3) {
    Tcl_SetResult(interp,
		  "Usage: vsc create <address> ?ncounters?",
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
    if (!((ncounters == 8) || (ncounters == 16))) {
      Tcl_SetResult(interp,"Number of counters restricted to 8 or 16",
		    TCL_STATIC);
      return TCL_ERROR;
    }
  } else {
    ncounters= 16;
  }

  sprintf(ctrname,"vsc%d",vcounter);
  vcounter++;
  vptr = (vscaler *) ckalloc(sizeof(vscaler));
  vptr->base = address;
  vptr->ncounters = ncounters;

  Tcl_CreateObjCommand(interp,ctrname,(Tcl_ObjCmdProc *) vsc_command,
		       (ClientData) vptr,NULL);
  Tcl_SetObjResult(interp,Tcl_NewStringObj(ctrname,strlen(ctrname)));
  return TCL_OK;
}

