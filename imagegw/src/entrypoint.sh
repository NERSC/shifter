#!/bin/bash

Q=systema

su -c "munged  -S /var/run/munge/systema.socket --key-file=/etc/munge/munge.key --force" munge
if [ "$1"  == "api" ] ; then
  python ./imagegwapi.py 

elif  [ "$1"  == "worker" ] ; then
  celery -A imageworker worker -Q $Q --loglevel=debug

elif  [ "$1"  == "flower" ] ; then
  flower -A imageworker

elif  [ "$1"  == "all" ] ; then
  celery -A imageworker worker -Q $Q --loglevel=debug &
  python ./imagegwapi.py 
else

  bash
fi
