Installing Shifter on a RHEL/Centos/Scientific Linux 7 System
*************************************************************

Building RPMs
=============

First, ensure your build system has all necessary packages installed::

    yum install epel-release
    yum install rpm-build gcc glibc-devel munge libcurl-devel json-c \
        json-c-devel pam-devel munge-devel libtool autoconf automake \
        gcc-c++ python-pip xfsprogs squashfs-tools python-devel

Next, if not using a prepared source tarball, generate one from the repo::

    git clone https://github.com/NERSC/shifter.git
    [[ perform any needed git operations to get a particular branch or commit
       you require ]]
    VERSION=$(grep Version: shifter/shifter.spec | awk '{print $2}')
    cp -rp shifter "shifter-$VERSION"
    tar cf "shifter-$VERSION.tar.gz" "shifter-$VERSION"

Build the RPMs from your tarball::

    rpmbuild -tb "shifter-$VERSION.tar.gz"

*Note about SLURM support*
To build with SLURM support do::

    rpmbuild -tb "shifter-$VERSION.tar.gz" --define "with_slurm /usr"

Change "/usr" to the base path SLURM is installed in.

Installing the Image Manager
============================

Install the Shifter runtime RPM

    yum -y install shifter{,-imagegw}-1*rpm

Create a config, create directories, start Mongo.

    cp /etc/shifter/imagemanager.json.example /etc/shifter/imagemanager.json
    mkdir /images
    mkdir -p /data/db
    mongod --smallfiles &

Start Munge

    echo "abcdefghijklkmnopqrstuvwxyz0123456" > /etc/munge/munge.key
    chown munge.munge /etc/munge/munge.key
    chmod 600 /etc/munge/munge.key
    runuser -u munge munged

Start the image gateway and a worker for "mycluster"

    gunicorn -b 0.0.0.0:5000 --backlog 2048  shifter_imagegw.api:app &

Installing the Runtime
============================

    yum -y install shifter{,-runtime}-1*rpm
    cp /etc/shifter/udiRoot.conf.example etc/shifter/udiRoot.conf
    sed -i 's|etcPath=.*|etcPath=/etc/shifter/shifter_etc_files/|' /etc/shifter/udiRoot.conf
    sed -i 's|imagegwapi:5000|localhost:5000|' /etc/shifter/udiRoot.conf
    echo "abcdefghijklkmnopqrstuvwxyz0123456" > /etc/munge/munge.key
    chown munge.munge /etc/munge/munge.key
    chmod 600 /etc/munge/munge.key
    runuser -u munge munged
