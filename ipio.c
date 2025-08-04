static char rcsid[] = "$Id: ipio.c,v 1.1 2013/04/04 18:44:52 nickm Exp $";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <tcl.h>
#include "vme_util.h"
#include "acro_ip470.h"

/*
  ipio - code for controlling Acromag IP470 digital I/O module

  Extension usage:
  
  ipio create ?base? ?writemask?
  d set   ?bit?                  Set bit
  d clear ?bit?                  Clear bit
  d stat  ?bit?                  Get current value of bit
  d read                         Get all bits of data
 */

typedef struct {
  unsigned int base;
  int bits[64];  /* State of bits */
  int wbits[64]; /* Write status of bits */
} ip_io;
static int dcounter=0;

int ipio_read(ClientData clientdata, Tcl_Interp *interp,
             int objc, Tcl_Obj * objv[])
{
  ip_io * iptr;
  int retn;
  int i, j;
  unsigned char byte;
  Tcl_Obj * optr;
  iptr = clientdata;

  for (i=0;i<6;i++) {
    retn = IP470_GetByte(iptr->base,i,&byte);
    if (retn < 0) {
      Tcl_SetResult(interp,"IP470_GetByte failed to read byte",TCL_STATIC);
      return TCL_ERROR;
    }
    for (j=0;j<8;j++) {
      iptr->bits[i*8+j] = (byte & (0x1<<j)) ? 1 : 0;
    }
  }
 
  /* Compose result */
  optr = Tcl_NewObj();
  for (i=0;i<64;i++) {
     Tcl_ListObjAppendElement(interp,optr,
                              Tcl_NewIntObj(iptr->bits[i]));
  }
  Tcl_SetObjResult(interp,optr);
  return TCL_OK;
}

int ipio_stat(ClientData clientdata, Tcl_Interp *interp,
             int objc, Tcl_Obj * objv[])
{
  ip_io * iptr;
  int retn,bit,dport,result,i;
  unsigned char cval;
  iptr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Usage: ipiox clear <bit>",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&bit) != TCL_OK) return TCL_ERROR;
  if ((bit < 0) || (bit > 47)) {
    Tcl_SetResult(interp,"Bit value must be between 0 and 63",TCL_STATIC);
    return TCL_ERROR;
  }
  dport  = bit / 8;
  
  retn = IP470_GetByte(iptr->base, dport, &cval);
  if (retn < 0) {
    Tcl_SetResult(interp,
                  "IP470_GetShort() failed to read word",
                  TCL_STATIC);
    return TCL_ERROR;
  }
  for (i=0; i<6; i++) {
    result = (cval & (0x1 << i)) ? 1 : 0;
    iptr->bits[dport*8 + i] = result;
  }

  Tcl_SetObjResult(interp,Tcl_NewIntObj(iptr->bits[bit]));
  return TCL_OK;
}

int ipio_clear(ClientData clientdata, Tcl_Interp *interp,
              int objc, Tcl_Obj * objv[]) 
{
  ip_io * iptr;
  int retn,bit;

  iptr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Usage: ipiox clear <bit>",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&bit) != TCL_OK) return TCL_ERROR;
  if ((bit < 0) || (bit > 48)) {
    Tcl_SetResult(interp,"Bit value must be between 0 and 63",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Check to see whether it is possible to write to this bit */
  if (!iptr->wbits[bit]) {
    Tcl_SetResult(interp,"Bit is read-only. Set write mask appropriately",
                  TCL_STATIC);
    return TCL_ERROR;
  }

  iptr->bits[bit]=0;
  retn = IP470_SetClrBit(iptr->base, bit, 0);
  if (retn < 0) {
    Tcl_SetResult(interp,
                  "IP470_SetClrBit() failed to set bit",
                  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

int ipio_set(ClientData clientdata, Tcl_Interp *interp,
            int objc, Tcl_Obj * objv[]) 
{
  ip_io * iptr;
  int retn,bit;

  iptr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Usage: ipiox set <bit>",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&bit) != TCL_OK) return TCL_ERROR;
  if ((bit < 0) || (bit > 63)) {
    Tcl_SetResult(interp,"Bit value must be between 0 and 63",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Check to see whether it is possible to write to this bit */
  if (!iptr->wbits[bit]) {
    Tcl_SetResult(interp,"Bit is read-only. Set write mask appropriately",
                  TCL_STATIC);
    return TCL_ERROR;
  }
  iptr->bits[bit]=1;
  retn = IP470_SetClrBit(iptr->base, bit, 1);
         
  if (retn < 0) {
    Tcl_SetResult(interp,
                  "IP470_SetClrBit() failed to set bit",
                  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}


int ipio_command(ClientData clientdata, Tcl_Interp *interp,
                int objc, Tcl_Obj * objv[]) {
  char * strval;
  int lstr;
  ip_io * iptr;

  iptr = clientdata;
  if (objc < 2) {
    Tcl_SetResult(interp,
                  "Options: clear set stat ",
                  TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  lstr = strlen(strval);

  if (!strncmp("read",strval,lstr)) {
    return ipio_read(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp("stat",strval,lstr)) {
    return ipio_stat(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp("clear",strval,lstr)) {
    return ipio_clear(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp("set",strval,lstr)) {
    return ipio_set(clientdata,interp,objc-2,objv+2);
  } else {
    Tcl_SetResult(interp,
                  "Options: clear set stat ",
                  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

int ipio_init(ip_io * iptr, unsigned int mask)
{
  unsigned char dmsk;
  int i, j;
  int on;

  dmsk = mask & 0xFF;
  for (i=0;i<6;i++) {
    on = (mask & (0x1 << i)) ? 1 : 0;
    for (j=0;j<8;j++) {
      iptr->wbits[i*8 + j] = on;
      iptr->bits[i*8 + j] = 0;
    }
  }

  return IP470_Config(iptr->base,dmsk);
  
}

int ipio(ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj * objv[])
{
  char * strval, ipioname[80];
  int lstr;
  int address;
  ip_io * iptr;
  int retn;
  unsigned int maskval;
  int ival;
  

  if (objc < 4) {
    Tcl_SetResult(interp,
                  "Usage: ipio create ?address? ?writemask?",
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

  /* Get write mask: 8 bit value */
  if (Tcl_GetIntFromObj(interp,objv[3],&ival) != TCL_OK) return TCL_ERROR;
  maskval = (unsigned int) ival;

  sprintf(ipioname,"ipio%d",dcounter);
  dcounter++;
  iptr = (ip_io *) ckalloc(sizeof(ip_io));
  memset(iptr,0,sizeof(ip_io));
  iptr->base = address;
  if ((retn = ipio_init(iptr,maskval)) < 0) {
    fprintf(stderr,"Error initializing IP470 at 0x%04x\n",address);
  }

  Tcl_CreateObjCommand(interp,ipioname,(Tcl_ObjCmdProc *) ipio_command,
                       (ClientData) iptr,NULL);
  Tcl_SetObjResult(interp,Tcl_NewStringObj(ipioname,strlen(ipioname)));
  return TCL_OK;
}
