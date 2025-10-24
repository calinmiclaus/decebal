SUBDIRS = m4  po libparted parted partprobe include doc debug

EXTRA_DIST =	config.rpath	\
		ABOUT-NLS	\
		AUTHORS		\
		BUGS		\
		COPYING		\
		THANKS		\
		TODO		\
	    	parted.spec.in	\
		parted.m4

aclocaldir=$(datadir)/aclocal
aclocal_DATA=parted.m4

dist-hook: parted.spec
	cp parted.spec $(distdir)


ACLOCAL_AMFLAGS = -I m4
