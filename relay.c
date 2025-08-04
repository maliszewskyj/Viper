static char cvsid[] = "$Id: relay.c,v 1.2 2008/09/04 16:06:29 nickm Exp $";
/*
 * relay - code for controlling VMIC2210 relay
 * 
 * relay create [base address]
 * r read
 * r clear  ?bit?
 * r set    ?bit?
 * r output ?val?
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
#include "vmic2210.h"
#include "vme_util.h"

typedef struct {
  unsigned int base;
  unsigned int word;
} rly;
static int rcounter = 0;

int rly_write(ClientData clientdata, Tcl_Interp *interp,
		int objc, Tcl_Obj * objv[]) {
  rly * rptr;
  int retn;
  int iword;
  unsigned int word;

  rptr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Usage: relayx write <value>",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&iword) != TCL_OK) return TCL_ERROR;

  word = (unsigned) iword;
  retn = VMIC2210_SetLongWord(rptr->base,word);
  if (retn != 0) {
    Tcl_SetResult(interp,
		  "VMIC2210_GetLongWord() failed to read word",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

int rly_read(ClientData clientdata, Tcl_Interp *interp,
	     int objc, Tcl_Obj * objv[]) {
  rly * rptr;
  int retn, i;
  unsigned int word, mask;
  Tcl_Obj * optr;

  rptr = clientdata;

  retn = VMIC2210_GetLongWord(rptr->base, &word);
  if (retn < 0) {
    Tcl_SetResult(interp,
		  "VMIC2210_GetLongWord() failed to read word",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  optr = Tcl_NewObj();
  for (i=0;i<32;i++) {
    mask = (1 << i);
    Tcl_ListObjAppendElement(interp,optr,
			     Tcl_NewIntObj(((word & mask)? 1: 0)));
  }
  Tcl_SetObjResult(interp,optr);

  return TCL_OK;
}

int rly_set(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  rly * rptr;
  int retn,bit;
  rptr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Usage: relayx set <bit>",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&bit) != TCL_OK) return TCL_ERROR;
  if ((bit < 0) || (bit > 31)) {
    Tcl_SetResult(interp,"Bit value must be between 0 and 31",TCL_STATIC);
    return TCL_ERROR;
  }
  retn = VMIC2210_SetClrBit(rptr->base, bit, 1);
  if (retn < 0) {
    Tcl_SetResult(interp,
		  "VMIC2210_SetClrBit() failed to clear bit",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

int rly_clear(ClientData clientdata, Tcl_Interp *interp,
	      int objc, Tcl_Obj * objv[]) 
{
  rly * rptr;
  int retn,bit;
  rptr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Usage: relayx clear <bit>",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&bit) != TCL_OK) return TCL_ERROR;
  if ((bit < 0) || (bit > 31)) {
    Tcl_SetResult(interp,"Bit value must be between 0 and 31",TCL_STATIC);
    return TCL_ERROR;
  }
  retn = VMIC2210_SetClrBit(rptr->base, bit, 0);
  if (retn < 0) {
    Tcl_SetResult(interp,
		  "VMIC2210_SetClrBit() failed to set bit",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

int rly_chk(ClientData clientdata, Tcl_Interp *interp,
		int objc, Tcl_Obj * objv[]) {
  rly * rptr;
  int retn;
  rptr = clientdata;

  retn = VMIC2210_CheckID (rptr->base);
  Tcl_SetObjResult(interp,Tcl_NewIntObj(retn));

  return TCL_OK;
}

int rly_enable(ClientData clientdata, Tcl_Interp *interp,
	      int objc, Tcl_Obj * objv[]) 
{
  rly * rptr;
  int retn,bit;
  rptr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Usage: relayx enable <true/false>",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&bit) != TCL_OK) return TCL_ERROR;
  bit = (bit)? 1: 0;
  retn = VMIC2210_Enable(rptr->base, bit);
  if (retn < 0) {
    Tcl_SetResult(interp,
		  "VMIC2210_SetClrBit() failed to set bit",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

int rly_command(ClientData clientdata, Tcl_Interp *interp,
		int objc, Tcl_Obj * objv[]) {
  char * strval;
  int lstr;
  rly * rptr;

  rptr = clientdata;
  if (objc < 2) {
    Tcl_SetResult(interp,
		  "Options: read clear set ",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  lstr = strlen(strval);

  if (!strncmp("read",strval,lstr)) {
    return rly_read(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp("write",strval,lstr)) {
    return rly_write(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp("clear",strval,lstr)) {
    return rly_clear(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp("set",strval,lstr)) {
    return rly_set(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp("check",strval,lstr)) {
    return rly_chk(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp("enable",strval,lstr)) {
    return rly_enable(clientdata,interp,objc-2,objv+2);
  } else {
    Tcl_SetResult(interp,
		  "Options: read clear set",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

int relay(ClientData clientdata, Tcl_Interp *interp, 
           int objc, Tcl_Obj * objv[])
{
  char * strval, relayname[80];
  int lstr;
  int address;
  rly * rptr;

  if (objc < 3) {
    Tcl_SetResult(interp,
		  "Usage: oms create <address>",
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

  sprintf(relayname,"relay%d",rcounter);
  rcounter++;
  rptr = (rly *) ckalloc(sizeof(rly));
  memset(rptr,0,sizeof(rly));
  rptr->base = address;

  Tcl_CreateObjCommand(interp,relayname,(Tcl_ObjCmdProc *) rly_command,
		       (ClientData) rptr,NULL);
  Tcl_SetObjResult(interp,Tcl_NewStringObj(relayname,strlen(relayname)));
  return TCL_OK;
}
