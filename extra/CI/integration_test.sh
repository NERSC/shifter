#!/bin/bash
set -ex

CIDIR="$BUILDDIR/extra/CI"
if [[ ! -e "$CIDIR" ]]; then
    echo "No CI dir found!"
    exit 1
fi

PYDIR=
for libpath in lib lib64; do
    for pypath in python2.6 python2.7; do
        for packagepath in site-packages dist-packages; do
            if [[ -e "/usr/$libpath/$pypath/$packagepath/shifter_imagegw" ]]; then
                PYDIR="/usr/$libpath/$pypath/$packagepath"
            fi
        done
    done
done
if [[ -z "$PYDIR" ]]; then
    echo "FAILED to find python dir"
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

export PYTHONPATH="$LIBEXECDIR:$PYDIR"

echo "Setting up imagegw configuration"
sudo cp "$CIDIR/imagemanager.json" /etc/shifter

me=$(whoami)
for i in /var/log/shifter_imagegw /images; do
    sudo mkdir -p $i
    sudo chown -R $me $i
done

echo "setting up munge"
if [[ -e /usr/sbin/create-munge-key ]]; then
    sudo /usr/sbin/create-munge-key
fi
sudo service munge start

echo "Starting imagegw api"
gunicorn -b 0.0.0.0:5000 --backlog 2048 --access-logfile=/var/log/shifter_imagegw/access.log --log-file=/var/log/shifter_imagegw/error.log shifter_imagegw.api:app &

echo "setting up base config"
sudo /bin/bash -c "cat /etc/shifter/udiRoot.conf.example | \
         sed 's|etcPath=.*|etcPath=/etc/shifter/shifter_etc_files|g' | \
         sed 's|imageGateway=.*|imageGateway=http://localhost:5000|g' \
         > /etc/shifter/udiRoot.conf"

sudo mkdir -p /etc/shifter/shifter_etc_files
sudo /bin/bash -c "getent passwd > /etc/shifter/shifter_etc_files/passwd"
sudo /bin/bash -c "getent group > /etc/shifter/shifter_etc_files/group"
sudo touch /etc/shifter/shifter_etc_files/shadow
sudo cp "$BUILDDIR/etc_files/nsswitch.conf" /etc/shifter/shifter_etc_files/nsswitch.conf

cat /etc/shifter/udiRoot.conf | egrep -v '^#'
sudo mkdir -p $LIBEXECDIR/opt/udiImage

echo "creating dummy command line tools for GPU support integration tests"
sudo touch /bin/nvidia-smi
sudo chmod 755 /bin/nvidia-smi
sudo touch /bin/nvidia-modprobe
sudo chmod 755 /bin/nvidia-modprobe

## need to sleep a bit to let gunicorn get started
sleep 10


echo "Pull Image"
shifterimg pull ubuntu:16.04
shifterimg lookup ubuntu:16.04
ls /images

echo "Ensure container gets basic setup"
python $CIDIR/integration/test_shifterConfig_format.py ubuntu:16.04

echo "Check capabilities and bounding sets"
python $CIDIR/integration/test_capabilities.py ubuntu:16.04

echo "Check /etc/mtab symlink"
python $CIDIR/integration/test_etcmtab.py ubuntu:16.04

#echo "Test GPU support"
#python $CIDIR/integration/test_gpu_support.py ubuntu:16.04
