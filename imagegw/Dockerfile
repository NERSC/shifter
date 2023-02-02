FROM python:3.8-slim as tools

RUN \
    apt-get -y update && apt-get -y install curl gnupg golang git make go-md2man

RUN \
    echo 'deb https://download.opensuse.org/repositories/devel:/kubic:/libcontainers:/stable/Debian_10/ /' > /etc/apt/sources.list.d/devel:kubic:libcontainers:stable.list  && \
    curl -L https://download.opensuse.org/repositories/devel:/kubic:/libcontainers:/stable/Debian_10/Release.key | apt-key add - && \
    apt-get -y update && \
    apt-get -y install umoci

RUN \
    go get -d  github.com/opencontainers/image-tools/cmd/oci-image-tool && \
    cd ~/go/src/github.com/opencontainers/image-tools/ && \
    make && make install

# Need newer version of skopeo then what is available from kubic
#FROM debian:sid as skopeo
#RUN apt-get -y update && apt-get -y install skopeo

FROM debian:bullseye as skopeo
RUN \
    apt-get -y update && \
    apt-get -y install golang wget go-md2man git make libgpgme-dev libdevmapper-dev

RUN \
    git clone https://github.com/containers/skopeo $GOPATH/src/github.com/containers/skopeo && \
    cd $GOPATH/src/github.com/containers/skopeo && \
    git checkout v1.4.1 && make bin/skopeo && \
    DISABLE_DOCS=1 make



FROM python:3.8-slim

RUN apt-get -y update && apt-get -y install squashfs-tools munge libassuan0 libgpgme11 ibdevmapper1.02.1

RUN mkdir /var/run/munge && chown munge /var/run/munge

COPY --from=tools /usr/bin/umoci /usr/bin/oci-image-tool /usr/bin/

RUN apt-get -y install skopeo
COPY --from=skopeo /src/github.com/containers/skopeo/bin/skopeo /usr/bin/

WORKDIR /usr/src/app

ADD requirements.txt /usr/src/app/

RUN pip install --no-cache-dir -r requirements.txt

COPY . /usr/src/app

RUN echo "CONFIG_PATH='/config'" >> /usr/src/app/shifter_imagegw/__init__.py

ENV PYTHONPATH=/usr/src/app/

RUN cp /usr/src/app/oci-image-tool-wrapper /usr/local/bin/oci-image-tool-wrapper

# Test that the tools work
RUN \
    skopeo && umoci && /usr/bin/oci-image-tool


ENTRYPOINT [ "./entrypoint.sh" ]
CMD [ ]

