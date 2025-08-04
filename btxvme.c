static char cvsid[] = "$Id: btxvme.c,v 1.21 2013/04/05 19:32:45 nickm Exp $";
/*
 * btxvme - loadable extension for performing transactions over VME bus
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <termio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <tcl.h>

#include "vme_util.h"

extern int vmedebug;
extern int omsdebug;
extern int ibdebug;

/* Function Prototypes */
int vsc(ClientData, Tcl_Interp *, int, Tcl_Obj * []);
int vs64(ClientData, Tcl_Interp *, int, Tcl_Obj * []);
int vsdummy(ClientData, Tcl_Interp *, int, Tcl_Obj * []);
int mdummy(ClientData, Tcl_Interp *, int, Tcl_Obj * []);
int sis(ClientData, Tcl_Interp *, int, Tcl_Obj * []);
int oms(ClientData, Tcl_Interp *, int, Tcl_Obj * []);
int maxv(ClientData, Tcl_Interp *, int, Tcl_Obj * []);
int ni1014(ClientData, Tcl_Interp *, int, Tcl_Obj * []);
int relay(ClientData, Tcl_Interp *, int, Tcl_Obj * []);
int dio(ClientData, Tcl_Interp *, int, Tcl_Obj * []);
int ipio(ClientData, Tcl_Interp *, int, Tcl_Obj * []);
int TIP(ClientData, Tcl_Interp *, int, Tcl_Obj * []);


int VME_IO(ClientData clientdata, Tcl_Interp *interp,
           int objc, Tcl_Obj * objv[])
{
  char * strval;
  int i, lstr, addr, val, nd;
  int retn;
  int wrdata = 0, xfersize;
  unsigned char  * clist;
  unsigned short * slist;
  unsigned int   * ilist;
  int as, dw;
  Tcl_Obj ** dptr, * rptr;

  if (objc < 6) {
    Tcl_SetResult(interp,
		  "Usage: vme read/write VME_Axx VME_Dxx address ndata/data",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  /* Determine which action to take */
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  if        (!strncmp("write",strval,lstr)) { wrdata = 1;
  } else if (!strncmp("read",strval,lstr)) {  wrdata = 0;
  } else {
    Tcl_SetResult(interp,"Actions: read, write",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Determine which address space to work in */
  strval = Tcl_GetStringFromObj(objv[2],&lstr);
  if (strtoas(strval,&as) < 0) {
    Tcl_AppendResult(interp,"Invalid address space: ",strval,(char *) NULL);
    return TCL_ERROR;
  }

  strval = Tcl_GetStringFromObj(objv[3],&lstr);
  if (strtodw(strval,&dw) < 0) {
    Tcl_AppendResult(interp,"Invalid data width: ",strval, (char *) NULL);
    return TCL_ERROR;
  }
  switch (dw) {
  case VME_D8: xfersize = 1; break;
  case VME_D16: xfersize = 2; break;
  case VME_D32: 
    xfersize = 4;    
  }

  /* Get base address */
  if (Tcl_GetIntFromObj(interp,objv[4],&addr) != TCL_OK) {
    return TCL_ERROR;
  }

  if (Tcl_ListObjGetElements(interp,objv[5],&nd,&dptr) != TCL_OK) 
    return TCL_ERROR;
  if (wrdata) {
    /* Read data from input list */
    switch(xfersize) {
    case 1:
      clist = (unsigned char *) ckalloc(nd * sizeof(char));
      break;
    case 2:
      slist = (unsigned short *) ckalloc(nd * sizeof(short));
      break;
    case 4:
    default:
      ilist = (unsigned int *) ckalloc(nd * sizeof(int));
    }

    for (i=0;i<nd;i++) {
      if (Tcl_GetIntFromObj(interp,dptr[i],&val) != TCL_OK) {
	switch(xfersize) {
	case 1: ckfree((char *) clist); break;
	case 2: ckfree((char *) slist); break;
	case 4: 
	default:ckfree((char *) ilist);
	}
	return TCL_ERROR;
      }
      switch(dw) {
      case VME_D8:
	clist[i] = (unsigned char) (0xff & val);
	break;
      case VME_D16:
	slist[i] = (unsigned short) (0xffff & val);
	break;
      case VME_D32:
      default:
	ilist[i] = (unsigned int) val;
      }
      
    }
  } else {
    /* Get number of data items to read */
    if (Tcl_GetIntFromObj(interp,dptr[0],&val) != TCL_OK)
      return TCL_ERROR;
    nd = val; 
  }

  /* Now actually read or write */
  if (wrdata) { /* Write data */
    switch(xfersize) {
    case 1:
      retn = _vme_write(as, addr, clist, nd, dw);
      break;
    case 2:
      retn = _vme_write(as, addr, slist, nd, dw);
      break;
    case 4:
    default:
      retn = _vme_write(as, addr, ilist, nd, dw);
    }
    if (retn < 0) {
      Tcl_SetResult(interp,"VME Write Failed",TCL_STATIC);
      return TCL_ERROR;
    }
  } else {      /* Read data */
    switch(xfersize) {
    case 1:
      clist = (unsigned char *) ckalloc(nd * sizeof(char));
      memset(clist,0,nd*sizeof(char));
      retn = _vme_read(as, addr, clist, nd, dw);
      if (retn < 0) {
	Tcl_SetResult(interp,"VME Read Failed",TCL_STATIC);
	return TCL_ERROR;
      }
      /* Return result as a string, rather than as a list */
      if (nd > 0) {
	Tcl_SetObjResult(interp,Tcl_NewStringObj((char *)clist,nd));
      }
      ckfree((char *)clist);
      break;
    case 2:
      slist = (unsigned short *) ckalloc(nd * sizeof(short));
      memset(slist,0,nd*sizeof(short));
      retn = _vme_read(as, addr, slist, nd, dw);
      if (retn < 0) {
	Tcl_SetResult(interp,"VME Read Failed",TCL_STATIC);
	return TCL_ERROR;
      }
      rptr = Tcl_NewObj();
      for (i=0;i<nd;i++) {
	val = (int) slist[i];
	Tcl_ListObjAppendElement(interp,rptr,Tcl_NewIntObj(val));
      }
      ckfree((char *)slist);
      Tcl_SetObjResult(interp,rptr);
      break;
    case 4:
    default:
      ilist = (unsigned int *) ckalloc(nd * sizeof(int));
      memset(ilist,0,nd*sizeof(int));
      retn = _vme_read(as, addr, ilist, nd, dw);
      if (retn < 0) {
	Tcl_SetResult(interp,"VME Read Failed",TCL_STATIC);
	return TCL_ERROR;
      }
      rptr = Tcl_NewObj();
      for (i=0;i<nd;i++) {
	val = ilist[i];
	Tcl_ListObjAppendElement(interp,rptr,Tcl_NewIntObj(val));
      }
      ckfree((char *) ilist);
      Tcl_SetObjResult(interp,rptr);
    }
  }

  return TCL_OK;
}

/*
 * Put serial port in well-defined state.
 */
int
FixTTY(ClientData clientdata, Tcl_Interp *interp,
           int objc, Tcl_Obj * objv[]){
  char * strval;
  int fd, lstr;
  struct termio termio;

  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  if ((fd = open(strval, O_RDWR)) < 0) {
    Tcl_AppendResult(interp,"Unable to open device: ",strval,(char *)0);
    return TCL_ERROR;
  }
 
  termio.c_cflag = B9600 | CS8 | CREAD | CLOCAL;

  termio.c_oflag = 0;
  termio.c_lflag = ICANON;
  termio.c_iflag = ICRNL | BRKINT | IGNBRK;
  termio.c_line  = 0;
  termio.c_cc[0] = 0;
  termio.c_cc[VMIN] = 64;
  termio.c_cc[VTIME] = 1;

  if (ioctl(fd, TCSETA, &termio) != 0) perror("serialOpen:ioctl");

  sleep(1);
  close(fd);

  return TCL_OK;
}

int
Btxvme_Init(Tcl_Interp *interp) {

  if (0 > init_vme()) {
    Tcl_SetResult(interp,"Cannot initialize VME bridge",TCL_STATIC);
    return TCL_ERROR;
  }

  Tcl_CreateObjCommand(interp,"vme",(Tcl_ObjCmdProc *) VME_IO,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp,"fixtty",(Tcl_ObjCmdProc *) FixTTY,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp,"vsc",(Tcl_ObjCmdProc *) vsc,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp,"vsdummy",(Tcl_ObjCmdProc *) vsdummy,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp,"sis", (Tcl_ObjCmdProc *) sis,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp,"oms",(Tcl_ObjCmdProc *) oms,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp,"maxv",(Tcl_ObjCmdProc *) maxv,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp,"mdummy",(Tcl_ObjCmdProc *) mdummy,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp,"nigpib", (Tcl_ObjCmdProc *) ni1014,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp,"rly", (Tcl_ObjCmdProc *) relay,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp,"dgio", (Tcl_ObjCmdProc *) dio,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp,"ipio", (Tcl_ObjCmdProc *) ipio,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateObjCommand(interp,"tip", (Tcl_ObjCmdProc *) TIP,
		       (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
  Tcl_LinkVar(interp,"debug",(char *) &vmedebug,TCL_LINK_INT);
  Tcl_LinkVar(interp,"ibdebug",(char *) &ibdebug, TCL_LINK_INT);

  Tcl_SetObjResult(interp,Tcl_NewStringObj(cvsid,strlen(cvsid)));
  return TCL_OK;
}
