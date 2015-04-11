PREFIX=/opt/nersc/udiRoot/tag
DESTDIR=$(PREFIX)
CONFIG_DIR=$(PREFIX)/etc
CONFIG_FILE=$(CONFIG_DIR)/udiRoot.conf
INCLUDE_FILE=$(CONFIG_DIR)/udiRoot.include
CRAY_NETTYPE=gem
CRAY_NODETYPE=c
SYSTEM=grace
VERSION=0.5
WLM_INT=torque
KEYLEN=512
CRAY_SUPPORT=1

CC      = /usr/bin/gcc
RM      = /bin/rm
CMP     = /usr/bin/cmp
INSTALL = /usr/bin/install
