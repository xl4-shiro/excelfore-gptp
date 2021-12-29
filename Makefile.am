#
# Excelfore gptp - Implementation of gPTP(IEEE 802.1AS)
# Copyright (C) 2019 Excelfore Corporation (https://excelfore.com)
#
# This file is part of Excelfore-gptp.
#
# Excelfore-gptp is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# Excelfore-gptp is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Excelfore-gptp.  If not, see
# <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
#
PACKAGENAME=xl4gptp
VERSION=1.2.0
DESCRIPTION=Excelfore gptp daemon and libary

ACLOCAL_AMFLAGS = -I m4
AM_LDFLAGS =
AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter -include gptp_private.h \
	-DXL4PKGVERSION=\"$(VERSION)-`cd $(srcdir);echo opensrc`\"

if BUILD_IN_STPL
AM_CFLAGS += -I$(srcdir)/..
AM_LDFLAGS += -L../xl4unibase/.libs -L../xl4combase/.libs
UB_GENCONF = $(srcdir)/../xl4unibase/ub_genconf.py
DOC_DIR = $(abs_srcdir)/../xl4unibase/doc
else # BUILD_IN_STPL
UB_GENCONF = $(STAGING_DIR_HOST)$(datadir)/xl4unibase/ub_genconf.py
DOC_DIR = $(STAGING_DIR_HOST)$(datadir)/xl4unibase
endif # BUILD_IN_STPL

pkgincludedir=$(includedir)/xl4tsn/gptp2
docdir=${datarootdir}/doc/xl4tsn/gptp2

AM_CFLAGS += -D_GNU_SOURCE

lib_LTLIBRARIES = libx4gptp2.la
GPTP2_SOURCES = ll_gptpsupport.h \
	gptpbasetypes.h mdeth.c mdeth.h mind.c mind.h gptpcommon.c gptpcommon.h \
	gptpclock.c gptpclock.h gptpnet.h gptpman.c gptpman.h \
	md_pdelay_req_sm.c md_pdelay_req_sm.h md_pdelay_resp_sm.c md_pdelay_resp_sm.h \
	md_sync_receive_sm.c md_sync_receive_sm.h \
	md_sync_send_sm.c md_sync_send_sm.h \
	port_sync_sync_receive_sm.c port_sync_sync_receive_sm.h \
	site_sync_sync_sm.c site_sync_sync_sm.h \
	port_sync_sync_send_sm.c port_sync_sync_send_sm.h \
	announce_interval_setting_sm.c announce_interval_setting_sm.h \
	port_announce_receive_sm.c port_announce_receive_sm.h \
	port_announce_information_sm.c port_announce_information_sm.h \
	port_announce_information_ext_sm.c port_announce_information_ext_sm.h \
	port_state_selection_sm.c port_state_selection_sm.h \
	port_state_setting_ext_sm.c port_state_setting_ext_sm.h \
	port_announce_transmit_sm.c port_announce_transmit_sm.h \
	clock_master_sync_send_sm.c clock_master_sync_send_sm.h \
	clock_slave_sync_sm.c clock_slave_sync_sm.h \
	clock_master_sync_receive_sm.c clock_master_sync_receive_sm.h \
	clock_master_sync_offset_sm.c clock_master_sync_offset_sm.h \
	gptp_capable_transmit_sm.c gptp_capable_transmit_sm.h \
	gptp_capable_receive_sm.c gptp_capable_receive_sm.h \
	sync_interval_setting_sm.c sync_interval_setting_sm.h \
	link_delay_interval_setting_sm.c link_delay_interval_setting_sm.h \
	one_step_tx_oper_setting_sm.c one_step_tx_oper_setting_sm.h \
	md_announce_send_sm.c md_announce_send_sm.h \
	md_announce_receive_sm.c md_announce_receive_sm.h \
	md_signaling_send_sm.c md_signaling_send_sm.h \
	md_signaling_receive_sm.c md_signaling_receive_sm.h \
	gm_stable_sm.c gm_stable_sm.h \
	gptpipc.h gptp_config.c gptp_config.h gptp2_debug_defs.c \
	gptpconf_values_test.c \
	md_abnormal_hooks.c md_abnormal_hooks.h

bin_PROGRAMS = gptp2d set_ptpclock gptpipcmon
check_PROGRAMS = gptpcommon_unittest
TESTS = gptpcommon_unittest
GPTP2_LDADD = -lx4unibase -lx4combase

dist_bin_SCRIPTS = gptpipc_extscript
GPTP2_SOURCES += posix/ix_ll_gptpsupport.c

AM_CFLAGS += -D_GNU_SOURCE

if EKF_ARMADA
  GPTP2_SOURCES += armada/ekf/ix_armada_gptpnet.c armada/ekf/ix_armada_gptpclock.c
  AM_CFLAGS += -DEKFARMADA_PLATFORM
if BUILD_IN_STPL
  AM_LDFLAGS += -L../xl4pfutils/.libs
endif # end of EKF_ARMADA && BUILD_IN_STPL
  GPTP2_LDADD += -lpfutils -lsmi -ldevmem
else # default platform
  AM_CFLAGS += -DPTP_VIRTUAL_CLOCK_SUPPORT
  GPTP2_SOURCES += posix/ix_gptpnet.c posix/ix_gptpclock.c posix/ix_ptpdevclock.c \
      gptpclock_virtual.c gptpclock_virtual.h

  check_PROGRAMS += freqadj_unittest ix_gptpclock_unittest ix_gptpnet_unittest \
      gptpmasterclock_response md_abnormal_hooks_unittest
  TESTS += freqadj_unittest ix_gptpclock_unittest md_abnormal_hooks_unittest gptp2_test_run.sh

  ix_gptpnet_unittest_SOURCES = posix/ix_gptpnet_unittest.c $(GPTP2_SOURCES)
  ix_gptpnet_unittest_CFLAGS = $(AM_CFLAGS)
  ix_gptpnet_unittest_LDADD = -lpthread $(GPTP2_LDADD)

  ix_gptpclock_unittest_SOURCES = posix/ix_gptpclock_unittest.c  $(GPTP2_SOURCES)
  ix_gptpclock_unittest_CFLAGS = $(AM_CFLAGS)
  ix_gptpclock_unittest_LDADD = -lpthread $(GPTP2_LDADD) -lcmocka

  freqadj_unittest_SOURCES = freqadj_unittest.c gptp_config.c gptpclock.c \
	posix/ix_gptpclock.c posix/ix_ptpdevclock.c gptpclock_virtual.c
  freqadj_unittest_CFLAGS = $(AM_CFLAGS)
  freqadj_unittest_LDADD = -lpthread $(GPTP2_LDADD) -lcmocka

  gptpmasterclock_response_SOURCES = gptpmasterclock_response.c
  gptpmasterclock_response_CFLAGS = $(AM_CFLAGS)
  gptpmasterclock_response_LDADD =  libx4gptp2.la

  md_abnormal_hooks_unittest_SOURCES =  md_abnormal_hooks_unittest.c $(GPTP2_SOURCES)
  md_abnormal_hooks_unittest_CFLAGS = $(AM_CFLAGS)
  md_abnormal_hooks_unittest_LDADD = -lpthread $(GPTP2_LDADD) -lcmocka

  bin_PROGRAMS += gptpclock_monitor
  gptpclock_monitor_SOURCES = gptpclock_monitor.c
  gptpclock_monitor_CFLAGS = $(AM_CFLAGS)
  gptpclock_monitor_LDADD =  libx4gptp2.la $(GPTP2_LDADD)

  libx4gptp2_la_SOURCES = gptpmasterclock.c gptpmasterclock.h \
	gptpipc.c gptpipc.h posix/ix_ptpdevclock.c
  libx4gptp2_la_LIBADD = $(GPTP2_LDADD)

if HAVE_SYSIO
  bin_PROGRAMS += gptp_sync_pout
  gptp_sync_pout_SOURCES = gptp_sync_pout.c
  gptp_sync_pout_CFLAGS = $(AM_CFLAGS)
  gptp_sync_pout_LDADD =  libx4gptp2.la $(GPTP2_LDADD)
endif

endif # end of default platform

gptp2d_SOURCES = gptp2d.c $(GPTP2_SOURCES)
gptp2d_CFLAGS = $(AM_CFLAGS)
gptp2d_LDADD = -lm -lpthread $(GPTP2_LDADD)

gptpcommon_unittest_SOURCES = gptpcommon_unittest.c $(GPTP2_SOURCES)
gptpcommon_unittest_CFLAGS = $(AM_CFLAGS)
gptpcommon_unittest_LDADD = -lpthread $(GPTP2_LDADD) -lcmocka

gptpipcmon_SOURCES = gptpipc.c gptpipcmon.c
gptpipcmon_CFLAGS = $(AM_CFLAGS)
gptpipcmon_LDADD = -lpthread $(GPTP2_LDADD)

set_ptpclock_SOURCES = set_ptpclock.c
set_ptpclock_CFLAGS = $(AM_CFLAGS)

# public header for the library
pkginclude_HEADERS = gptpbasetypes.h gptpmasterclock.h gptpipc.h

GENERATED_FILES = gptp_config.c gptp_config.h gptpconf_values_test.c \
	gptp2_debug_defs.c gptp2.conf

BUILT_SOURCES = $(GENERATED_FILES)

gptp2d.c: gptp_config.c gptp_config.h gptpconf_values_test.c
gptp_config.c gptp_config.h gptpconf_values_test.c: gptp_defaults.h
	@cmd=$(UB_GENCONF); \
	if [ ! -x $${cmd} ]; then \
		cmd=$${cmd/\/usr\/local/\/usr}; \
		if [ "$${cmd}" = "$(UB_GENCONF)" ]; then \
			cmd=$${cmd/\/usr/\/usr\/local}; \
		fi; \
	fi; \
	$${cmd} -i $(srcdir)/gptp_defaults.h -c $(srcdir)/gptp_config.c -d \
	$(srcdir)/gptp_config.h -v $(srcdir)/gptpconf_values_test.c -e gptpbasetypes.h -p gptp

$(srcdir)/gptp2_debug_defs.c: gptp2_gen_debug.py mdeth.h gptpcommon.h
	cd $(srcdir); ./gptp2_gen_debug.py > gptp2_debug_defs.c

$(srcdir)/gptp2.conf: gptp_defaults.h
	sed -n '/\/\*/,$$p' $(srcdir)/gptp_defaults.h | sed '/#endif/d' | sed 's/#define DEFAULT_/CONF_/;s/^/#/;s/\/\*/  /;s/\*\//  /;s/\/\//#/' | sed 's/##/# /' > $@
sysconf_DATA = gptp2.conf

deb:
	@cd $(srcdir); make DEB_BUILD_OPTIONS=nocheck -f debian.make

clean-local:
	cd $(srcdir); rm -f $(GENERATED_FILES)

DOC_HTML_INDEX=$(srcdir)/docoutput/html/index.html
DOC_LATEC_REFMAN=$(srcdir)/docoutput/latex/refman.pdf
TARGET_PDF=$(srcdir)/docoutput/$(PACKAGENAME)_apis.pdf
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

$(srcdir)/docoutput/html $(DOC_HTML_INDEX): $(pkginclude_HEADERS) README.md xl4gptp_doxygen.conf
	cd $(srcdir); PROJECT_NAME=$(PACKAGENAME) PROJECT_VERSION=$(VERSION)-`date +%Y%m%d` \
	DOXYGEN_INPUT="$(pkginclude_HEADERS) README.md" doxygen $(PACKAGENAME)_doxygen.conf

install-html-local: $(srcdir)/docoutput/html $(DOC_HTML_INDEX)
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



CPPCHECK_TST_TARGET = $(gptp2d_SOURCES)
CPPCHECK_TST_TARGET += $(gptp_sync_pout_SOURCES) $(gptpclock_monitor_SOURCES)
CPPCHECK_TST_TARGET += $(gptpclock_monitor_SOURCES)
CPPCHECK_TST_TARGET += $(libx4gptp2_la_SOURCES)
CPPCHECK_TST_TARGET += $(freqadj_unittest_SOURCES)
-include $(srcdir)/make_cppcheck.mk
