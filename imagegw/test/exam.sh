#!/bin/sh
path=$1
id=$2

if [ "$2" = "bogus" ] ; then
  exit 0
else
  exit 1
fi
