################################################################################
#
# testapp
#
################################################################################

TESTAPP_SITE = $(TOPDIR)/package/testapp/src
TESTAPP_SITE_METHOD = local
TESTAPP_LICENSE = MIT
TESTAPP_LICENSE_FILES = LICENSE
TESTAPP_DEPENDENCIES = libdrm

define TESTAPP_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) \
		$(shell PKG_CONFIG_SYSROOT_DIR="$(STAGING_DIR)" $(PKG_CONFIG_HOST_BINARY) --cflags libdrm) \
		-o $(@D)/testapp $(@D)/main.c \
		$(shell PKG_CONFIG_SYSROOT_DIR="$(STAGING_DIR)" $(PKG_CONFIG_HOST_BINARY) --libs libdrm) \
		$(TARGET_LDFLAGS)
endef

define TESTAPP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/testapp $(TARGET_DIR)/usr/bin/testapp
endef

$(eval $(generic-package))
