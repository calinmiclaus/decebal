# This file is part of GNU Parted
# Copyright (C) 1999, 2000, 2001 Free Software Foundation, Inc.
#
# This file may be modified and/or distributed without restriction.

SUBDIRS       = fs_amiga		\
		fs_ext2			\
		fs_fat			\
		fs_hfs			\
		fs_jfs			\
		fs_linux_swap		\
		fs_ntfs			\
		fs_reiserfs		\
		fs_ufs			\
		fs_xfs

LIBS = @INTLLIBS@ @LIBS@

partedincludedir      =	-I$(top_srcdir)/include
lib_LTLIBRARIES	      =	libparted.la
libparted_la_LDFLAGS  = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
 			-release $(LT_RELEASE)

libparted_la_SOURCES  = blkpg.h			\
			dvh.h			\
			crc32.c			\
			constraint.c		\
			debug.c			\
			device.c		\
			exception.c		\
			filesys.c		\
			geom.c			\
			libparted.c		\
			natmath.c		\
			timer.c			\
			unit.c			\
			disk.c			\
			disk_bsd.c		\
			disk_dos.c		\
			disk_gpt.c		\
			disk_loop.c		\
			disk_mac.c		\
			disk_dvh.c		\
			disk_pc98.c		\
			disk_sun.c		\
			disk_amiga.c		\
			@OS@.c

EXTRA_libparted_la_SOURCES    = linux.c		\
				gnu.c

libparted_la_LIBADD   = @UUID_LIBS@			\
			@OS_LIBS@			\
			@REISER_LIBS@			\
			fs_amiga/libamigafs.la		\
			fs_ext2/libext2.la		\
			fs_fat/libfat.la		\
			fs_hfs/libhfs.la		\
			fs_jfs/libjfs.la		\
			fs_linux_swap/liblinuxswap.la	\
			fs_ntfs/libntfs.la		\
			fs_reiserfs/libreiserfs.la	\
			fs_ufs/libufs.la		\
			fs_xfs/libxfs.la

EXTRA_DIST	      = mbr.s

INCLUDES = $(partedincludedir) @INTLINCS@

