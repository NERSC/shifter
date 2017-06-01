#!/bin/bash

if [ -e "/config/imagemanager.json" ] ; then
  echo "Copying configuration from /config"
  cp /config/imagemanager.json .
else
  echo "Use the volume mount to pass a configuration"
  echo "into the container (e.g. -v `pwd`/myconfig:/config)"
fi

for service in $@ ; do
  echo "service: $service"
  if [ "$service"  == "api" ] ; then
    gunicorn -b 0.0.0.0:5000 --log-file /var/log/gunicorn.log --log-level DEBUG  --backlog 2048 shifter_imagegw.api:app
  elif  [ $(echo $service|grep -c "worker:") -gt 0 ] ; then
    queue=$(echo $service|sed 's/.*://')
    echo "Worker Queue: $queue"
    export PYTHONPATH=`pwd`
    celery -c 1 -A shifter_imagegw.imageworker worker -Q $queue --loglevel=debug &
  elif  [ "$service"  == "flower" ] ; then
    flower -A imageworker &
  elif  [ $(echo $service|grep -c "munge:") -gt 0 ] ; then
    socket=$(echo $service|awk -F: '{print $2}')
    key=$(echo $service|awk -F: '{print $3}')
    cp /config/$key.key /etc/munge/$socket.key
    chown munge /etc/munge/$socket.key
    chmod 600 /etc/munge/$socket.key
    runuser -u munge -- /usr/sbin/munged  -S /var/run/munge/${socket}.socket --key-file=/etc/munge/$socket.key --force -F &
  else
    echo "$service not recognized"
  fi
done

wait

if [ $# -eq 0 ] ; then
  bash
fi
