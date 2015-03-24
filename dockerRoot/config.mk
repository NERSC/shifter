PREFIX=/global/syscom/sc/nsg/opt/dockerRoot
CONFIG_DIR=/global/syscom/sc/nsg/etc
CONFIG_FILE=$(CONFIG_DIR)/dockerRoot.conf
INCLUDE_FILE=$(CONFIG_DIR)/dockerRoot.include
CRAY_NETTYPE=gem
CRAY_NODETYPE=c
SYSTEM=grace
WLM_INT=alps

CC      = /usr/bin/gcc
RM      = /bin/rm
CMP     = /usr/bin/cmp
INSTALL = /usr/bin/install
