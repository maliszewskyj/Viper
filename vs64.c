static char cvsid[] = "$Id: vs64.c,v 1.6 2009/01/15 15:25:32 nickm Exp $";
/*
 * vs64 - code for controlling Joerger VS64 VME scaler
 * 
 * vs64 create [base address]
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
#include "vs64.h"
#include "vme_util.h"

#define XFERTHENREAD 1

/* Global variables */

int vscounter=0;
extern int omsdebug;

/* Reset module */
int
vs64_reset(Tcl_Interp *interp, unsigned int base) {
  unsigned int addr;
  unsigned short val;

  addr = base + VS64_GLBRST;
  val = 1;  

  if (_vme_write(VME_A16, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_reset:write RESET",TCL_STATIC);
    return TCL_ERROR;
  }
  return TCL_OK;
}

/* Arm module */
int
vs64_arm(Tcl_Interp * interp, unsigned int base) {
  unsigned int addr;
  unsigned short val;

  /* Set global count enable */
  addr = base + VS64_CONTROL; // Right now just reset all counters
  val = 0;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_preload:write",TCL_STATIC);
    return TCL_ERROR;
  }

  addr = base + VS64_CNT_EN;
  val = 1;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_reset:write CONTROL",TCL_STATIC);
    return TCL_ERROR;
  }

  addr = base + VS64_ARM;
  val = 1;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_reset:write CONTROL",TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/* Disarm module */
int
vs64_disarm(Tcl_Interp * interp, unsigned int base) {
  unsigned int addr;
  unsigned short val;

  addr = base + VS64_DISARM;
  val = 1;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_disarm:write CONTROL",TCL_STATIC);
    return TCL_ERROR;
  }

  addr = base + VS64_CNT_DIS;
  val = 1;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_disarm:write CONTROL",TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}


/* Get counter status */
/* Status register:
   Bit  0: Global count enable is enabled
   Bit  1: Global count enable FF is enabled
   Bit  2: IRQ#1 source set (overflow has occurred)
   Bit  3: IRQ#2 source set (FP transfer clock received)
   Bit  4: IRQ#3 source set (Gate has ended)
   Bit  5: IRQ#1 Set (overflow has occurred)
   Bit  6: IRQ#2 Set (FP Clock registers received)
   Bit  7: IRQ#3 Set (Gate has ended)
   Bit  8: FP Reset input true
   Bit  9: Programmed internal gate is true
   Bit 10: FP Gate input is True (asserting GATE OPEN)
   Bit 11: FP Arm  input is True (asserting ARM)
   Bit 12: FP Arm output is True (asserting ARM)

 */
int
vs64_status(Tcl_Interp * interp, unsigned int base) {
  unsigned int addr;
  unsigned short val;

  addr = base + VS64_STATUS;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_status:read CONTROL",TCL_STATIC);
    return TCL_ERROR;
  }

  /* If (val & 1) = 1, then we're armed and counting */
  val = (val & 0x0800) ? 1 : 0;
  Tcl_SetObjResult(interp,Tcl_NewIntObj(val));
  return TCL_OK;
}

/* Read from  */
int
vs64_verify(Tcl_Interp * interp, unsigned int base) {
  unsigned int addr;
  unsigned short val;
  int model;
  int serial;

  addr = base + VS64_ID;
  if (_vme_read(VME_A16, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_status:read CONTROL",TCL_STATIC);
    return TCL_ERROR;
  }

  model  = (val & 0xF800) >> 10;
  serial = (val & 0x7FF);

  /* The low byte of what we read from the MFCID register should be 'J' */
  if (omsdebug) {
    printf("VS64 at 0x%08x Model = %d Serial = %d\n",addr,model,serial);
  }
  val = (16) ? 1 : 0;
  Tcl_SetObjResult(interp,Tcl_NewIntObj(val));
  return TCL_OK;
}


/*
 * Write direction vector to direction register
 * Enable IRQ mask for the same vector - assume we're counting down for
 *   monitor
 */
int
vs64_direction(ClientData clientdata, Tcl_Interp *interp,
	      int objc, Tcl_Obj * objv[]) {
  int val;
  unsigned short sval;
  unsigned int addr;
  vs * vptr;
 
  if (objc < 3) {
    Tcl_SetResult(interp,"Usage: vs# direction vector",TCL_STATIC);
    return TCL_ERROR;
  }
  vptr = (vs *) clientdata;
  if (Tcl_GetIntFromObj(interp,objv[2],&val) != TCL_OK) return TCL_ERROR;
  sval = (short) (0xffff & val);

  addr = vptr->base;
  /*
  addr = vptr->base + VSC_DIRECT;
  if (_vme_write(VME_A16, addr, &sval, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_direction:write DIRECT",TCL_STATIC);
    return TCL_ERROR;
  }

  addr = vptr->base + VSC_IRQMSK;
  if (_vme_write(VME_A16, addr, &sval, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_direction:write DIRECT",TCL_STATIC);
    return TCL_ERROR;
  }
  */

  return TCL_OK;
}


/*
 * Preload counters with values
 *
 */
int
vs64_preload(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  int nd, i, val;
  unsigned short sval;
  unsigned int addr;
  vs * vptr;
  Tcl_Obj **dptr;

  if (objc < 3) {
    Tcl_SetResult(interp,"Usage: vs# preload [list]",TCL_STATIC);
    return TCL_ERROR;
  }
  vptr = (vs *) clientdata;
  if (Tcl_ListObjGetElements(interp,objv[2],&nd,&dptr) != TCL_OK) 
    return TCL_ERROR;

  /* Load counter[] with values from list */
  for (i=0;i<nd;i++) {
      if (Tcl_GetIntFromObj(interp,dptr[i],&val) != TCL_OK) return TCL_ERROR;
      vptr->counts[i] = val;
  }

  addr = vptr->base + VS64_GLBRST; // Right now just reset all counters
  sval = 1;
  if (_vme_write(VME_A16, addr, &sval, 1, VME_D16) < 0) {
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
vs64_read(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  int i;
  unsigned int addr;
  unsigned short val;
  vs * vptr;
  Tcl_Obj * rptr;

  vptr = (vs *) clientdata;
#ifdef XFERTHENREAD
  /* Clock in transfer */
  addr = vptr->base + VS64_CLKXFER;
  val = 1;
  if (_vme_write(VME_A16, addr, &val, 1, VME_D16) < 0) {
    Tcl_SetResult(interp,"vsc_preload:write",TCL_STATIC);
    return TCL_ERROR;
  }

  addr = vptr->base + VS64_XFER;
#else
  addr = vptr->base + VS64_XFERFLY;

#endif

  memset(vptr->counts,0,sizeof(vptr->counts));
  if (_vme_read(VME_A16, addr, vptr->counts, VS64_NCOUNTERS, VME_D32) < 0) {
    Tcl_SetResult(interp,"vsc_read:read",TCL_STATIC);
    return TCL_ERROR;
  }
  rptr = Tcl_NewObj();
  for (i=0;i<VS64_NCOUNTERS;i++) {
    Tcl_ListObjAppendElement(interp,rptr,Tcl_NewIntObj(vptr->counts[i]));
  }
  Tcl_SetObjResult(interp,rptr);

  return TCL_OK;
}

int
vs64_command(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char * strval;
  int lstr;
  vs * vptr;

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
    return vs64_arm(interp,vptr->base);
  } else if (!strncmp(strval,"disarm",lstr)) {
    return vs64_disarm(interp,vptr->base);
  } else if (!strncmp(strval,"reset",lstr)) {
    return vs64_reset(interp,vptr->base);
  } else if (!strncmp(strval,"status",lstr)) {
    return vs64_status(interp,vptr->base);
  } else if (!strncmp(strval,"verify",lstr)) {
    return vs64_verify(interp,vptr->base);
  } else if (!strncmp(strval,"version",lstr)) {
    Tcl_AppendResult(interp,cvsid,(char *)0);
  } else if (!strncmp(strval,"direction",lstr)) {
    return vs64_direction(clientdata,interp,objc,objv);
  } else if (!strncmp(strval,"preload",lstr)) {
    return vs64_preload(clientdata,interp,objc,objv);
  } else if (!strncmp(strval,"read",lstr)) {
    return vs64_read(clientdata,interp,objc-2,objv+2);
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
vs64(ClientData clientdata, Tcl_Interp *interp,
           int objc, Tcl_Obj * objv[]){
  char * strval, ctrname[80];
  int lstr;
  int address, ncounters;
  vs * vptr;

  if (objc < 2) {
    Tcl_SetResult(interp,
		  "Usage: vs64 create <address>",
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

  sprintf(ctrname,"vs%d",vscounter);
  vscounter++;
  vptr = (vs *) ckalloc(sizeof(vs));
  vptr->base = address;



  Tcl_CreateObjCommand(interp,ctrname,(Tcl_ObjCmdProc *) vs64_command,
		       (ClientData) vptr,NULL);
  Tcl_SetObjResult(interp,Tcl_NewStringObj(ctrname,strlen(ctrname)));
  return TCL_OK;
}

