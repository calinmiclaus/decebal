sbin_PROGRAMS = parted

partedincludedir      = -I$(top_srcdir)/include

parted_SOURCES = command.c	\
		 command.h	\
		 parted.c	\
		 strlist.c	\
		 strlist.h	\
		 ui.c		\
		 ui.h

parted_LDADD = @PARTED_START_STATIC@ @INTLLIBS@ @LIBS@ $(top_builddir)/libparted/libparted.la \
	       @PARTED_LIBS@ @PARTED_END_STATIC@

parted_LDFLAGS = @PARTEDLDFLAGS@

INCLUDES	= $(partedincludedir) @INTLINCS@

