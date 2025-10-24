info_TEXINFOS = parted.texi
MAKEINFO = makeinfo --no-split

man_MANS      =	parted.8 partprobe.8

EXTRA_DIST    =	FAT		\
		FAQ		\
		USER.jp		\
		API		\
		fdl.texi	\
		$(man_MANS)	\
		pt_BR-parted.8	\
		FAQ-pt_BR	\
		parted-pt_BR.texi

install-data-local:
	$(INSTALL) -D $(srcdir)/pt_BR-parted.8 $(DESTDIR)$(mandir)/pt_BR/man8/parted.8

