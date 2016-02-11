

all:


testprep:
	@[ -e "test/config/munge.key" ] || dd if=/dev/urandom bs=1 count=1024 > test/config/munge.key 
	@[ -e "test/config/ssh.key" ] || (ssh-keygen -t rsa -f test/config/ssh -N '' -C 'Test key' && mv test/config/ssh test/config/ssh.key)


starttest: testprep
	docker build -t imagegwapi .
	docker build -t shifter-test ../../
	docker-compose up -d


getssh:
	$(eval IP=$(shell docker-machine ip $(shell docker-machine active)))
	$(eval PORT=$(shell docker-compose ps systema|grep systema|sed 's/.*://'|sed 's/-.*//'))
	@echo "To connect, do:"
	@echo "ssh -i test/config/ssh.key $(IP) -p $(PORT)"
