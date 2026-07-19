################################################################################
#
# testapp
#
################################################################################

TESTAPP_SITE = $(TOPDIR)/package/testapp/src
TESTAPP_SITE_METHOD = local
TESTAPP_LICENSE = MIT
TESTAPP_LICENSE_FILES = LICENSE

define TESTAPP_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -o $(@D)/testapp $(@D)/main.c
endef

define TESTAPP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/testapp $(TARGET_DIR)/usr/bin/testapp
endef

$(eval $(generic-package))
