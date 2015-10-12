FROM python:2.7-slim

RUN apt-get update && apt-get -y install openssh-client squashfs-tools munge
ADD test/ssh.key /ssh/ssh.key
ADD test/munge.key /etc/munge/munge.key

RUN chmod 600 /ssh/ssh.key && \
    echo "   StrictHostKeyChecking no" >> /etc/ssh/ssh_config && \
    chmod 600 /etc/munge/munge.key && chown munge /etc/munge/munge.key


WORKDIR /usr/src/app

COPY requirements.txt /usr/src/app/
RUN pip install --no-cache-dir -r requirements.txt

COPY . /usr/src/app
RUN cp docker.json imagemanager.json

ENTRYPOINT [ "./entrypoint.sh" ]
CMD [ ]

