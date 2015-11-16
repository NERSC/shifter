#!/bin/sh

export PATH=$PATH:$(pwd)/test/

function refreshmunge {
  export MUNGE=$(grep auth ~/.dockercfg |sed 's/",//'|sed 's/.*"//'|systema munge )
}

function update {
  IP=$(docker-machine ip $(docker-machine active))
  PORT=$(docker-compose ps systema|grep systema|sed 's/.*://'|sed 's/-.*//')
  alias systema="ssh -i test/ssh.key $IP -p $PORT -l root"
  echo y|ssh -i test/ssh.key $IP -p $PORT -l root date
  export IMAGEGW="$IP:5555"
}
