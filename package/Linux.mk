#
# Linux-specific packaging script
#

ifndef OPENPILOT_IS_COOL
    $(error Top level Makefile must be used to build this target)
endif

DEB_DIST             := unstable
DEB_VER              := $(subst RELEASE-,,$(PACKAGE_LBL))
DEB_RELEASE          := 1
DEB_ARCH             := $(shell dpkg --print-architecture)
DEB_NAME             := openpilot
DEB_NAME_VER         := $(DEB_NAME)-$(DEB_VER)
DEB_NAME_VER_REL     := $(DEB_NAME)_$(DEB_VER)-$(DEB_RELEASE)
DEB_PACKAGE_NAME     := $(DEB_NAME_VER_REL)_$(DEB_ARCH)
DEB_ORIG_SRC_NAME    := $(DEB_NAME)_$(DEB_VER).orig.tar.gz
DEB_DIR              := package/linux/debian

SED_DATE_STRG         = $(shell date -R)
SED_SCRIPT            = s/<VERSION>/$(DEB_VER)-$(DEB_RELEASE)/;s/<DATE>/$(SED_DATE_STRG)/;s/<DIST>/$(DEB_DIST)/


.PHONY: package
package: debian
	@$(ECHO) "Building Linux package, please wait..."
	# Override clean and build because OP has already performed them.
	$(V1) printf "override_dh_auto_clean:\noverride_dh_auto_build:\n\t#\n" >> debian/rules
	$(V1) dpkg-buildpackage -b -us -uc
	$(V1) mv $(ROOT_DIR)/../$(DEB_PACKAGE_NAME).deb $(PACKAGE_DIR)
	$(V1) mv $(ROOT_DIR)/../$(DEB_PACKAGE_NAME).changes $(PACKAGE_DIR)
	$(V1) rm -r debian

debian: $(DEB_DIR)
	$(V1) rm -rf debian
	$(V1) cp -rL $(DEB_DIR) debian
	$(V1) sed -i -e "$(SED_SCRIPT)" debian/changelog

.PHONY: package_src
package_src:  $(PACKAGE_DIR)/$(DEB_ORIG_SRC_NAME) $(PACKAGE_DIR)/$(DEB_NAME_VER)
	$(V1) cd $(PACKAGE_DIR)/$(DEB_NAME_VER) && dpkg-buildpackage -S -us -uc

$(PACKAGE_DIR)/$(DEB_ORIG_SRC_NAME): $(DIST_NAME).gz | $(PACKAGE_DIR)
	$(V1) cp $(DIST_NAME).gz $(PACKAGE_DIR)/$(DEB_ORIG_SRC_NAME)

$(PACKAGE_DIR)/$(DEB_NAME_VER): $(PACKAGE_DIR)/$(DEB_ORIG_SRC_NAME) debian | $(PACKAGE_DIR)
	$(V1) cd $(PACKAGE_DIR) && tar -xf $(DEB_ORIG_SRC_NAME)
	$(V1) mv debian $(PACKAGE_DIR)/OpenPilot
	$(V1) cd $(PACKAGE_DIR) && rm -rf $(DEB_NAME_VER) && mv OpenPilot $(DEB_NAME_VER)

##############################
#
# Install OpenPilot
#
##############################
prefix  := /usr/local
bindir  := $(prefix)/bin
libdir  := $(prefix)/lib
datadir := $(prefix)/share

INSTALL = cp -a --no-preserve=ownership
LN = ln
LN_S = ln -s

.PHONY: install
install:
	@$(ECHO) " INSTALLING GCS TO $(DESTDIR)/)"
	$(V1) $(MKDIR) -p $(DESTDIR)$(bindir)
	$(V1) $(MKDIR) -p $(DESTDIR)$(libdir)
	$(V1) $(MKDIR) -p $(DESTDIR)$(datadir)
	$(V1) $(MKDIR) -p $(DESTDIR)$(datadir)/applications
	$(V1) $(MKDIR) -p $(DESTDIR)$(datadir)/pixmaps
	$(V1) $(MKDIR) -p $(DESTDIR)$(udevdir)
	$(V1) $(INSTALL) $(BUILD_DIR)/openpilotgcs_$(GCS_BUILD_CONF)/bin/openpilotgcs $(DESTDIR)$(bindir)
	$(V1) $(INSTALL) $(BUILD_DIR)/openpilotgcs_$(GCS_BUILD_CONF)/bin/udp_test $(DESTDIR)$(bindir)
	$(V1) $(INSTALL) $(BUILD_DIR)/openpilotgcs_$(GCS_BUILD_CONF)/lib/openpilotgcs $(DESTDIR)$(libdir)
	$(V1) $(INSTALL) $(BUILD_DIR)/openpilotgcs_$(GCS_BUILD_CONF)/share/openpilotgcs $(DESTDIR)$(datadir)
	$(V1) $(INSTALL) $(ROOT_DIR)/package/linux/openpilot.desktop $(DESTDIR)$(datadir)/applications
	$(V1) $(INSTALL) $(ROOT_DIR)/package/linux/openpilot.png $(DESTDIR)$(datadir)/pixmaps
	$(V1) rm $(DESTDIR)/$(datadir)/openpilotgcs/translations/Makefile


