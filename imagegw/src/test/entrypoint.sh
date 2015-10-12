#!/bin/bash


/etc/init.d/munge start

/usr/sbin/sshd -D
