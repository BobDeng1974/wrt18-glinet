include $(TOPDIR)/rules.mk

PKG_NAME:=uci2dat
PKG_VERSION:=2019-04-03
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

PKG_MAINTAINER:=wind <wind@wind164.org>

include $(INCLUDE_DIR)/package.mk
#include $(INCLUDE_DIR)/cmake.mk

define Package/uci2dat
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=MTK/WIFI_SHEEL
  DEPENDS:=+libc +libuci
  TITLE+= (mtk wifi shell tools)
endef

define Package/uqmi/description
  some shells for ctrl mtk driver
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/uci2dat/install
	$(CP) ./files/* $(1)/
	$(INSTALL_DIR) $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ap_client $(1)/usr/bin/
endef

$(eval $(call BuildPackage,uci2dat))
