# NOTE:  This Dockerfile is used to build a test shifter image.  The image can be used
#        to mimic a system (login node) that would have shifter installed.
#        This is only used for testing purposes.
#        It runs munge and sshd which would normally be a bad idea for a container.
#
#        If you are looking to build an image for the gateway.  Look at the Dockerfile
#        in the 'imagegw' directory.

FROM ubuntu:14.04

LABEL name="Shifter"
LABEL version="18.03"
LABEL maintainer="scanon@lbl.gov"

# Thanks to Sven Dowideit <SvenDowideit@docker.com>
# for a Dockerfile that configured ssh

ARG DEBIAN_FRONTEND=noninteractive

# Install requirements to build shifter, run munge, and openssh
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        autoconf \
        automake \
        build-essential \
        curl \
        g++ \
        gcc \
        libcap-dev \
        libcurl4-openssl-dev \
        libjson-c-dev \
        libmunge-dev \
        libtool \
        make \
        munge \
        openssh-server \
        python2.7 \
    && apt-get autoremove -y \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir /var/run/sshd \
    && echo 'root:lookatmenow' | chpasswd \
    && sed -i 's/PermitRootLogin without-password/PermitRootLogin yes/' /etc/ssh/sshd_config \
    && sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd \
    && echo "export VISIBLE=now" >> /etc/profile

ENV NOTVISIBLE "in users profile"

ADD . /src/

RUN cd /src/ \
    && cp /bin/mount /src/dep/mount \
    && touch configure.ac \
    && sh ./autogen.sh \
    && tar cf /src/dep/udiRoot_dep.tar /bin/mount \
    && ./configure \
        --prefix=/opt/shifter/udiRoot/1.0 \
        --sysconfdir=/etc/shifter \
        --with-libcurl \
        --with-munge \
        --disable-nativeSlurm \
        --disable-staticsshd \
    && make \
    && make install

ADD imagegw/test/entrypoint.sh /entrypoint.sh

# Fix up perms and other things
RUN mkdir /root/.ssh \
    && chmod 700 /root/.ssh \
    && echo "   StrictHostKeyChecking no" >> /etc/ssh/ssh_config \
    && chmod 755 /var/log/ \
    && echo 'PATH=$PATH:/opt/shifter/udiRoot/1.0/bin/' > /etc/profile.d/shifter.sh \
    && chmod 755 /etc/profile.d/shifter.sh

#    chmod 600 /etc/munge/munge.key && chown munge /etc/munge/munge.key && \

ADD ./imagegw/test/premount.sh /etc/shifter/premount.sh
ADD ./imagegw/test/postmount.sh /etc/shifter/postmount.sh
ADD ./imagegw/test/test.squashfs /images/test
COPY ./imagegw/shifter_imagegw/fasthash.py /usr/local/bin
RUN chmod a+rx /usr/local/bin/fasthash.py

EXPOSE 22
ENTRYPOINT [ "/entrypoint.sh" ]
