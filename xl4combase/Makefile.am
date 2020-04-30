#
# excelfore-gptp - Implementation of gPTP(IEEE 802.1AS)
# Copyright (C) 2019 Excelfore Corporation (https://excelfore.com)
#
# This file is part of excelfore-gptp.
#
# excelfore-gptp is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# excelfore-gptp is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with excelfore-gptp.  If not, see
# <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
#
PACKAGENAME=xl4combase
VERSION=1.0.1
DESCRIPTION=Excelfore communication base library
lib_LTLIBRARIES = libx4combase.la
libx4combase_la_SOURCES = combase.h
libx4combase_la_LIBADD =
pkginclude_HEADERS = combase.h
pkgincludedir=$(includedir)/$(PACKAGENAME)
docdir=${datarootdir}/doc/$(PACKAGENAME)

ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -Wall -Wno-unused-result \
	-DXL4PKGVERSION=\"$(VERSION)-`cd $(srcdir);echo opensrc`\"

if BUILD_IN_STPL
AM_CFLAGS += -I$(srcdir)/..
AM_LDFLAGS = -L../xl4unibase/.libs
DOC_DIR = $(abs_srcdir)/../xl4unibase/doc
else
DOC_DIR = $(datadir)/xl4unibase
endif

if POSIX_PLATFORM
AM_CFLAGS += -DPOSIX_PLATFORM
endif

if LINUX_ETHTOOL
AM_CFLAGS += -DLINUX_ETHTOOL
endif

if NO_ETHERNET
AM_CFLAGS += -DCOMBASE_NO_ETH
else
libx4combase_la_SOURCES += cb_ethernet.h cb_ethernet.c
pkginclude_HEADERS += cb_ethernet.h
endif

if NO_INET
AM_CFLAGS += -DCOMBASE_NO_INET
else
libx4combase_la_SOURCES += cb_inet.h cb_inet.c
pkginclude_HEADERS += cb_inet.h
endif

if NO_THREAD
AM_CFLAGS += -DCOMBASE_NO_THREAD
else
libx4combase_la_SOURCES += cb_thread.h
pkginclude_HEADERS += cb_thread.h
libx4combase_la_LIBADD += -lpthread
endif

pkginclude_HEADERS += cb_ipcshmem.h cb_ipcsock.h
libx4combase_la_SOURCES += cb_ipcshmem.h cb_ipcsock.h
if NO_IPCSHMEM
libx4combase_la_SOURCES += cb_noipcshmem.c
else
libx4combase_la_SOURCES += cb_ipcshmem.c
libx4combase_la_LIBADD += -lrt
endif

if NO_IPCSOCK
AM_CFLAGS += -DCOMBASE_NO_IPCSOCK
else
libx4combase_la_SOURCES += cb_ipcsock.c
endif

if NO_EVENT
AM_CFLAGS += -DCOMBASE_NO_EVENT
else
libx4combase_la_SOURCES += cb_tmevent.h cb_tmevent.c
pkginclude_HEADERS += cb_tmevent.h
libx4combase_la_LIBADD += -lrt
endif

check_PROGRAMS = cb_ethernet_ovip_unittest cb_timer_unittest cb_thread_waitpoint_unittest \
cb_noipcshmem_unittest
AM_MAKEFLAGS = CMOCKA_TEST_ABORT='1'

cb_ethernet_ovip_unittest_SOURCES = cb_ethernet_ovip_unittest.c
cb_ethernet_ovip_unittest_LDADD = libx4combase.la
cb_ethernet_ovip_unittest_LDFLAGS = $(AM_LDFLAGS) -lx4unibase -lcmocka -lpthread

cb_timer_unittest_SOURCES = cb_timer_unittest.c
cb_timer_unittest_LDADD = libx4combase.la
cb_timer_unittest_LDFLAGS = $(AM_LDFLAGS) -lx4unibase -lcmocka -lpthread

cb_thread_waitpoint_unittest_SOURCES = cb_thread_waitpoint_unittest.c
cb_thread_waitpoint_unittest_LDADD = libx4combase.la
cb_thread_waitpoint_unittest_LDFLAGS = $(AM_LDFLAGS) -lx4unibase -lcmocka -lpthread

cb_noipcshmem_unittest_SOURCE = cb_noipcshmem_unittest.c
cb_noipcshmem_unittest_LDADD = libx4combase.la
cb_noipcshmem_unittest_LDFLAGS = $(AM_LDFLAGS) -lx4unibase -lcmocka -lpthread

TESTS = $(check_PROGRAMS)

DOC_HTML_INDEX=$(srcdir)/docoutput/html/index.html
DOC_LATEC_REFMAN=$(srcdir)/docoutput/latex/refman.pdf
TARGET_PDF=$(srcdir)/docoutput/combase_apis.pdf
TEX_TEMPLATE = xl4_header_template.tex
doc: $(TARGET_PDF)
$(srcdir)/xl4_header.tex:
	@tddir=$(DOC_DIR); \
	if [ ! -f $${tddir}/$(TEX_TEMPLATE) ]; then \
		tddir=$${tddir/\/usr/\/usr\/local}; \
	fi; \
	if [ ! -f $${tddir}/$(TEX_TEMPLATE) ]; then \
		echo "$${tddir}/$(TEX_TEMPLATE) doesn't exist"; \
		exit 1;\
	fi; \
	cp $${tddir}/$(TEX_TEMPLATE) $@; \
	sed -i "s:EXCELFORE_LOGO_PNG:$${tddir}/excelfore_logo.png:" $@; \
	sed -i "s:EXCELFORE_LOGO_L_PNG:$${tddir}/excelfore_logo_L.png:" $@
	DOCDATE=`cd $(srcdir); git show -s --format=%ci HEAD | sed -r "s/(\S*).*/\1/"`; \
	sed -i "s/XL4-DOCUMENT-DATE/$${DOCDATE}/" $@
	sed -i "s/XL4-DOCUMENT-NAME/$(PACKAGENAME)/" $@
	sed -i "s/XL4-DOCUMENT-VERSION/$(VERSION)/" $@

$(TARGET_PDF): $(DOC_LATEC_REFMAN)
	$(INSTALL_DATA) $^ $@

$(DOC_LATEC_REFMAN): $(srcdir)/xl4_header.tex $(DOC_HTML_INDEX)
	make -C $(srcdir)/docoutput/latex

docoutput/html $(DOC_HTML_INDEX): $(pkginclude_HEADERS) README.md
	cd $(srcdir); COMBASE_VERSION=$(VERSION)-`date +%Y%m%d` doxygen combase_doxygen.conf

install-html-local: docoutput/html $(DOC_HTML_INDEX)
	install -d $(DESTDIR)$(docdir)
	cp -r $< $(DESTDIR)$(docdir)/
uninstall-html:
	rm -rf $(DESTDIR)$(docdir)/html
	if [ -d $(DESTDIR)$(docdir) -a -z "`ls -A $(DESTDIR)$(docdir)`" ]; then \
		rmdir $(DESTDIR)$(docdir); \
	fi

install-pdf-local: $(TARGET_PDF)
	install -d $(DESTDIR)$(docdir)
	$(INSTALL_DATA) $^ $(DESTDIR)$(docdir)/
uninstall-pdf:
	rm -rf $(DESTDIR)$(docdir)/*.pdf
	if [ -d $(DESTDIR)$(docdir) -a -z "`ls -A $(DESTDIR)$(docdir)`" ]; then \
		rmdir $(DESTDIR)$(docdir); \
	fi

deb:
	@if [ ! -e $(srcdir)/debian.make ]; then \
		if [ -e $(datadir)/xl4unibase/debian.make ]; then \
			ln -s $(datadir)/xl4unibase/debian.make $(srcdir)/debian.make; \
		elif [ -e /usr/share/xl4unibase/debian.make ]; then \
			ln -s /usr/share/xl4unibase/debian.make $(srcdir)/debian.make; \
		else \
			echo "can't find debian.make"; \
			exit 1; \
		fi; \
	fi
	@cd $(srcdir); make -f debian.make

TOOLPROGRAMS=vport_pcap_conv.py
install-tools:
	for f in $(TOOLPROGRAMS); do install -m 755 $(srcdir)/tools/$${f} $(bindir); done
uninstall-tools:
	for f in $(TOOLPROGRAMS); do rm -f $(bindir)/$${f}; done

python_PYTHON = tools/gen_c_debug_strings.py \
		tools/__init__.py
pythondir=$(prefix)/lib/python2.7/dist-packages/$(PACKAGENAME)
py_compile = -c :
