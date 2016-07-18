#!/bin/sh

pids=$(ps auxww|grep celery|grep systema|grep -v grep|awk '{print $2}')
if [ ! -z "$pids" ] ; then
  echo "Killing $pids"
  ps auxww|grep celery|grep systema|grep -v grep|awk '{print $2}'|xargs kill
fi

exit 0
