# Makefile for ddrescue - A data recovery tool
# Copyright (C) 2004 Antonio Diaz Diaz.

prefix = /usr/local
datadir = $(prefix)/share
infodir = $(datadir)/info
mandir = $(datadir)/man
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin

DISTNAME = ddrescue-0.7

CXX = g++
INSTALL = install
INSTALL_PROGRAM = $(INSTALL) -p -m 755
INSTALL_DATA = $(INSTALL) -p -m 644
SHELL = /bin/sh
CPPFLAGS =
CXXFLAGS = -Wall -W -O2
LDFLAGS =

objs = ddrescue.o

.PHONY : all install install-info install-man install-strip \
         uninstall uninstall-info uninstall-man \
         dist clean distclean

all : ddrescue

ddrescue : $(objs)
	$(CXX) $(LDFLAGS) $(CXXFLAGS) -o ddrescue $(objs)

doc : ddrescue
	help2man -o ddrescue.1 --no-info ./ddrescue

%.o : %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(objs) : Makefile


install : all install-man
	if test ! -d $(DESTDIR)$(bindir) ; then $(INSTALL) -d $(DESTDIR)$(bindir) ; fi
	$(INSTALL_PROGRAM) ./ddrescue $(DESTDIR)$(bindir)/ddrescue

install-info :
	if test ! -d $(DESTDIR)$(infodir) ; then $(INSTALL) -d $(DESTDIR)$(infodir) ; fi
	$(INSTALL_DATA) ./ddrescue.info $(DESTDIR)$(infodir)/ddrescue.info
	install-info $(DESTDIR)$(infodir)/ddrescue.info $(DESTDIR)$(infodir)/dir

install-man :
	if test ! -d $(DESTDIR)$(mandir)/man1 ; then $(INSTALL) -d $(DESTDIR)$(mandir)/man1 ; fi
	$(INSTALL_DATA) ./ddrescue.1 $(DESTDIR)$(mandir)/man1/ddrescue.1

install-strip : all
	$(MAKE) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' install

uninstall : uninstall-man
	-rm -f $(DESTDIR)$(bindir)/ddrescue

uninstall-info :
	install-info --remove $(DESTDIR)$(infodir)/ddrescue.info $(DESTDIR)$(infodir)/dir
	-rm -f $(DESTDIR)$(infodir)/ddrescue.info

uninstall-man :
	-rm -f $(DESTDIR)$(mandir)/man1/ddrescue.1

dist :
	ln -sf . $(DISTNAME)
	tar -cvf $(DISTNAME).tar \
	  $(DISTNAME)/COPYING \
	  $(DISTNAME)/ChangeLog \
	  $(DISTNAME)/INSTALL \
	  $(DISTNAME)/Makefile \
	  $(DISTNAME)/README \
	  $(DISTNAME)/ddrescue.1 \
	  $(DISTNAME)/ddrescue.cc
	rm -f $(DISTNAME)
	bzip2 -v $(DISTNAME).tar

clean :
	-rm -f ddrescue $(objs)

distclean : clean
	-rm -f *.tar *.bz2
