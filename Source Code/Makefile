CC=gcc
RASPCC=arm-buildroot-linux-gnueabihf-gcc
CFLAGS= -g -I.
DEP1 = -lpthread
DEP2 = -lpthread -lrt

RASPIP=10.42.0.242
IP=127.0.0.1
PORT=5000

############## COMPILATION ###################
.PHONY: all
all:
	$(CC) groupchat_server.c -o server.elf $(DEP1)
	$(CC) groupchat_client.c -o client.elf $(DEP2)
	$(CC) groupchat_client_send.c -o send.elf $(DEP2)

RASPall:
	$(RASPCC) groupchat_server.c -o RASPserver.elf $(DEP1)
	$(RASPCC) groupchat_client.c -o RASPclient.elf $(DEP2)
	$(RASPCC) groupchat_client_send.c -o RASPsend.elf $(DEP2)

############## RUNNNG ###################
run.server:
	./server.elf $(PORT)

RASPrun.server:
	./RASPserver $(PORT)

run.client:
	./client.elf $(IP) $(PORT)

run.client.rasp:
	./client.elf $(RASPIP) $(PORT)

############## CLEAN ###################
.PHONY: clean
clean: 
	rm -f *.elf
