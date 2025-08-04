/* $Id: vme_util.h,v 1.9 2015/06/05 20:20:52 nickm Exp $ */
#ifndef _vme_util_h
#define _vme_util_h

#ifdef CCT_UNIVERSE // Concurrent technologies
#include "vme_api_en.h"
#elif  UNIVERSE    // VMIC VMISFT
#include <vme/vme.h>
#include <vme/vme_api.h>
#elif  GEFVME
#include "gef/gefcmn_vme.h"
#elif  XXUSB
#include "libxxusb.h"
#else 
#include <signal.h>
#include <stdint.h>


typedef enum
{
  VME_A32SD = 0x0D,             /* A32 supervisory data access */
  VME_A16U  = 0x29,             /* A16 nonpriviledged access */
  VME_A24SD = 0x3D,             /* A24 supervisory data access */
}
vme_addr_mod_t;

//typedef enum
//{
//  VME_A16,                      /* Short VMEbus address space */
//  VME_A24,                      /* Standard VMEbus address space */
//  VME_A32,                      /* Extended VMEbus address space */
//}
//vme_address_space_t;

//typedef enum
//{
//  VME_D8 = 1,                   /* Byte */
//  VME_D16 = 2,                  /* Word */
//  VME_D32 = 4,                  /* Double word */
//  VME_D64 = 8                   /* Quad word */
//}
//vme_dwidth_t;

  typedef struct _vme_bus_handle *vme_bus_handle_t;
  typedef struct _vme_master_handle *vme_master_handle_t;

#endif

#define VME_A16 0
#define VME_A24 1
#define VME_A32 2

#define VME16 0
#define VME24 1
#define VME32 2
#define NDEVS 3
 
#define VME16D08 0
#define VME16D16 1
#define VME16D32 2
#define VME24D08 3
#define VME24D16 4
#define VME24D32 5
#define VME32D08 6
#define VME32D16 7
#define VME32D32 8


#ifndef VME_D8
#define VME_D8   0
#define VME_D16  1
#define VME_D32  2
#define VME_D64  3
#endif

int strtoas (const char *, int *as);
int strtodw (const char *, int *dw);
int vmemcpy (void *, const void *, int, int);
int _vme_read (int, long, void *, int, int);
int _vme_write(int, long, const void *, int, int);
int init_vme();
int vsleep(const unsigned int);

#endif
