PREFIX=/global/syscom/sc/nsg/opt/udiRoot
CONFIG_DIR=/global/syscom/sc/nsg/etc
CONFIG_FILE=$(CONFIG_DIR)/udiRoot.conf
INCLUDE_FILE=$(CONFIG_DIR)/udiRoot.include
CRAY_NETTYPE=gem
CRAY_NODETYPE=c
SYSTEM=grace
VERSION=0.5
WLM_INT=alps

CC      = /usr/bin/gcc
RM      = /bin/rm
CMP     = /usr/bin/cmp
INSTALL = /usr/bin/install
