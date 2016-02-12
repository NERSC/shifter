FROM python:2.7-slim

RUN apt-get update && apt-get -y install openssh-client squashfs-tools munge
#ADD test/ssh.key /ssh/ssh.key
#ADD test/munge.key /etc/munge/munge.key

#RUN chmod 600 /ssh/ssh.key && \
#    echo "   StrictHostKeyChecking no" >> /etc/ssh/ssh_config && \
#    chmod 600 /etc/munge/munge.key && chown munge /etc/munge/munge.key

RUN echo "   StrictHostKeyChecking no" >> /etc/ssh/ssh_config
RUN mkdir /var/run/munge && chown munge /var/run/munge


WORKDIR /usr/src/app

ADD requirements.txt /usr/src/app/

RUN pip install --no-cache-dir -r requirements.txt

COPY . /usr/src/app

ENTRYPOINT [ "./entrypoint.sh" ]
CMD [ ]

