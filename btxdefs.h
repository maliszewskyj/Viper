#ifndef _btxdefs_h
#define _btxdefs_h

#define VME16D08 0
#define VME16D16 1
#define VME16D32 2
#define VME24D08 3
#define VME24D16 4
#define VME24D32 5
#define VME32D08 6
#define VME32D16 7
#define VME32D32 8

#ifdef GEFVME
#define NBTXDEVS 3
#else
#define NBTXDEVS 9
#endif

#endif /* _btxdefs_h */
