# This is a general makefile to create simple deb packages for automake projects
# The project must be source controlled by git
# Makefile.am must have 3 lines to define the next 3 variables:
#   PACKAGENAME, VERSION, DESCRIPTION
#
# to create the deb packages, simply put this file into the source directory,
# the same directory as where Makefile.am resides, and run
# make -f debian.make
#
# Note: dependencies are not defined. Add in 'debian/control' if needed
#
PACKAGENAME=$(shell sed -n '/AC_INIT/p' configure.ac | awk -F"[][]" '{print $$2}')
VERSION=$(shell sed -n '/AC_INIT/p' configure.ac | awk -F"[][]" '{print $$4}')
DESCRIPTION=$(shell sed -n "s/^.*PACKAGE_DESC,//p" configure.ac | sed 's/.$$//')
USERNAME=$(shell git config user.name)
USEREMAIL=$(shell git config user.email)
deb:
	@rm -Rf debbuild
	@mkdir debbuild
	@for f in `git ls-files`; do cp --parents $$f debbuild; done
	@if [ ! -e debbuild/debian.make ]; then ln -s ../debian.make debbuild/debian.make; fi
	@make -C debbuild -f debian.make debp

debp:
	@mkdir -p debian
	@for f in "changelog" "rules" "control" "compat"; do rm -f debian/$$f; done
	@make -f debian.make debian/changelog
	@make -f debian.make debian/rules
	@make -f debian.make debian/control
	@make -f debian.make debian/compat
	@make -f debian.make debian/shlibs.local
	@./autogen.sh
	@debuild -us -uc

debian/changelog:
	@echo "$(PACKAGENAME) ($(VERSION)) UNRELEASED; urgency=medium" > $@
	@echo "" >> $@
	@echo "  * package release." >> $@
	@echo "" >> $@
	@echo " -- $(USERNAME) <$(USEREMAIL)>  `date -R`" >> $@

debian/rules:
	@echo "#!/usr/bin/make -f" > $@
	@echo "%:" >> $@
	@echo "	dh \$$@ --with autoreconf" >> $@
	@echo "override_dh_auto_install:" >> $@
	@echo "	make DESTDIR=\$$(CURDIR)/debian/$(PACKAGENAME) install-exec" >> $@
	@echo "	make DESTDIR=\$$(CURDIR)/debian/$(PACKAGENAME)-dev install-data" >> $@
	@echo "	if [ \"\$$(DEB_BUILD_GNU_TYPE)\" =  \"\$$(DEB_HOST_GNU_TYPE)\" ]; then \\" >> $@
	@echo "		make DESTDIR=\$$(CURDIR)/debian/$(PACKAGENAME)-doc install-html;\\" >> $@
	@echo "		make DESTDIR=\$$(CURDIR)/debian/$(PACKAGENAME)-doc install-pdf;\\" >> $@
	@echo "	fi" >> $@

debian/control:
	@echo "Source: $(PACKAGENAME)" > $@
	@echo "Maintainer: $(USERNAME) <$(USEREMAIL)>" >> $@
	@echo "Standards-Version: $(VERSION)" >> $@
	@echo "" >> $@
	@echo "Package: $(PACKAGENAME)" >> $@
	@echo "Architecture: any" >> $@
	@echo "Description: $(DESCRIPTION)" >> $@
	@echo "" >> $@
	@echo "Package: $(PACKAGENAME)-dev" >> $@
	@echo "Architecture: any" >> $@
	@echo "Description: $(DESCRIPTION), development files" >> $@
	@echo "" >> $@
	@echo "Package: $(PACKAGENAME)-doc" >> $@
	@echo "Architecture: any" >> $@
	@echo "Description: $(DESCRIPTION), documents" >> $@

debian/compat:
	@echo "9" > $@

debian/shlibs.local:
	@echo "libx4unibase 0 libx4unibase" > $@
	@echo "libx4combase 0 libx4combase" >> $@
