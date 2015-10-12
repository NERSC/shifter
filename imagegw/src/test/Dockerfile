#
# NOTE:  This is onlu used for testing purposes.
#        This is meant to emulate a system, so it runs munge and sshd which would
#        normally be a bad idea.


FROM ubuntu:14.04
MAINTAINER Shane Canon <scanon@lbl.gov>

# Install and configure ssh
#
# Thanks to Sven Dowideit <SvenDowideit@docker.com>
#
RUN apt-get update && apt-get install -y openssh-server munge && \
  mkdir /var/run/sshd && \
  echo 'root:lookatmenow' | chpasswd && \
  sed -i 's/PermitRootLogin without-password/PermitRootLogin yes/' /etc/ssh/sshd_config && \
  sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

ENV NOTVISIBLE "in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile

# Add the key, entrypoint and munge key
ADD ssh.pub /root/.ssh/authorized_keys
ADD entrypoint.sh /entrypoint.sh
ADD ./munge.key /etc/munge/munge.key

RUN chmod 700 /root/.ssh && \
    echo "   StrictHostKeyChecking no" >> /etc/ssh/ssh_config && \
    chmod 600 /etc/munge/munge.key && chown munge /etc/munge/munge.key && \
    chmod 755 /var/log/

EXPOSE 22
ENTRYPOINT [ "/entrypoint.sh" ]

