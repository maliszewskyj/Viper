static const char * cvsid = "$Id: vme_util.c,v 1.14 2015/06/05 20:20:41 nickm Exp $";

/*
 *
 * vme_util.c is a utility program to accomandate the use of universe device 
 * driver.Previously read() write() and lseek() commands were available on 
 * the vic64 device driver and were used to find, retrieve and write data. 
 * These commands are not available on the  universe driver. Three new 
 * functions were created to allow VMEbus reads and writes. 
 *
 * ALL THREE NEW FUNCTIONS USE THE UNIVERSE SYMBOL 
 * CREATED BY THE MAKE COMMAND  
 * (UNIVERSE symbol is only defined when using the universe device driver)
 * 
 * vme_init initializes the VMEbus. If the UNIVERSE symbol is defined the 
 * function creates and maps three master windows . If the UNIVERSE symbol 
 * is not defined, then this function opens the nine devices created by 
 * vic64 device driver. 
 *
 * vme_read & vme_write are mostly alike with the exception of which 
 * variable is the source or destination. These functions first distinguish 
 * which address space to use. If UNIVERSE is defined, the function looks up
 * which master window pointer to use.  The location of the data, master 
 * window pointer, number of elements and data width are all passed to 
 * vmemcpy function.  Otherwise, the correct device is selected using address
 * space and data width. Vic64 uses lseek to set a pointer to the correct 
 * address, then either reads or writes  the data.
 *
 * vmemcpy is only used if UNIVERSE is defined. This is where the universe 
 * device determines which data width to use. Once the correct data width is
 * selected, the correct sized address pointer is created and assigned to 
 * either the source or destination address depending on function called.
 * Using the number of elements, the data is copied through the pointers. 
 *
 */

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>
#include <byteswap.h>
#include "vme_util.h"
#include "btxdefs.h"

//#define TRACE(...) if (omsdebug) { fprintf(stderr,__VA_ARGS__); }

#define A16OFFSET 0
#define A24OFFSET 0
#define A32OFFSET 0x01000000

#ifdef CCT_UNIVERSE
// Note that apparently in the more recent versions of the Concurrent
// Technology board support (TSipackage byte transfers are not supported?
int                 v_ctlhandle;
int                 v_handle[NDEVS];
int                 btxfd[NBTXDEVS];
static int fd;
uint8_t *           vptr[NDEVS];
char * btxdev[NBTXDEVS] = {
  "null",
  "lsi0",
  "lsi1",
  "null",
  "null",
  "null",
  "null",
  "lsi4",
  "lsi5"
};
#elif   GEFVME
GEF_VME_BUS_HDL     hdl;
GEF_VME_MASTER_HDL  btxfd[NBTXDEVS], master_hdl = NULL;
GEF_VME_MASTER_HDL  fd;
GEF_VME_BUS_TIMEOUT bus_timeout;
GEF_STATUS          status;
#elif   XXUSB
#include "libxxusb.h"
xxusb_device_type   devices[100];
struct usb_device   *usbDev;
usb_dev_handle      *uDev;
#else
vme_bus_handle_t    bus_handle;
vme_master_handle_t v_handle[NDEVS];
#endif

int vmedebug;

/*===========================================================================
 * Convert an address space string to it's integer value
 * Returns: 0 or -1
 */
int strtoas (const char *str,       /* String expression to be converted */
	     int *as)               /* Result of conversion */
{
  char *table[] = {
    "VME_A16", "VME_A24", "VME_A32"
  };
  int ii;

  for (ii = 0; ii <= VME_A32; ++ii)
    if (table[ii])
      {
        if (0 == strcmp (str, table[ii]))
          {
            *as = ii;
            return (0);
          }
      }

  return (-1);
}

/*===========================================================================
 * Convert a data width string argument to a data width value
 * Returns: 0 or -1
 */
int
strtodw (const char *str,       /* String to be converted */
         int *dw)               /* Result of conversion */
{

  char *table[] = {
    NULL, "VME_D8", "VME_D16", NULL, "VME_D32", NULL, NULL, NULL, "VME_D64"
  };
  int ii;

  for (ii = 0; ii <= 8; ++ii)
    if (table[ii]) {
      if (0 == strcmp (str, table[ii])) {
	switch(ii){
	case 1: *dw = VME_D8;  break;
	case 2: *dw = VME_D16; break;
	case 4: *dw = VME_D32; break;
	case 8: *dw = VME_D64; break;
	default:
	  *dw = 0;
	}
	
	return (0);
      }
    }

  return (-1);
}

/*=======================================================================*/
int init_vme()
{

#ifdef UNIVERSE
  /* Initialize VME UNIVERSE bridge */
  if (0 > vme_init(&bus_handle)) 
    {
      return -1;
    }
  /*bus_handle allocated by vme_int
   *&v_handle[]pointer to the window to allocate
   *0x0000 VMEbus address
   *VME_A16U address modifier (Short supervisory access)
   *0x10000 window size
   *VME_CTL_PWEN flags (enable posted writes)
   *NULL physical address (driver attempts ro find valid space)
   */
  if (0> vme_master_window_create(bus_handle,
				  &v_handle[VME16],
				  0x0000,
				  VME_A16U,
				  0x10000,
				  VME_CTL_PWEN, 
				  NULL)) 
    {
      vme_term(bus_handle);
      return -2;
    }

  if (NULL == (vptr[VME16] = vme_master_window_map(bus_handle,
						   v_handle[VME16],
						   0))) 
    {
      vme_master_window_release(bus_handle,v_handle[VME16]);
      vme_term(bus_handle);
      return -3;
    }
  /*bus_handle allocated by vme_int
   *&v_handle[]pointer to the window to allocate
   *0x0000 VMEbus address
   *VME_A24SD address modifier (Stardard supervisory data access)
   *0x10000 window size
   *VME_CTL_PWEN flags (enable posted writes)
   *NULL physical address (driver attempts ro find valid space)
   */
  if (0> vme_master_window_create(bus_handle,
				  &v_handle[VME24],
				  0x0000,
				  VME_A24SD,
				  0x10000,
				  VME_CTL_PWEN,
				  NULL)) 
    {
      vme_term(bus_handle);
      return -2;
    }

  if (NULL == (vptr[VME24] = vme_master_window_map(bus_handle,
						   v_handle[VME24],
						   0))) 
    {
      vme_master_window_release(bus_handle,v_handle[VME24]);
      vme_term(bus_handle);
      return -3;
    }
  /*bus_handle allocated by vme_int
   *&v_handle[]pointer to the window to allocate
   *0x0000 VMEbus address
   *VME_A32SD address modifier (Extended supervisory data access)
   *0x10000 window size
   *VME_CTL_PWEN flags (enable posted writes)
   *NULL physical address (driver attempts ro find valid space)
   */
  if (0> vme_master_window_create(bus_handle,
				  &v_handle[VME32],
				  0x0000,
				  VME_A32SD,
				  0x10000,
				  VME_CTL_PWEN,
				  NULL)) 
    {
      vme_term(bus_handle);
      return -2;
    }

  if (NULL == (vptr[VME32] = vme_master_window_map(bus_handle,
						   v_handle[VME32],
						   0))) 
    {
      vme_master_window_release(bus_handle,v_handle[VME32]);
      vme_term(bus_handle);
      return -3;
    }
#elif CCT_UNIVERSE
  int i;
  int devHandle;
  int result;
  EN_PCI_IMAGE_DATA idata;

  if ((v_ctlhandle=vme_openDevice("ctl"))>0) {
      unsigned char swap = 0x28;
      printf("Enabling byteswap...");
      if ((result = vme_setByteSwap(v_ctlhandle,swap)) != 0) {
	printf("could not enable: %d\n",result);
      } else printf("done\n");
  }


  for (i=0;i<NBTXDEVS;i++) {
    if (!strcmp(btxdev[i],"null")) {
      btxfd[i] = -1;
      continue;
    } 
    devHandle=vme_openDevice(btxdev[i]);
    if (devHandle < 0) {
      printf("Could not open device \"%s\"\n",btxdev[i]);
      return -1;
    } else {

      idata.pciAddress=0;
      idata.pciAddressUpper=0;
      idata.vmeAddress=0x0000;
      idata.vmeAddressUpper=0;
      idata.size = 0x10000;
      idata.sizeUpper = 0;
      idata.readPrefetch=0;
      idata.prefetchSize= 0;
      idata.sstMode = 0;
      idata.dataWidth = EN_VME_D16;
      idata.addrSpace = EN_VME_A16;
      idata.type = EN_LSI_DATA;
      idata.mode = EN_LSI_USER;
      idata.vmeCycle = 0;
      idata.sstbSel =0;
      idata.ioremap = 1;

      switch(i) {
      case VME16D08:
	idata.dataWidth = EN_VME_D8;   
	idata.addrSpace = EN_VME_A16;  
	break;
      case VME16D16:
	idata.dataWidth = EN_VME_D16;  
	idata.addrSpace = EN_VME_A16;  
	break;
      case VME16D32:
	idata.dataWidth = EN_VME_D32;  
	idata.addrSpace = EN_VME_A16;  
	break;
      case VME24D08:
	idata.dataWidth = EN_VME_D8;   
	idata.addrSpace = EN_VME_A24;  
	break;
      case VME24D16:
	idata.dataWidth = EN_VME_D16;  
	idata.addrSpace = EN_VME_A24;  
	break;
      case VME24D32:
	idata.dataWidth = EN_VME_D32;  
	idata.addrSpace = EN_VME_A24;  
	break;
      case VME32D08:
	idata.dataWidth = EN_VME_D8;   
	idata.addrSpace = EN_VME_A32;  
	idata.vmeAddress = A32OFFSET;
	idata.size      = 0x80000;
	break;
      case VME32D16:
	idata.dataWidth = EN_VME_D16;  
	idata.addrSpace = EN_VME_A32;  
	idata.vmeAddress = A32OFFSET;
	idata.size      = 0x80000;
	break;
      case VME32D32:
      default:
	idata.dataWidth = EN_VME_D32;  
	idata.addrSpace = EN_VME_A32;
	idata.vmeAddress = A32OFFSET;
	idata.size      = 0x80000;
	break;
      }
      result = vme_enablePciImage(devHandle, &idata);
      if (result < 0) {
	printf("vme_enablePciImage %d (%s)\n",i,strerror(errno));
	return -2;
      }
      btxfd[i] = devHandle;
      
    }
  }
#elif GEFVME
  int i;
  GEF_VME_ADDR addr;
  GEF_UINT32 size = 0x10000;

  status = gefVmeOpen(&hdl);
  
  if (status != GEF_STATUS_SUCCESS) {
      printf("gefVmeOpen failed: %x\n",status);
      return -1;
  }

  bus_timeout = GEF_VME_BUS_TIMEOUT_16us;
  status = gefVmeSetBusTimeout (hdl, bus_timeout);
  if (status != GEF_STATUS_SUCCESS) {
    printf("gefVmeSetBusTimeout failed: %x\n",status);
    return -4;
  }
  printf("Set bus timeout to %dus\n", bus_timeout);

  addr.upper = 0x0000;
  addr.lower = 0x0000;
  addr.addr_mode     = GEF_VME_ADDR_MODE_DATA;
  addr.transfer_mode = GEF_VME_TRANSFER_MODE_SCT;
  addr.broadcast_id  = GEF_VME_BROADCAST_ID_DISABLE;
  addr.flags         = GEF_VME_WND_PCI_IO_SPACE | 
    GEF_VME_WND_EXCLUSIVE | GEF_VME_WND_PWEN;
  addr.vme_2esst_rate = GEF_VME_2ESST_RATE_INVALID;

  for (i=0;i<NBTXDEVS;i++) {
    switch(i) {
    case VME_A16:
      addr.transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32;
      addr.addr_space = GEF_VME_ADDR_SPACE_A16;
      break;
    case VME_A24:
      addr.transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32;
      addr.addr_space = GEF_VME_ADDR_SPACE_A24;	
      break;
    case VME_A32:
    default:
      addr.transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32;
      addr.addr_space = GEF_VME_ADDR_SPACE_A32;
      break;
    }

    status = gefVmeCreateMasterWindow(hdl,
				      &addr,
				      size,
				      &master_hdl);
    if (status != GEF_STATUS_SUCCESS) {
      switch(GEF_GET_ERROR(status)) {
      case GEF_STATUS_NOT_SUPPORTED:
	printf("gefVmeCreateMasterWindow() not supported in this package\n");
	break;
      case GEF_STATUS_BAD_PARAMETER_1:
	printf("gefVmeCreateMasterWindow() reports invalid bus handle\n");
	break;
      case GEF_STATUS_BAD_PARAMETER_2:
	printf("gefVmeCreateMasterWindow() reports invalid VMEbus address");
	break;
      case GEF_STATUS_BAD_PARAMETER_3:
	printf("gefVmeCreateMasterWindow() reports invalid size\n");
	break;
      case GEF_STATUS_BAD_PARAMETER_4:
	printf("gefVmeCreateMasterWindow() reports invalid master handle\n");
	break;
      default:
	printf("gefVmeCreateMasterWindow() not supported in this package\n");
      }
    } else {
      btxfd[i] = master_hdl;

      // Software byteswapping mode
      // status = gefVmeSetReadWriteByte(master_hdl, GEF_ENDIAN_BIG);
    }
  }

#elif XXUSB
  int i;
  char * Serial;

  xxusb_devices_find(devices);
  usbDev = devices[0].usbdev;
  uDev = xxusb_device_open(usbDev);

  if (!uDev) {
    printf("No VM_USB present!\r\n");
    exit(1);
  }

  Serial = devices[0].SerialString;
  printf("VM_USB Serial %s\n",Serial);


#else
  int i;

  /* Open vme devices */
  for (i=0;i<NBTXDEVS;i++) {
    if ((btxfd[i] = open(btxdev[i],O_RDWR)) < 0) {
      perror("Open:");
      exit(1);
    }
  }

#ifdef MAPMEM
  if ((vme_mem = (unsigned char *) malloc(VME_WINSIZE)) == NULL) {
    perror("Can't malloc vme_mem");
    exit(1);
  }
  printf("Mapped Memory\n");
  /* Align the memory to a page */
  if ((vme_mem = (unsigned char *) PAGE_ALIGN((unsigned long) vme_mem))
      ==NULL){
    perror("Can't page align vme_mem");
    exit(1);
  }

  /* Map allocated memory to physical memory */
  if ((vme_mem = (unsigned char *) mmap((caddr_t) vme_mem,
					VME_WINSIZE,
					PROT_READ|PROT_WRITE,
					MAP_SHARED|MAP_FIXED,
					btxfd[0],
					0x0)) == NULL) {
    perror("Can't map physical memory.");
    exit(1);
  }
#endif      /*  MAPMEM */ 
#endif      /* UNIVERSE*/
  return 0; /* Success */
}

/*===========================================================================
 * Copy data using the width specified
 * Returns: 0 or -1
 */
int
vmemcpy (void *dest,            /* Copy to */
         const void *src,       /* Copy from */
         int nelem,             /* Number of data width elements to copy */
         int dw                 /* Data width for each copy operation */
  )
{
  int ii;

  if (vmedebug&4) { 
    printf("vmemcpy: src = 0x%08lx dest = 0x%08lx nelem = %d dw = %d\n",
	   (unsigned long) src, (unsigned long) dest, nelem, dw); }
  switch (dw)
    {
    case VME_D8:
      {
        const uint8_t *s = src;
        uint8_t *d = dest;

        for (ii = 0; ii < nelem; ++ii, ++s, ++d)
          *d = *s;
      }
      break;
    case VME_D16:
      {
        const uint16_t *s = src;
        uint16_t *d = dest;

        for (ii = 0; ii < nelem; ++ii, ++s, ++d)
          *d = *s;
      }
      break;
    case VME_D32:
      {
        const uint32_t *s = src;
        uint32_t *d = dest;

        for (ii = 0; ii < nelem; ++ii, ++s, ++d)
          *d = *s;
      }
      break;
    case VME_D64:
      {
        const uint64_t *s = src;
        uint64_t *d = dest;

        for (ii = 0; ii < nelem; ++ii, ++s, ++d)
          *d = *s;
      }
      break;
    default:
      errno = EINVAL;
      return (-1);
    }
  return (0);
}
/*******************************************************************************/
void vme_debug(int offset, 
	       const void * data, 
	       int nelem, 
	       int dw,
	       int rdwr) 
{
  int ii;
  char ch;
  ch = (rdwr) ? 'W' : 'R';

  switch (dw)
    {
    case VME_D8:
      {
        const uint8_t *d = data;
	char och;
        for (ii = 0; ii < nelem; ii++, offset+=sizeof(uint8_t), d++) {
	  och = (isprint(*d)) ? (*d) : ' ';
	  printf("%c 0x%08x 0x%02x (%c)\n", ch, offset, *d,och);
	}
      }
      break;
    case VME_D16:
      {
        const uint16_t *d = data;
        for (ii = 0; ii < nelem; ii++, offset+=sizeof(uint16_t), d++)
	  printf("%c 0x%08x 0x%04x\n", ch, offset, *d);
      }
      break;
    case VME_D32:
      {
        const uint32_t *d = data;
        for (ii = 0; ii < nelem; ii++, offset+=sizeof(uint32_t), d++)
	  printf("%c 0x%08x 0x%08x\n", ch, offset, *d);
      }
      break;
    case VME_D64:
    default:
      errno = EINVAL;
    }
}


/*=======================================================================*/
int _vme_read(int as,      /* Address space */
	     long addr,   /* Address */
	     void * data, /* Data to be read */
	     int nelems,  /* Number of elements to transfer */
	     int dw)      /* Data width */
{
#ifdef UNIVERSE
  uint8_t * ptr;
  switch(as){
  case VME16: ptr = vptr[VME16]; break;
  case VME24: ptr = vptr[VME24]; break;
  case VME32: ptr = vptr[VME32]; break;
  default: return -1; 
  }
  // Store result of vmemcpy
  int cpySuccess = vmemcpy(data, ptr + addr, nelems, dw); 
  if (vmedebug) vme_debug(addr, data, nelems, dw, 0);
  return cpySuccess;
#elif CCT_UNIVERSE
  long ad;
  int bytes;

  ad = addr;
  switch(as){
  case VME16: 
    ad = addr - A16OFFSET;
    switch(dw){
    case VME_D8:fd =  btxfd[VME16D08]; bytes = nelems; break;
    case VME_D16:fd = btxfd[VME16D16]; bytes = 2 * nelems; break;
    case VME_D32:fd = btxfd[VME16D32]; bytes = 4 * nelems; break;
  }break;
  case VME24: 
    ad = addr - A24OFFSET;
    switch(dw){
    case VME_D8:fd =  btxfd[VME24D08]; bytes = nelems; break;
    case VME_D16:fd = btxfd[VME24D16]; bytes = 2 * nelems; break;
    case VME_D32:fd = btxfd[VME24D32]; bytes = 4 * nelems; break;
  }break;
  case VME32: 
    ad = addr - A32OFFSET;
    switch(dw){
    case VME_D8:fd =  btxfd[VME32D08]; bytes = nelems; break;
    case VME_D16:fd = btxfd[VME32D16]; bytes = 2 * nelems; break;
    case VME_D32:fd = btxfd[VME32D32]; bytes = 4 * nelems; break;
  }break;
  default:
    return -3; // Bad AM
  }
  int retn;
  if (fd <= 0) return -1; /* Uninitialized device */
  if ((retn=vme_read(fd,ad,data,bytes)) != bytes) {
    if (vmedebug) 
      printf("_vme_read: expected %d bytes, but received %d instead\n",bytes,retn);
    return -2;
  }
#elif  GEFVME
  switch(as){
  case VME16: fd = btxfd[VME_A16]; break;
  case VME24: fd = btxfd[VME_A24]; break;
  case VME32: fd = btxfd[VME_A32]; break;
  default:
    return -3; // Bad AM
  }
  switch(dw){
  case VME_D8:
    status = gefVmeRead8(fd, addr, nelems, (GEF_UINT8 *)data);
    break;
  case VME_D16:
    status = gefVmeRead16(fd, addr, nelems, (GEF_UINT16 *)data);
    break;
  case VME_D32:
    status = gefVmeRead32(fd, addr, nelems, (GEF_UINT32 *)data);
    break;
  default:
    return -4; // Bad width
  }
  if (status != GEF_STATUS_SUCCESS) {
    if (vmedebug) {
      switch(GEF_GET_ERROR(status)) {
      case GEF_STATUS_NOT_SUPPORTED:
	printf("gefVmeRead() not supported in this package\n");
	break;
      case GEF_STATUS_BAD_PARAMETER_1:
	printf("gefVmeRead() reports invalid master handle\n");
	break;
      case GEF_STATUS_BAD_PARAMETER_2:
	printf("gefVmeRead() reports invalid length\n");
	break;
      case GEF_STATUS_BAD_PARAMETER_3:
	printf("gefVmeRead() reports invalid length\n");
	break;
      case GEF_STATUS_BAD_PARAMETER_4:
	printf("gefVmeRead() reports invalid pointer\n");
	break;
      default:
	printf("gefVmeRead() reports unspecified error\n");
      }
      return -2;
    }

      printf("_vme_read: gefVmeRead() returned code %d\n",status);
    return -2;
  }
#elif XXUSB
  unsigned short am;
  short retn;
  unsigned short *sptr;
  unsigned int *iptr;
  long lData;
  int i;
  switch(as){
  case VME16: am = 0x29; break; // A16 nonprivileged access 
  case VME24: am = 0x3D; break; // A24 supervisory data access
  case VME32: am = 0x0D; break; // A32 supervisory data access
  default: return -1; 
  }

  switch(dw){
  case VME_D16:
    sptr = (unsigned short *) data;
    for (i=0;i<nelems;i++) {
      retn = VME_read_16(uDev, am, addr, &lData);
      //printf("vme_read(VME_D16) 0x%08x : 0x%08x\n",addr,lData);
      *sptr = (unsigned short) lData;
      addr += 2;
      sptr++;
    }
    break;
  case VME_D32:
    iptr = (unsigned int *) data;
    for (i=0;i<nelems;i++) {
      retn = VME_read_32(uDev, am, addr, &lData);
      //printf("vme_read(VME_D32) 0x%08x : 0x%08x\n",addr,lData);
      *iptr = (unsigned int) lData;
      addr += 4;
      iptr++;
    }
    break;
  case VME_D8:
  default:
    printf("vme_read(VME_D8) Bad width\n");
    return -4; // Bad width
  }  
#else
  if (lseek(fd,addr,0) < 0) return -1;
  if (read(fd,data,nelems) != nelems) return -2;
#endif

  if (vmedebug) vme_debug(addr, data, nelems, dw,0);
  return (0);
}

/*=======================================================================*/
int _vme_write(int as,            /* Address space */
	      long addr,         /* Address */
	      const void * data, /* Data to be read */
	      int nelems,        /* Number of elements to transfer */
	      int dw)            /* Data width */
{
#ifdef UNIVERSE
  uint8_t * ptr;
  int retn;
  switch(as){
  case VME16: ptr = vptr[VME16]; break;
  case VME24: ptr = vptr[VME24]; break;
  case VME32: ptr = vptr[VME32]; break;
  default: return -1; 
  }
  //store result of vmemcpy
  int cpySuccess = vmemcpy(ptr + addr, data, nelems, dw); 
  if (vmedebug) vme_debug(addr, data, nelems, dw,1);
  return cpySuccess;
#elif CCT_UNIVERSE
  long ad;
  int bytes,retn;
  switch(as){
  case VME16: 
    ad = addr - A16OFFSET;
    switch(dw){
    case VME_D8:fd =  btxfd[VME16D08]; bytes = nelems; break;
    case VME_D16:fd = btxfd[VME16D16]; bytes = 2 * nelems; break;
    case VME_D32:fd = btxfd[VME16D32]; bytes = 4 * nelems; break;
  }break;
  case VME24: 
    ad = addr - A24OFFSET;
    switch(dw){
    case VME_D8:fd =  btxfd[VME24D08]; bytes = nelems; break;
    case VME_D16:fd = btxfd[VME24D16]; bytes = 2 * nelems; break;
    case VME_D32:fd = btxfd[VME24D32]; bytes = 4 * nelems; break;
  }break;
  case VME32: 
    ad = addr - A32OFFSET;
    switch(dw){
    case VME_D8:fd =  btxfd[VME32D08]; bytes = nelems; break;
    case VME_D16:fd = btxfd[VME32D16]; bytes = 2 * nelems; break;
    case VME_D32:fd = btxfd[VME32D32]; bytes = 4 * nelems; break;
  }break;
  }
  if (fd <= 0) return -1; /* Uninitialized device */
  if ((retn = vme_write(fd, ad, (UINT8 *)data, bytes)) != bytes) {
    if (vmedebug) printf("vme_write: write failed (%d)\n",retn);
    return -2;
  }
#elif  GEFVME
  switch(as){
  case VME16: fd = btxfd[VME_A16]; break;
  case VME24: fd = btxfd[VME_A24]; break;
  case VME32: fd = btxfd[VME_A32]; break;
  default:
    return -3; // Bad AM
  }
  switch(dw){
  case VME_D8:
    status = gefVmeWrite8(fd, addr, nelems, (GEF_UINT8 *)data);
    break;
  case VME_D16:
    status = gefVmeWrite16(fd, addr, nelems, (GEF_UINT16 *)data);
    break;
  case VME_D32:
    status = gefVmeWrite32(fd, addr, nelems, (GEF_UINT32 *)data);
    break;
  default:
    return -4; // Bad width
  }
  if (status != GEF_STATUS_SUCCESS) {
    if (vmedebug) printf("_vme_write: write failed (%d)\n",status);
    return -5;
  }
#elif XXUSB
  unsigned short am;
  short retn;
  unsigned short *sptr;
  unsigned int *iptr;
  long lData;
  int i;
  switch(as){
  case VME16: am = 0x29; break; // A16 nonprivileged access 
  case VME24: am = 0x3D; break; // A24 supervisory data access
  case VME32: am = 0x0D; break; // A32 supervisory data access
  default: return -1; 
  }

  switch(dw){
  case VME_D16:
    sptr = (unsigned short *) data;
    for (i=0;i<nelems;i++) {
      lData = (long) *sptr;
      retn = VME_write_16(uDev, am, addr, lData);
      addr += 2;
      sptr++;
    }
    break;
  case VME_D32:
    iptr = (unsigned int *) data;
    for (i=0;i<nelems;i++) {
      lData = (long) *iptr;
      retn = VME_write_32(uDev, am, addr, lData);
      addr += 4;
      iptr++;
    }
    break;
  case VME_D8:
  default:
    return -4; // Bad width
  }  
#else
  if (lseek(fd,addr,0) < 0) return -1;
  write(fd,data,nelems);
#endif
  if (vmedebug) vme_debug(addr, data, nelems, dw,1);
  return (0);
}

int vsleep(unsigned int usec)
{
  struct timespec tm,tr;

  tm.tv_sec = 0;
  tm.tv_nsec = usec * 1000; /* Turn into microseconds */
  return nanosleep(&tm,&tr);
}


