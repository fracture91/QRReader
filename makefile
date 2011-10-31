LIB=-Wall
CCPP=g++

all: server.elf client.elf

server.elf:
	$(CCPP) server.cpp -o server.elf $(LIB)

client.elf:
	$(CCPP) client.cpp -o client.elf $(LIB)

clean: 
	rm -f *.elf
