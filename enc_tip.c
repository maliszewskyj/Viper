static char cvsid[] = "$Id: enc_tip.c,v 1.5 2010/02/12 17:45:10 nickm Exp $";
/*

tip0 position  <axis> ?position?
     position = ((raw - zero)/resolution)*direction
tip0 raw       <axis>
tip0 configure <axis> -resolution     #
                      -zero           #
		      -direction      # 
                      -databits       #
		      -clock          #            deadband in encoder pulses
                      -gray           (on/off)


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
#include "tews_tip114.h"
#include "vme_util.h"

static int enc_counter=0;

void TIP_Init(tip_mod *tptr) {
  int i;

  for (i=0;i<NTAXIS;i++) {
    tptr->axis[i].ctrl.word = DEFCFG;
    tptr->axis[i].zero       = 0;
    tptr->axis[i].direction     = 1;
    tptr->axis[i].resolution = 1.0;
    tptr->axis[i].position   = 0.0;
  }
}

int TIP_Position(ClientData clientdata, Tcl_Interp * interp,
		 int objc, Tcl_Obj * objv[])
{
  int axis, rawpos, cookedpos, cookedzero, retn;
  double position;
  tip_mod * tptr;

  tptr = clientdata;
  if (objc<1) {
    Tcl_SetResult(interp,"Usage: tipx position ?axis?",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&axis) != TCL_OK) return TCL_ERROR;
  if ((axis < 0) || (axis > 9)) {
    Tcl_SetResult(interp,"Error: axis must be between 0 and 9", TCL_STATIC);
    return TCL_ERROR;
  }

  // Start position capture
  retn = TIP_Capture(tptr->base);
  if (retn < 0) {
    Tcl_SetResult(interp,"Timed out reading encoder position",TCL_STATIC);
    return TCL_ERROR;
  }

  if ((retn = TIP_RawPosition(tptr->base,axis,&rawpos)) < 0) {
    Tcl_SetResult(interp,"Error reading encoder position",TCL_STATIC);
    return TCL_ERROR;
  }

  if (rawpos & tptr->axis[axis].signbit) {
    cookedpos = (~(rawpos & tptr->axis[axis].mask)) & tptr->axis[axis].mask;
    cookedpos *= -1;
  } else {
    cookedpos = rawpos;
  }

  if (tptr->axis[axis].zero & tptr->axis[axis].signbit) {
    cookedzero = (~(tptr->axis[axis].zero & tptr->axis[axis].mask)) 
      & tptr->axis[axis].mask;
    cookedzero *= -1;
  } else {
    cookedzero = tptr->axis[axis].zero;
  }
  
  position = ((cookedpos - cookedzero) / tptr->axis[axis].resolution)
    * tptr->axis[axis].direction;

  //printf("TIP_Position: Axis = %d  Raw = %d Cooked = %d  Zero = %d CookedZero = %d\n",
  //	 axis,rawpos,cookedpos,tptr->axis[axis].zero,cookedzero);
  Tcl_SetObjResult(interp,Tcl_NewDoubleObj(position));
  return TCL_OK;
}

int TIP_Axis_Option_Parse(Tcl_Interp * interp,
			   int objc, Tcl_Obj * objv[], 
			   tip_axis * ax)
{
  int i, lstr, optint;
  char * option;
  double optdbl;

  for (i=0;i<objc;i = i + 2) {
    if (objc < i + 1) break;  
    option = Tcl_GetStringFromObj(objv[i],&lstr);
    if (!strncmp("-zero",option, lstr)) {
      if (Tcl_GetIntFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;    
      ax->zero = optint;
    } else if (!strncmp("-resolution",option,lstr)) {
      if (Tcl_GetDoubleFromObj(interp,objv[i+1],&optdbl) != TCL_OK) 
	return TCL_ERROR;
      if (optdbl <= 0) {
	Tcl_SetResult(interp,"Resolution must be greater than zero",TCL_STATIC);
	return TCL_ERROR;
      }
      ax->resolution = optdbl; 
    } else if (!strncmp("-databits",option,lstr)) {
      if (Tcl_GetIntFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;   
      if ((optint < 0) || (optint > 0x21)) {
	Tcl_SetResult(interp,"Databits must be between 1 and 0x20",TCL_STATIC);
	return TCL_ERROR;
      }
      ax->ctrl.fields.databits = optint;
      //printf("databits = 0x%x\n",optint);
    } else if (!strncmp("-direction",option,lstr)) {
      if (Tcl_GetIntFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;   
      if ((optint != -1) && (optint != 1)) {
	Tcl_SetResult(interp,"Direction must be 1 or -1",TCL_STATIC);
	return TCL_ERROR;
      }
      ax->direction = optint;
    } else if (!strncmp("-clockrate",option,lstr)) {
      if (Tcl_GetIntFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;   
      ax->ctrl.fields.clockrate = optint;
    } else if (!strncmp("-gray",option,lstr)) {
      if (Tcl_GetBooleanFromObj(interp,objv[i+1],&optint) != TCL_OK) 
	return TCL_ERROR;
      ax->ctrl.fields.gray = (optint) ? 1 : 0;
    }
  }
  return TCL_OK;
}

int TIP_Axis_Option_Report(Tcl_Interp * interp, tip_axis ax) 
{
  Tcl_Obj * rptr, * iptr;
  int ival;
  double dval;
  rptr = Tcl_NewObj();

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-zero",5));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewIntObj(ax.zero));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-direction",10));
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewIntObj(ax.direction));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-databits",9));
  ival = ax.ctrl.fields.databits;
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewIntObj(ival));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-clockrate",10));
  ival = ax.ctrl.fields.clockrate;
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewIntObj(ival));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-gray",5));
  ival = ax.ctrl.fields.gray;
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewBooleanObj(ival));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  iptr = Tcl_NewObj();
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewStringObj("-resolution",11));
  dval = ax.resolution;
  Tcl_ListObjAppendElement(interp,iptr,Tcl_NewDoubleObj(dval));
  Tcl_ListObjAppendElement(interp,rptr,iptr);

  Tcl_SetObjResult(interp,rptr);
  return TCL_OK;
}

int TIP_Configure(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) 
{
  int axis,i;
  tip_mod * tptr;
  tptr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Specify axis",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&axis) != TCL_OK) return TCL_ERROR;
  if ((axis < 0) || (axis > 9)) {
    Tcl_SetResult(interp,"Error: axis must be between 0 and 9", TCL_STATIC);
    return TCL_ERROR;
  }

  if (objc == 1) {
    /* Print current configuration */
    return TIP_Axis_Option_Report(interp, tptr->axis[axis]);
  }

  if (TIP_Axis_Option_Parse(interp,objc-1,objv+1,&(tptr->axis[axis])) 
      != TCL_OK) return TCL_ERROR;
  if(TIP_WriteConfig(tptr->base,axis,
		     tptr->axis[axis].ctrl.fields.databits,
		     tptr->axis[axis].ctrl.fields.clockrate,
		     tptr->axis[axis].ctrl.fields.gray) < 0) {
    Tcl_SetResult(interp,"Error configuring axis",TCL_STATIC);
    return TCL_ERROR;
  }

  tptr->axis[axis].signbit = (0x1 << (tptr->axis[axis].ctrl.fields.databits-1));
  tptr->axis[axis].mask = 0;
  for (i=0;i<(tptr->axis[axis].ctrl.fields.databits-1);i++)
    tptr->axis[axis].mask += (0x1 << i);
  tptr->axis[axis].maxval = tptr->axis[axis].signbit + tptr->axis[axis].mask;

  return TCL_OK;
}

int TIP_command(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) 
{
  char * strval;
  int lstr,axis,val;
  tip_mod * tptr;

  tptr = clientdata;
  if (objc < 2) {
    Tcl_SetResult(interp,
		  "Options: position configure raw version",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  lstr = strlen(strval);

  if (!strncmp(strval, "position", lstr)) {
    return TIP_Position(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval, "configure",lstr)) {
    return TIP_Configure(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp(strval, "raw", lstr)) {
    if (objc<3) {
      Tcl_SetResult(interp,"Usage: tipx raw ?axis?",TCL_STATIC);
      return TCL_ERROR;
    }
    if (Tcl_GetIntFromObj(interp,objv[2],&axis) != TCL_OK) return TCL_ERROR;
    TIP_RawPosition(tptr->base,axis,&val);
    Tcl_SetObjResult(interp,Tcl_NewIntObj(val));
    return TCL_OK;
  } else if (!strncmp(strval, "version",lstr)) {
    Tcl_SetObjResult(interp,Tcl_NewStringObj(cvsid,strlen(cvsid)));
    return TCL_OK;
  } else {
   Tcl_SetResult(interp,
		  "Options: position configure raw version",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/* Top level TCL wrapper */
int TIP(ClientData clientdata, Tcl_Interp *interp,
           int objc, Tcl_Obj * objv[]){
  char * strval, encname[80];
  int lstr;
  int address;
  tip_mod * tptr;

  if (objc < 3) {
    Tcl_SetResult(interp,
		  "Usage: tip create address",
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

  sprintf(encname,"tip%d",enc_counter);
  enc_counter++;
  tptr = (tip_mod *) ckalloc(sizeof(tip_mod));
  memset(tptr,0,sizeof(tip_mod));
  tptr->base = address;
  TIP_Init(tptr);

  Tcl_CreateObjCommand(interp,encname,(Tcl_ObjCmdProc *) TIP_command,
		       (ClientData) tptr,NULL);
  Tcl_SetObjResult(interp,Tcl_NewStringObj(encname,strlen(encname)));
  return TCL_OK;
}
