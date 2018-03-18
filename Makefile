EXECS= client server

all:	$(EXECS)

client:  
	gcc -g -Wall -o client client.c -lpthread

server: 
	gcc -g -Wall -o server server.c
	
clean:
	-rm -f $(EXECS) *.o *.~
