#!/bin/bash

Q=systema

/etc/init.d/munge start
if [ "$1"  == "api" ] ; then
  python ./imagegwapi.py 

elif  [ "$1"  == "worker" ] ; then
  celery -A imageworker worker -Q $Q

elif  [ "$1"  == "flower" ] ; then
  flower -A imageworker

elif  [ "$1"  == "all" ] ; then
  celery -A imageworker worker -Q $Q  &
  python ./imagegwapi.py 
else

  bash
fi
