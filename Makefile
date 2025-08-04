# $Id: Makefile,v 1.22 2015/06/05 20:16:36 nickm Exp $
# gef--        builds shared object for GE VR12 Single Board Computer BSP
# cct--        builds shared object for Concurrent Technologies BSP
# universe--   builds shared object for VMIVME-7750 Single Board Computer
# vic64--      builds shared object for VMIVME-7587 Single Board Computer
# libbtxvme.so-links object list
# ibio--       
# tech8014--   
# .c.o--
# clean--      removes objects 
# realclean-   removes objects and shared objects
UDEFS   = -DUNIVERSE -DUSEGETSPUTS
CCTDEFS   = -DCCT_UNIVERSE -I/usr/local/linuxvmeen 
GEFDEFS = -DGEFVME
USBDEFS = -DXXUSB -I/usr/include/tcl8.6/
DEFS    = 
CFLAGS  = -g -Wall -fPIC
LOBJ    = btxvme.o vme_util.o vscaler.o omsmot.o oms58.o nigpib.o ni1014.o vmic2210.o relay.o vs64.o vmic2510.o dio.o mdummy.o vsdummy.o oms_maxv.o maxv_mot.o enc_tip.o tews_tip114.o ipio.o acro_ip470.o sis3820.o
ULIBS   = -lvme
CCTLIBS  = -L/usr/local/linuxvmeen/cct_modules -lcctvmeen
USBLIBS = -lxx_usb -lusb
LIBS    = 
GEFLIBS = -L/usr/lib/gef -lvme
LDFLAGS	= -shared -warn-once -fPIC

# Change default to cct
all: cct

xxusb:
	make libbtxvme.so "CFLAGS=$(CFLAGS) $(USBDEFS)" "LIBS = $(LIBS) $(USBLIBS)"

cct:
	make libbtxvme.so "CFLAGS=$(CFLAGS) $(CCTDEFS)" "LIBS = $(LIBS) $(CCTLIBS)"

gef:
	make libbtxvme.so "CFLAGS=$(CFLAGS) $(GEFDEFS)" "LIBS = $(LIBS) $(GEFLIBS)"

universe:
	make libbtxvme.so "CFLAGS=$(CFLAGS) $(UDEFS)"\
	"LIBS = $(LIBS) $(ULIBS)"

vic64:
	make libbtxvme.so "CFLAGS=$(CFLAGS) $(DEFS)"

libbtxvme.so: $(LOBJ)
	ld $(LDFLAGS) -o libbtxvme.so $(LOBJ) $(LIBS)

ibio: ibio.o ni1014.o
	$(CC) $(CFLAGS) -o ibio ibio.o ni1014.o

tech8014: tech8014.o tech8014.h
	$(CC) $(CFLAGS) -o $@ tech8014.o

tstenc: tstenc.c
	$(CC) $(CFLAGS) -o $@ tstenc.c -lm

sis3820_test: sis3820_test.c sis3820.h cct
	$(CC) $(CFLAGS) -o $@ sis3820_test.c vme_util.o -lm $(CCTLIBS)

.c.o:
	$(CC) $(CFLAGS) -c $*.c

clean:
	rm -f *.o *~
realclean: clean
	rm -f *.so ibio
