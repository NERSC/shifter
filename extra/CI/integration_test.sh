#!/bin/bash

CIDIR="$BUILDDIR/extra/CI"
if [[ ! -e "$CIDIR" ]]; then
    echo "No CI dir found!"
    exit 1
fi

LIBEXECDIR=
for path in /usr/libexec/shifter /usr/lib/shifter; do
    if [[ -e $path ]]; then
        LIBEXECDIR=$path
    fi
done
if [[ -z "$LIBEXECDIR" ]]; then
    echo "FAILED to find shifter libexec dir"
    exit 1
fi

echo "Setting up imagegw configuration"
sudo cp "$CIDIR/imagemanager.json" /etc/shifter

me=$(whoami)
for i in /var/log/shifter_imagegw /var/log/shifter_imagegw_worker /images; do
    sudo mkdir -p $i
    sudo chown -R $me $i
done

echo "Starting imagegw api"
gunicorn -b 0.0.0.0:5000 --backlog 2048 --access-logfile=/var/log/shifter_imagegw/access.log --log-file=/var/log/shifter_imagegw/error.log shifter_imagegw.api:app &

echo "Starting image worker"
celery -A shifter_imagegw.imageworker worker -Q mycluster -n mycluster.%h --loglevel=debug --logfile=/var/log/shifter_imagegw_worker/mycluster.log &

echo "Pull Image"
shifterimg pull ubuntu:14.04

ls /images
