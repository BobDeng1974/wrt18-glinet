###############################################
# OpenWrt Makefile for xmipcnet program
#
# Most of the variables used here are defined in
# the include directives below. We just need to
# specify a basic description of the package,
# where to build our program, where to find
# the source files, and where to install the
# compiled program on the router.
#
# Be very careful of spacing in this file.
# Indents should be tabs, not spaces, and
# there should be no trailing whitespace in
# lines that are not commented.
#
##############################################
include $(TOPDIR)/rules.mk
# Name and release number of this package
PKG_NAME:=xmipcnet
PKG_RELEASE:=1

# CMAKE_INSTALL:=1
# This specifies the directory where we're going to build the program. 
# The root build directory, $(BUILD_DIR), is by default the build_mipsel
# directory in your OpenWrt SDK directory
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
# include $(INCLUDE_DIR)/cmake.mk

# Specify package information for this program.
# The variables defined here should be self explanatory.
# If you are running Kamikaze, delete the DESCRIPTION
# variable below and uncomment the Kamikaze define
# directive for the description below

define Package/xmipcnet/Default
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=XM IPC NetSDK client
  TITLE:=xmipcnet - read XM-ipc stream
endef

define Package/xmipcnet
	$(call Package/xmipcnet/Default)
	DEPENDS:=+libpthread +librt +libstdcpp
	TITLE+= (XM IPC netsdk client)
endef

# Uncomment portion below for Kamikaze and delete DESCRIPTION variable above
define Package/xmipcnet/description
       XM IPC netsdk client, read ipc stream and forward to cloud
endef

# Specify what needs to be done to prepare for building the package.
# In our case, we need to copy the source files to the build directory.
# This is NOT the default.  The default uses the PKG_SOURCE_URL and the
# PKG_SOURCE which is not defined here to download the source from the web.
# In order to just build a simple program that we have just written, it is
# much easier to do it this way.
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/xmipcnet/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/xmipcnet $(1)/bin/
	
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) ./files/ipc_renew.sh $(1)/sbin/

	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libxmnetsdk.so $(1)/usr/lib
	
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/ipc_cfg.conf $(1)/etc/config/ipc_cfg
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/ipc_renew.init $(1)/etc/init.d/ipc
endef

# This line executes the necessary commands to compile our program.
# The above define directives specify all the information needed, but this
# line calls BuildPackage which in turn actually uses this information to
# build a package.
$(eval $(call BuildPackage,xmipcnet))
