#!/bin/bash
#
# Startup script for mock system
#

#
# Check for config
#
if [ ! -e /config ] ; then
  echo "Missing config directory."
  echo "Use the volume mount to pass in configuration data."
  exit 1
fi

# Copy shifter config
cp /config/udiRoot.conf /etc/shifter/udiRoot.conf

# Replace system name if SYSTEM variable is defined
#
if [ ! -z "$SYSTEM" ] ; then
  sed -i "s/systema/$SYSTEM/" /etc/shifter/udiRoot.conf
fi

# Add the key, entrypoint and munge key
if [ -e /config/ssh.pub ] ; then
  chmod 700 /root/.ssh
  chown -R root /root/.ssh 
  cp /config/ssh.pub /root/.ssh/authorized_keys
fi

# Copy and fix up munge key
#
if [ -e /config/munge.key ] ; then
  cp /config/munge.key /etc/munge/munge.key
  chmod 600 /etc/munge/munge.key
  chown munge /etc/munge/munge.key && \
  chown munge /etc/munge/munge.key
  /etc/init.d/munge start
fi

# Copy any pre/post mount scripts
#
[ -e /config/premount.sh ] && cp /config/premount.sh /etc/shifter/premount.sh
[ -e /config/postmount.sh ] && cp /config/postmount.sh /etc/shifter/postmount.sh

#
# Add a test user if ADDUSER is defined
#
if [ ! -z "$ADDUSER" ] ; then
  useradd -m $ADDUSER -s /bin/bash
  cp -a /root/.ssh/ /home/$ADDUSER/
  chown $ADDUSER -R /home/$ADDUSER/.ssh/
fi

# Do this at the end in case we add a user
mkdir -p  /opt/shifter/default/etc_files
getent passwd > /opt/shifter/default/etc_files/passwd
getent group > /opt/shifter/default/etc_files/group
cp /etc/nsswitch.conf /opt/shifter/default/etc_files/

# Start up sshd (typically a bad idea)
#
/usr/sbin/sshd -D
