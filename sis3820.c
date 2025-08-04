static char cvsid[] = "$Id: vscaler.c,v 1.9 2006/11/17 16:05:10 nickm Exp $";
/*
 * vscaler - code for controlling SIS 3820 scaler
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
#include "sis3820.h"
#include "vme_util.h"

static int scounter = 0;
int sis_preload(ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj * objv[]) {
  int nd, i, val;
  unsigned int addr;
  unsigned int preset1, preset2, presetmask;
  unsigned int counts[32];
  sis_scaler * sptr;
  Tcl_Obj ** dptr;

  if (objc < 3) {
    Tcl_SetResult(interp,"Usage: sis# preload [list]",TCL_STATIC);
    return TCL_ERROR;
  }

  sptr = (sis_scaler *)clientdata;

  if (Tcl_ListObjGetElements(interp,objv[2],&nd,&dptr) != TCL_OK) 
    return TCL_ERROR;
  nd = (nd > 32) ? 32 : nd;
  preset1 = 0;
  preset2 = 0;
  presetmask = 0;
  memset(counts,0,sizeof(counts));
  for (i=0;i<nd;i++) {
    val = 0;
    if (Tcl_GetIntFromObj(interp,dptr[i],&val) != TCL_OK) return TCL_ERROR;
    counts[i] = val;
    if ((i < 16) && (val != 0)) {
      preset1=val;
      presetmask = i & (0xF);
    } else if ((i >= 16) && (val != 0)) {
      preset2=val;
      presetmask += (i-15) << 16;
    }
  }

  // Should probably reset here


  addr = sptr->base + SIS3820_PRESET_GROUP1;
  val = preset1;
  if (_vme_write(VME_A32, addr, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis_arm: PRESET_GROUP1",TCL_STATIC);
    return TCL_ERROR;
  }

  addr = sptr->base + SIS3820_PRESET_GROUP2;
  val = preset2;
  if (_vme_write(VME_A32, addr, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis_arm: PRESET_GROUP2",TCL_STATIC);
    return TCL_ERROR;
  }

  addr = sptr->base + SIS3820_PRESET_CHANNEL_SELECT;
  val = presetmask;
  if (_vme_write(VME_A32, addr, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis_arm: PRESET_GROUP1",TCL_STATIC);
    return TCL_ERROR;
  }

  // Enable Preset Group1
  /*
  addr = sptr->base + SIS3820_PRESET_ENABLE_HIT;
  val = SIS3820_PRESET_STATUS_ENABLE_GROUP1;
  if (_vme_write(VME_A32, addr, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis_arm: PRESET_GROUP1",TCL_STATIC);
    return TCL_ERROR;
  }
  */
  sptr->preset = 1;
  return TCL_OK;
}

int sis_direction(Tcl_Interp * interp, unsigned int base, int objc, Tcl_Obj * objv[]) {

  /* Do NOTHING */
  return TCL_OK;
}

int sis_clear(Tcl_Interp * interp, unsigned int base) {
  unsigned int addr;
  unsigned int val;

  addr = base + SIS3820_COUNTER_CLEAR;
  val = 0x7FFFFFFF;
  if (_vme_write(VME_A32, addr, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis_arm: COUNTER_CLEAR",TCL_STATIC);
    return TCL_ERROR;
  }

  addr = base + SIS3820_KEY_COUNTER_CLEAR;
  val = 0x7FFFFFFF;
  if (_vme_write(VME_A32, addr, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis_arm: KEY_COUNTER_CLEAR",TCL_STATIC);
    return TCL_ERROR;
  }


  return TCL_OK;
}

/* Arm module */
int sis_arm(ClientData clientdata, Tcl_Interp * interp) {
  unsigned int addr;
  unsigned int val;
  sis_scaler * sptr;

  sptr = (sis_scaler *) clientdata;


  addr = sptr->base + SIS3820_PRESET_ENABLE_HIT;
  val = SIS3820_PRESET_STATUS_ENABLE_GROUP1;
  if (_vme_write(VME_A32, addr, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis_arm: PRESET_ENABLE_HIT",TCL_STATIC);
    return TCL_ERROR;
  }

  addr = sptr->base + SIS3820_KEY_OPERATION_ENABLE;
  val = SIS3820_KEY_OPERATION_ENABLE;
  if (_vme_write(VME_A32, addr, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis_arm: KEY_OPERATION_ENABLE",TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/* Disarm module */
int sis_disarm(ClientData clientdata, Tcl_Interp * interp) {
  unsigned int addr;
  unsigned int val;
  sis_scaler * sptr;

  sptr = (sis_scaler *) clientdata;
  addr = sptr->base + SIS3820_KEY_OPERATION_DISABLE;
  val = 1;
  if (_vme_write(VME_A32, addr, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis_arm: KEY_OPERATION_ENABLE",TCL_STATIC);
    return TCL_ERROR;
  }

  sptr->preset = 0;
  return TCL_OK;
}

/* Get counter status */
int sis_status(ClientData clientdata, Tcl_Interp * interp) {
  unsigned int addr;
  unsigned int val;
  int presetreached, isenabled;
  sis_scaler * sptr;
  
  sptr = (sis_scaler *)clientdata;

  addr = sptr->base + SIS3820_PRESET_ENABLE_HIT;
  if (_vme_read(VME_A32, addr, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis_status: SIS_CONTROL_STATUS",TCL_STATIC);
    return TCL_ERROR;
  }
  presetreached = (val & SIS3820_PRESET_LNELATCHED_REACHED_GROUP1) ? 0 : 1;

  addr = sptr->base + SIS3820_CONTROL_STATUS;
  if (_vme_read(VME_A32, addr, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis_status: SIS_CONTROL_STATUS",TCL_STATIC);
    return TCL_ERROR;
  }

  /* If (val & 1) = 1, then we're armed and counting */
  isenabled = (val & 0x00010000) ? 1 : 0;

  //fprintf(stderr," - isenabled = %d presetreached = %d countforpreset = %d\n",isenabled,presetreached,sptr->preset);
  Tcl_SetObjResult(interp,Tcl_NewIntObj(isenabled));
  return TCL_OK;
}

/* Read from  */
int sis_verify(Tcl_Interp * interp, unsigned int base) {
  unsigned int addr;
  unsigned int val;
  unsigned int test;

  addr = base + SIS3820_MODID;
  if (_vme_read(VME_A32, addr, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"vsc_status:read CONTROL",TCL_STATIC);
    return TCL_ERROR;
  }

  test = ((val >> 16) == 0x3820) ? 1 : 0;

  Tcl_SetObjResult(interp,Tcl_NewIntObj(test));
  return TCL_OK;
}


/*
 * Read data from scalers. 
 *
 * Output can be stuffed into one of two places: result
 *                                               list
 *
 */
int sis_read(ClientData clientdata, Tcl_Interp *interp,
            int objc, Tcl_Obj * objv[]) {
  int i;
  unsigned int addr, val;
  sis_scaler * sptr;
  Tcl_Obj * rptr;

  sptr = (sis_scaler *) clientdata;
  addr = sptr->base + SIS3820_COUNTER_CH1;
  
  memset(sptr->counts,0,sizeof(sptr->counts));
  for (i=0;i<sptr->ncounters;i++) {
    if (_vme_read(VME_A32, addr, &val, 1, VME_D32) < 0) {
      Tcl_SetResult(interp,"sis_read:read",TCL_STATIC);
      return TCL_ERROR;
    }
    sptr->counts[i] = val;
    addr += 4;
  }
  
  rptr = Tcl_NewObj();
  for (i=0;i<sptr->ncounters;i++) {
    Tcl_ListObjAppendElement(interp,rptr,Tcl_NewIntObj(sptr->counts[i]));
  }
  Tcl_SetObjResult(interp,rptr);

  return TCL_OK;
}


int sis_command(ClientData clientdata, Tcl_Interp *interp,
            int objc, Tcl_Obj * objv[]) {
  char * strval;
  int lstr;
  sis_scaler * sptr;

  sptr = clientdata;
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
    return sis_arm(clientdata,interp);
  } else if (!strncmp(strval,"disarm",lstr)) {
    return sis_disarm(clientdata,interp);
  } else if (!strncmp(strval,"reset",lstr)) {
    return sis_clear(interp,sptr->base);
  } else if (!strncmp(strval,"status",lstr)) {
    return sis_status(clientdata,interp);
  } else if (!strncmp(strval,"verify",lstr)) {
    return sis_verify(interp,sptr->base);
  } else if (!strncmp(strval,"version",lstr)) {
    Tcl_SetResult(interp,cvsid,TCL_STATIC);
  } else if (!strncmp(strval,"direction",lstr)) {
    return sis_direction(interp,sptr->base,objc,objv);
  } else if (!strncmp(strval,"preload",lstr)) {
    return sis_preload(clientdata,interp,objc,objv);
  } else if (!strncmp(strval,"read",lstr)) {
    return sis_read(clientdata,interp,objc-2,objv+2);
  } else {
    Tcl_SetResult(interp,
                  "Options: arm disarm reset status verify preload read",
                  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}


/* Top level TCL wrapper */
int sis(ClientData clientdata, Tcl_Interp *interp,
           int objc, Tcl_Obj * objv[]){
  char * strval, ctrname[80];
  int lstr;
  int address, ncounters;
  unsigned int val;
  sis_scaler * sptr;

  if (objc < 3) {
    Tcl_SetResult(interp,
                  "Usage: sis create <address>",
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
  ncounters= 32;

  sprintf(ctrname,"sis%d",scounter);
  scounter++;
  sptr = (sis_scaler *) ckalloc(sizeof(sis_scaler));
  sptr->base = address;
  sptr->ncounters = ncounters;
  sptr->preset = 0;

  /* SIS3820 KEY Reset */
  address = sptr->base + SIS3820_KEY_RESET;
  val = 1;
  if (_vme_write(VME_A32, address, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis: KEY RESET",TCL_STATIC);
    return TCL_ERROR;
  }

  // Configure operation register
  address = sptr->base + SIS3820_OPERATION_MODE;
  val = 0;
  if (_vme_write(VME_A32, address, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis: OPERATION_MODE",TCL_STATIC);
    return TCL_ERROR;
  }

  address = sptr->base + SIS3820_KEY_COUNTER_CLEAR;
  val = 0x7FFFFFFF;
  if (_vme_write(VME_A32, address, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis_arm: KEY_OPERATION_ENABLE",TCL_STATIC);
    return TCL_ERROR;
  }

  // Configure operation register
  address = sptr->base + SIS3820_OPERATION_MODE;
  val = SIS3820_OP_MODE_SCALER 
    + SIS3820_CONTROL_INPUT_MODE2
    + SIS3820_CONTROL_OUTPUT_MODE1
    + SIS3820_LNE_SOURCE_INTERNAL_10MHZ
    + SIS3820_NON_CLEARING_MODE;
  if (_vme_write(VME_A32, address, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis: OPERATION_MODE",TCL_STATIC);
    return TCL_ERROR;
  }

  // Configure LNE prescaling
  address = sptr->base + SIS3820_LNE_PRESCALE;
  val = 9999; // 1 kHz
  if (_vme_write(VME_A32, address, &val, 1, VME_D32) < 0) {
    Tcl_SetResult(interp,"sis: LNE_PRESCALE",TCL_STATIC);
    return TCL_ERROR;
  }

  Tcl_CreateObjCommand(interp,ctrname,(Tcl_ObjCmdProc *) sis_command,
                       (ClientData) sptr,NULL);
  Tcl_SetObjResult(interp,Tcl_NewStringObj(ctrname,strlen(ctrname)));
  return TCL_OK;
}
