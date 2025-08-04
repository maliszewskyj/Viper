static char cvsid[] = "$Id: nigpib.c,v 1.4 2003/07/21 15:35:46 dkeyser Exp $";
/*
 * nigpib - Code for controlling National Instruments 1014 GPIB Controller
 *
 * ni1014 create <addr>
 *
 * Created command has the following syntax
 *
 * gpib0 version
 * gpib0 cmd 
 * gpib0 ren
 * gpib0 sic
 * gpib0 status
 * gpib0 trg   ?dev?
 * gpib0 clear ?dev?
 * gpib0 read  ?dev? ?nbytes?
 * gpib0 write ?dev? ?data?
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <tcl.h>
#include <sys/types.h>
#include "ni1014fn.h"
#include "nigpib.h"

#ifdef MAPMEM
#define VME_WINSIZE 65536
extern char * vme_mem;
#endif

/* Global variables */
static int gcounter;
extern int ibdebug;
static char ibbuf[MAXBUF];


int
ib_cmd(ClientData clientdata, Tcl_Interp * interp,
	int objc, Tcl_Obj * objv[]) {
  int lstr;
  int retn;
  char * strval;
  ibbd * gptr;  
  if (objc < 1) {
    Tcl_SetResult(interp,"Usage: gpib# cmd <command string>",TCL_STATIC);
    return TCL_ERROR;
  }
  gptr = (ibbd *) clientdata;
  strval = Tcl_GetStringFromObj(objv[0],&lstr);
  retn = ibcmd(gptr,strval,lstr);
  if (retn < 0) {
    Tcl_SetResult(interp,"Error sending command",TCL_STATIC);
    return TCL_ERROR;
  }
	       
  return TCL_OK;
}

int
ib_read(ClientData clientdata, Tcl_Interp * interp,
	int objc, Tcl_Obj * objv[]) {
  int addr, nbytes;
  int retn;
  ibbd * gptr;

  if (objc < 2) {
    Tcl_SetResult(interp,"gpib# read <addr> <nbytes>",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&addr) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetIntFromObj(interp,objv[1],&nbytes) != TCL_OK) return TCL_ERROR;
  if (nbytes > sizeof(ibbuf)) nbytes = sizeof(ibbuf);

  memset(ibbuf,0,sizeof(ibbuf));
  gptr = (ibbd *) clientdata;

  if ((addr < 1) || (addr > 31)) {
    Tcl_SetResult(interp,"Address out of range",TCL_STATIC);
    return TCL_ERROR;
  }
  
  if ((retn = ibrd(gptr, addr, ibbuf, nbytes)) < 0) {
    Tcl_SetResult(interp,"Error reading from device",TCL_STATIC);
    return TCL_ERROR;
  }

  if (retn) Tcl_SetObjResult(interp,Tcl_NewStringObj(ibbuf,retn));
    
  return TCL_OK;
}

int
ib_write(ClientData clientdata, Tcl_Interp * interp,
	 int objc, Tcl_Obj * objv[]) {
  int addr, lstr, nbytes, retn;
  char * strval;
  ibbd * gptr;
  
  if (objc < 2) {
    Tcl_SetResult(interp,"gpib# write <addr> <data>",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&addr) != TCL_OK) return TCL_ERROR;
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  nbytes = (lstr < sizeof(ibbuf)) ? lstr : sizeof(ibbuf);
  memcpy(ibbuf,strval,nbytes);
  gptr = (ibbd *) clientdata;

  if (ibdebug) {
    printf("Writing %d bytes\n",nbytes);
    printf("Writing->%s<-\n",ibbuf);
  }
  retn = ibwrt(gptr, addr, ibbuf, nbytes);

  return TCL_OK;
}

int
ib_command(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) {
  char * strval;
  int lstr, bool, dev;
  ibbd * gptr;

  gptr = clientdata;

  if (objc < 2) {
    Tcl_SetResult(interp,
		  "Options: version",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  lstr = strlen(strval);  

  if (!strncmp(strval,"cmd",lstr)) {
    return ib_cmd(clientdata, interp, objc-2, objv+2);
  } else if (!strncmp(strval,"sic",lstr)) {
    ibsic(gptr);
    return TCL_OK;
  } else if (!strncmp(strval,"clear",lstr)) {
    if (objc < 3) {
      Tcl_SetResult(interp,"Usage: gpib# clear <device>",TCL_STATIC);
      return TCL_ERROR;
    }
    if (Tcl_GetBooleanFromObj(interp,objv[2],&dev) != TCL_OK) 
      return TCL_ERROR;
    ibclr(gptr,dev);
  } else if (!strncmp(strval,"trg",lstr)) {
    if (objc < 3) {
      Tcl_SetResult(interp,"Usage: gpib# trg <device>",TCL_STATIC);
      return TCL_ERROR;
    }
    if (Tcl_GetBooleanFromObj(interp,objv[2],&dev) != TCL_OK) 
      return TCL_ERROR;
    ibtrg(gptr,dev);
  } else if (!strncmp(strval,"ren",lstr)) {
    bool = 1;
    if (objc > 3) 
      if (Tcl_GetBooleanFromObj(interp,objv[2],&bool) != TCL_OK) 
	return TCL_ERROR;
    ibren(gptr,bool);
  } else if (!strncmp(strval,"status",lstr)) {
    bool = (int) ibstat(gptr);
    Tcl_SetObjResult(interp,Tcl_NewIntObj(bool));
    return TCL_OK;
  } else if (!strncmp(strval,"read",lstr)) {
    return ib_read(clientdata, interp, objc-2, objv+2);
  } else if (!strncmp(strval,"write",lstr)) {
    return ib_write(clientdata, interp, objc-2, objv+2);
  } else if (!strncmp(strval,"init",lstr)) {
    ibinit(gptr);
    ibsic(gptr);
  } else if (!strncmp(strval,"version",lstr)) {
    Tcl_SetObjResult(interp,Tcl_NewStringObj(cvsid,strlen(cvsid)));
    return TCL_OK;
  } else {
    Tcl_SetResult(interp,
		  "Options: cmd sic clear trg ren read write version",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

void
ibexit(ClientData clientdata) {
  ibbd * gptr;
  gptr = (ibbd *) clientdata;

}


/* Top level TCL wrapper */
int
ni1014(ClientData clientdata, Tcl_Interp *interp,
       int objc, Tcl_Obj * objv[]) {
  char * strval, modname[80];
  int lstr;
  int address;
  ibbd * gptr;

  if (objc < 3) {
    Tcl_SetResult(interp,
		  "Usage: nigpib create address",
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
  /* Test bounds of address */

  /* Set up structure describing module */
  sprintf(modname,"gpib%d",gcounter);
  gcounter++;
  gptr = (ibbd *) ckalloc(sizeof(ibbd));
  memset(gptr,0,sizeof(ibbd));
  gptr->fd   = 0;
  gptr->addr = address;


#ifdef MAPMEM
  gptr->base = vme_mem + address;
#else
  gptr->base = NULL;
#endif

  Tcl_CreateObjCommand(interp,modname,(Tcl_ObjCmdProc *) ib_command,
		       (ClientData) gptr, ibexit);
  Tcl_SetObjResult(interp,Tcl_NewStringObj(modname,strlen(modname)));
  return TCL_OK;
}
