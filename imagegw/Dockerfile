FROM python:3.8-slim as tools

RUN \
    apt-get -y update && apt-get -y install curl gnupg git make wget

RUN \
    echo 'deb https://download.opensuse.org/repositories/devel:/kubic:/libcontainers:/stable/Debian_10/ /' > /etc/apt/sources.list.d/devel:kubic:libcontainers:stable.list  && \
    curl -L https://download.opensuse.org/repositories/devel:/kubic:/libcontainers:/stable/Debian_10/Release.key | apt-key add - && \
    apt-get -y update && \
    apt-get -y install umoci

RUN \
    cd /tmp && \
    wget -q https://go.dev/dl/go1.26.4.linux-amd64.tar.gz && \
    tar xzf go1.26.4.linux-amd64.tar.gz && \
    PATH=/tmp/go/bin:$PATH && \
    ./go/bin/go install github.com/opencontainers/image-tools/cmd/oci-image-tool@v1.0.0-rc3


FROM python:3.8-slim

RUN apt-get -y update && apt-get -y install squashfs-tools munge libassuan0 libgpgme11 ibdevmapper1.02.1

RUN mkdir /var/run/munge && chown munge /var/run/munge

COPY --from=tools /usr/bin/umoci /root/go/bin/oci-image-tool /usr/bin/

RUN apt-get -y install skopeo

WORKDIR /usr/src/app

ADD requirements.txt /usr/src/app/

RUN pip install --no-cache-dir -r requirements.txt

COPY . /usr/src/app

RUN echo "CONFIG_PATH='/config'" >> /usr/src/app/shifter_imagegw/__init__.py

ENV PYTHONPATH=/usr/src/app/

RUN cp /usr/src/app/oci-image-tool-wrapper /usr/local/bin/oci-image-tool-wrapper

# Test that the tools work
RUN \
    skopeo --version && umoci --version && /usr/bin/oci-image-tool --version

ENTRYPOINT [ "./entrypoint.sh" ]
CMD [ ]

