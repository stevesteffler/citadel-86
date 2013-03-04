#
# MAKEFILE
#
#  PURPOSE: for Citadel-86 main executables.  Utility makefile is separate.
#
#  This makefile is meant for use in automatic compilation of Citadel-86.  Since
#  this version of Citadel-86 is written in the Turbo C dialect, this makefile
#  is tailored toward an MS-DOS environment using Turbo C.  However, I've tried
#  to keep the system dependent code as isolated as is possible in a makefile.
#  I also believe that I have not violated any of the UNIX makefile rules with
#  this file, outside of the backslashes necessary for DOS.
#
#  All Citadel-specific source files, including the headers, are expected to be
#  in the current directory; all system (stdio.h, etc.) are expected to be in
#  the $(INC) directories (or the current directory).
#
#  For those of you who like clean, tidy makefiles: Sorry.  It works.  It don't
#  get in the way.  Good 'nuff fer me.
#
#  Note that CTDLANSI.H and ANSISYS.H play no part in this file.  I don't like
#  doing large recompiles incessantly.
#
#  Comment from current project maintainer (Steve Steffler): We would like to
#  re-integrate the aforementioned CTDLANSI.H and ANSISYS.H files into this 
#  makefile, since compile time for the project is no longer a concern.
#
STDINC=\tc\include
LIB_INC=\tc\lib
CC=\tc\bin\tcc -D -ml -O -wpro -wuse -j2
INC=-I$(STDINC) -I$(LIB_INC)  
HEADERS=ctdl.h sysdep.h slist.h

PORT=	arch.obj areas.obj \
        bio.obj \
        calllog.obj cc.obj cn.lib compact.obj ctdl.obj \
	domains.obj dom-core.obj dom-init.obj dom-data.obj \
	events.obj \
	floors.obj \
	hot_help.obj \
	info.obj infodisp.obj \
	log.obj \
	mail.obj mailfwd.obj misc.obj modem.obj msg.obj msgnfmt.obj \
	netcache.obj netcall.obj netedit.obj netmisc.obj netrcv.obj netitl.obj \
	offline.obj \
	rooma.obj roomb.obj route.obj \
	shared.obj \
	tools.lib \
	virt.obj virt2.obj vortex.obj

.c.obj:
   $(CC) -c -d $(INC) $<
#   sedd $<

NORMAL: ctdl.exe

ALL: ctdl.exe confg.exe c86door.exe

ctdl.exe: $(PORT) sysdep2.obj sysdep1.obj citvid.obj sysarc.obj splitf.obj \
		syszm.obj sysdoor.obj sysibm.obj sysedit.obj syszibm.obj
	tlink @gg
	del ctdl.map

ctdlz.exe ctdlz: $(PORT) sysdep2.obj sysdep1.obj sysarc.obj syszm.obj splitf.obj \
		sysdoor.obj syszen.obj syszibm.obj sysedit.obj
	tlink @zz

ctdl-ti.exe ctdl-ti: $(PORT) sysdep2.obj sysdep1.obj sysarc.obj splitf.obj \
		syszm.obj sysdoor.obj systi.obj sysedit.obj ti_delay.obj \
		ti_modem.obj ti_video.obj
	tlink @tilink
	del ctdl-ti.map

c86door.exe c86door: c86door.obj
        tlink @cdoor
        copy c86door.exe \c86\build
	del c86door.map

c86door.obj: c86door.h

ctdl.obj: $(HEADERS)

floors.obj: $(HEADERS)

events.obj: $(HEADERS)

rooma.obj: $(HEADERS)

roomb.obj: $(HEADERS)

offline.obj: $(HEADERS)

info.obj: $(HEADERS)

infodisp.obj: $(HEADERS)

misc.obj: $(HEADERS)

shared.obj: $(HEADERS)

mail.obj: $(HEADERS)

mailfwd.obj: $(HEADERS)

modem.obj: $(HEADERS)

log.obj: $(HEADERS)

msg.obj: $(HEADERS)

netcall.obj: $(HEADERS)

netcache.obj: $(HEADERS)

netrcv.obj: $(HEADERS)

sysdep2.obj: $(HEADERS)

splitf.obj: $(HEADERS)

sysdep1.obj: $(HEADERS)

sysarc.obj: $(HEADERS)

netmisc.obj: $(HEADERS)

netedit.obj: $(HEADERS)

netitl.obj: $(HEADERS)

ctdlansi.obj: $(HEADERS)

ansisys.obj: $(HEADERS)

citvid.obj: $(HEADERS) citvid.h

citvid2.obj: $(HEADERS) citvid2.h

virt.obj: $(HEADERS) ctdlvirt.h

virt2.obj: $(HEADERS) ctdlvirt.h

confg.exe confg: confg.obj confg2.obj syscfg.obj cn.lib info.obj mailfwd.obj \
		shared.obj virt.obj tools.lib dom-init.obj
	tlink @cc
	del confg.map

confg.obj: $(HEADERS)

confg2.obj: $(HEADERS)

syscfg.obj: $(HEADERS) c86door.h

cn.lib: arch.obj libnet.obj liblog.obj libcryp.obj libroom.obj\
 libtabl.obj libmsg.obj liblog2.obj ..\port\libtime.obj libbio.obj formhdr.obj
    del cn.lib
    tlib /c cn +arch +libnet +liblog +libcryp +libroom +libtabl +libmsg \
    +liblog2 +libtime +libbio +formhdr

tools.lib: tools.obj
	del tools.lib
	tlib /c tools +tools.obj

arch.obj: $(HEADERS)

citzen.obj: citzen.asm
   dir

libnet.obj: $(HEADERS)

libbio.obj: $(HEADERS)

liblog.obj: $(HEADERS)

liblog2.obj: $(HEADERS)

libcryp.obj: $(HEADERS)

libroom.obj: $(HEADERS)

libtabl.obj: $(HEADERS)

libmsg.obj: $(HEADERS)

calllog.obj: $(HEADERS)

hot_help.obj: $(HEADERS)

vortex.obj: $(HEADERS)

syszm.obj: $(HEADERS)

cc.obj: $(HEADERS)

sysdoor.obj: $(HEADERS) C86door.h

sysedit.obj: $(HEADERS)

sysibm.obj: $(HEADERS)

msgnfmt.obj: $(HEADERS)

route.obj: $(HEADERS)

compact.obj: $(HEADERS)

areas.obj: $(HEADERS)

syszibm.obj: $(HEADERS)

ti_modem.obj: ti_pro.h

ti_video.obj: ti_pro.h

ti_delay.obj:

systi.obj: $(HEADERS)

dom-core.obj domains.obj: $(HEADERS) 2ndfmt.h

bio.obj: $(HEADERS)

stroll.obj: $(HEADERS)

#
# It appears that due to the way DOS's command.com works, Borland's Makefile for
# this has to be written this way rather than the way you'd do it under a sane
# operating system.
#
bin: NORMAL c86door
	copy ctdl.exe \c86\build
	copy c86door.exe \c86\build
