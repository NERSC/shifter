#!/bin/sh

function refreshmunge {
  export MUNGE=$(grep auth ~/.dockercfg |sed 's/",//'|sed 's/.*"//'|systema munge )
}

function refresha {
  IP=$(docker-machine ip $(docker-machine active))
  PORT=$(docker-compose ps systema|grep systema|sed 's/.*://'|sed 's/-.*//')
  alias systema="ssh -i test/ssh.key $IP -p $PORT -l root"
  systema -y date
}
