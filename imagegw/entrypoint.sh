#!/bin/bash
if [ -z $LOG_LEVEL ] ; then
  LOG_LEVEL=INFO
fi
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
    python ./shifter_imagegw/api.py
  elif  [ $(echo $service|grep -c "munge:") -gt 0 ] ; then
    socket=$(echo $service|awk -F: '{print $2}')
    key=$(echo $service|awk -F: '{print $3}')
    cp /config/$key.key /etc/munge/$socket.key
    chown munge /etc/munge/$socket.key
    chmod 600 /etc/munge/$socket.key
    runuser -u munge -- /usr/sbin/munged  -S /var/run/munge/${socket}.socket --key-file=/etc/munge/$socket.key --force -F &
  elif [ "$service"  == "munge" ] ; then
    cp /config/munge.key /etc/munge/munge.key
    chown munge /etc/munge/munge.key
    chmod 600 /etc/munge/munge.key
    runuser -u munge -- /usr/sbin/munged  --key-file=/etc/munge/munge.key --force -F
  else
    echo "$service not recognized"
  fi
done

wait

if [ $# -eq 0 ] ; then
  bash
fi
