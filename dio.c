static char rcsid[] = "$Id: dio.c,v 1.6 2015/07/27 19:31:42 nickm Exp $";
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <tcl.h>
#include "vme_util.h"
#include "vmic2510.h"

/*
  dio - code for controlling VMIC2510B TTL digital I/O module

  Extension usage:
  
  dio create ?base? ?writemask?
  d set   ?bit?                  Set bit
  d clear ?bit?                  Clear bit
  d stat  ?bit?                  Get current value of bit
  d read                         Get all bits of data
 */

typedef struct {
  unsigned int base;
  int bits[64];  /* State of bits */
  int wbits[64]; /* Write status of bits */
} dgio;
static int dcounter=0;

int dio_read(ClientData clientdata, Tcl_Interp *interp,
	     int objc, Tcl_Obj * objv[])
{
  dgio * dptr;
  int retn;
  int i, j;
  unsigned char byte;
  unsigned int lword;
  Tcl_Obj * optr;
  dptr = clientdata;

  for (i=0;i<1;i++) {
    retn = VMIC2510_GetWord(dptr->base,i*4,&lword);
    if (retn < 0) {
      Tcl_SetResult(interp,"VMIC2510_GetWord failed to read word",TCL_STATIC);
      return TCL_ERROR;
    }
    for (j=0;j<32;j++) {
      // Assume no byteswapping necessary
      dptr->bits[i*32+j] = (lword & (0x1<<j)) ? 1 : 0;
    }
  }
 
  /* Compose result */
  optr = Tcl_NewObj();
  for (i=0;i<64;i++) {
     Tcl_ListObjAppendElement(interp,optr,
			      Tcl_NewIntObj(dptr->bits[i]));
  }
  Tcl_SetObjResult(interp,optr);
  return TCL_OK;
}

int dio_stat(ClientData clientdata, Tcl_Interp *interp,
	     int objc, Tcl_Obj * objv[])
{
  dgio * dptr;
  int retn,bit,dport,result,i;
  unsigned short word;
  dptr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Usage: diox clear <bit>",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&bit) != TCL_OK) return TCL_ERROR;
  if ((bit < 0) || (bit > 63)) {
    Tcl_SetResult(interp,"Bit value must be between 0 and 63",TCL_STATIC);
    return TCL_ERROR;
  }
  dport  = bit / 16;
  
  retn = VMIC2510_GetShort(dptr->base, dport, &word);
  if (retn < 0) {
    Tcl_SetResult(interp,
		  "VMIC2510_GetShort() failed to read word",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  for (i=0; i<16; i++) {
    result = (word & (0x1 << i)) ? 1 : 0;
    dptr->bits[dport*16 + i] = result;
  }

  Tcl_SetObjResult(interp,Tcl_NewIntObj(dptr->bits[bit]));
  return TCL_OK;
}

int dio_clear(ClientData clientdata, Tcl_Interp *interp,
	      int objc, Tcl_Obj * objv[]) 
{
  dgio * dptr;
  int retn,bit;

  dptr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Usage: diox clear <bit>",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&bit) != TCL_OK) return TCL_ERROR;
  if ((bit < 0) || (bit > 63)) {
    Tcl_SetResult(interp,"Bit value must be between 0 and 63",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Check to see whether it is possible to write to this bit */
  if (!dptr->wbits[bit]) {
    Tcl_SetResult(interp,"Bit is read-only. Set write mask appropriately",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  dptr->bits[bit]=0;
  retn = VMIC2510_SetClrBit(dptr->base, bit, 0);
  if (retn < 0) {
    Tcl_SetResult(interp,
		  "VMIC2510_SetClrBit() failed to set bit",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

int dio_set(ClientData clientdata, Tcl_Interp *interp,
	    int objc, Tcl_Obj * objv[]) 
{
  dgio * dptr;
  int retn,bit;

  dptr = clientdata;
  if (objc < 1) {
    Tcl_SetResult(interp,"Usage: diox set <bit>",TCL_STATIC);
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp,objv[0],&bit) != TCL_OK) return TCL_ERROR;
  if ((bit < 0) || (bit > 63)) {
    Tcl_SetResult(interp,"Bit value must be between 0 and 63",TCL_STATIC);
    return TCL_ERROR;
  }

  /* Check to see whether it is possible to write to this bit */
  if (!dptr->wbits[bit]) {
    Tcl_SetResult(interp,"Bit is read-only. Set write mask appropriately",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  dptr->bits[bit]=1;
  retn = VMIC2510_SetClrBit(dptr->base, bit, 1);
	 
  if (retn < 0) {
    Tcl_SetResult(interp,
		  "VMIC2510_SetClrBit() failed to set bit",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}


int dio_command(ClientData clientdata, Tcl_Interp *interp,
		int objc, Tcl_Obj * objv[]) {
  char * strval;
  int lstr;
  dgio * dptr;

  dptr = clientdata;
  if (objc < 2) {
    Tcl_SetResult(interp,
		  "Options: clear set stat ",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  strval = Tcl_GetStringFromObj(objv[1],&lstr);
  lstr = strlen(strval);

  if (!strncmp("read",strval,lstr)) {
    return dio_read(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp("stat",strval,lstr)) {
    return dio_stat(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp("clear",strval,lstr)) {
    return dio_clear(clientdata,interp,objc-2,objv+2);
  } else if (!strncmp("set",strval,lstr)) {
    return dio_set(clientdata,interp,objc-2,objv+2);
  } else {
    Tcl_SetResult(interp,
		  "Options: clear set stat ",
		  TCL_STATIC);
    return TCL_ERROR;
  }

  return TCL_OK;
}

int dio_init(dgio * dptr, unsigned int mask)
{
  unsigned char dmsk;
  int i, j;
  int on;

  dmsk = mask & 0xFF;
  for (i=0;i<8;i++) {
    on = (mask & (0x1 << i)) ? 1 : 0;
    for (j=0;j<8;j++) {
      dptr->wbits[i*8 + j] = on;
      dptr->bits[i*8 + j] = 0;
    }
  }

  return VMIC2510_Config(dptr->base,1,1,1,dmsk);
  
}


int dio(ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj * objv[])
{
  char * strval, dioname[80];
  int lstr;
  int address;
  dgio * dptr;
  int retn;
  unsigned int maskval;
  int ival;
  

  if (objc < 4) {
    Tcl_SetResult(interp,
		  "Usage: dio create ?address? ?writemask?",
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

  sprintf(dioname,"dio%d",dcounter);
  dcounter++;
  dptr = (dgio *) ckalloc(sizeof(dgio));
  memset(dptr,0,sizeof(dgio));
  dptr->base = address;
  retn = dio_init(dptr,maskval);

  Tcl_CreateObjCommand(interp,dioname,(Tcl_ObjCmdProc *) dio_command,
		       (ClientData) dptr,NULL);
  Tcl_SetObjResult(interp,Tcl_NewStringObj(dioname,strlen(dioname)));
  return TCL_OK;
}
