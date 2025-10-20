FROM python:3.10

RUN \
    apt-get -y update && \
    apt-get -y install squashfs-tools munge libgpgme11 ibdevmapper1.02.1 skopeo oci-image-tool


RUN mkdir /var/run/munge && chown munge /var/run/munge

WORKDIR /usr/src/app

ADD requirements.txt /usr/src/app/

RUN pip install --no-cache-dir -r requirements.txt

RUN apt-get -y install umoci
COPY . /usr/src/app

RUN echo "CONFIG_PATH='/config'" >> /usr/src/app/shifter_imagegw/__init__.py

ENV PYTHONPATH=/usr/src/app/

ENTRYPOINT [ "./entrypoint.sh" ]
CMD [ ]

