# NOTE:  This Dockerfile is used to build a test shifter image.  The image can be used
#        to mimic a system (login node) that would have shifter installed.
#        This is only used for testing purposes.
#        It runs munge and sshd which would normally be a bad idea for a container.
#
#        If you are looking to build an image for the gateway.  Look at the Dockerfile
#        in imagegw/src.

FROM ubuntu:14.04
MAINTAINER Shane Canon <scanon@lbl.gov>

# Thanks to Sven Dowideit <SvenDowideit@docker.com>
# for a Dockerfile that configured ssh

# Install requirements to build shifter, run munge, and openssh
RUN apt-get update && \
       apt-get install -y gcc autoconf make libtool g++ munge libmunge-dev  \
       libcurl4-openssl-dev libjson-c-dev build-essential openssh-server  \
    curl && \
    mkdir /var/run/sshd && \
    echo 'root:lookatmenow' | chpasswd && \
    sed -i 's/PermitRootLogin without-password/PermitRootLogin yes/' /etc/ssh/sshd_config && \
    sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd && \
    echo "export VISIBLE=now" >> /etc/profile

ENV NOTVISIBLE "in users profile"

ADD . /src/

RUN \
       cd /src/ && \
       cp /bin/mount /src/dep/mount && \
       sh ./autogen.sh && \
       tar cf /src/dep/udiRoot_dep.tar /bin/mount && \
       ./configure --prefix=/opt/shifter/udiRoot/1.0 --sysconfdir=/etc/shifter  \
           --with-libcurl --with-munge --disable-nativeSlurm --disable-staticsshd && \
       make && make install

ADD imagegw/test/entrypoint.sh /entrypoint.sh

# Fix up perms and other things
RUN \
    mkdir /root/.ssh && chmod 700 /root/.ssh && \
    echo "   StrictHostKeyChecking no" >> /etc/ssh/ssh_config && \
    chmod 755 /var/log/ && \
    echo 'PATH=$PATH:/opt/shifter/udiRoot/1.0/bin/' > /etc/profile.d/shifter.sh && \
    chmod 755 /etc/profile.d/shifter.sh

#    chmod 600 /etc/munge/munge.key && chown munge /etc/munge/munge.key && \

ADD ./imagegw/test/premount.sh /etc/shifter/premount.sh
ADD ./imagegw/test/postmount.sh /etc/shifter/postmount.sh

EXPOSE 22
ENTRYPOINT [ "/entrypoint.sh" ]
