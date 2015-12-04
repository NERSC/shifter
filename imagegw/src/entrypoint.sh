#!/bin/bash

QA=systema
QB=systemb

/etc/init.d/munge start
start-stop-daemon --chuid munge --start --exec /usr/sbin/munged  -- -S /var/run/munge/systema.socket --key-file=/etc/munge/munge.key --force
start-stop-daemon --chuid munge --start --exec /usr/sbin/munged  -- -S /var/run/munge/systemb.socket --key-file=/etc/munge/munge.key --force

if [ "$1"  == "api" ] ; then
  python ./imagegwapi.py 

elif  [ "$1"  == "workera" ] ; then
  celery -A imageworker worker -Q $QA --loglevel=info

elif  [ "$1"  == "workerb" ] ; then
  celery -A imageworker worker -Q $QB --loglevel=info

elif  [ "$1"  == "flower" ] ; then
  flower -A imageworker

elif  [ "$1"  == "all" ] ; then
  celery -A imageworker worker -Q $QA --loglevel=info &
  celery -A imageworker worker -Q $QB --loglevel=info &
  python ./imagegwapi.py 
else

  bash
fi
