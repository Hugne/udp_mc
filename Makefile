all: client6 server6

client6: client6.c sockopt.c hexdump.c util.c 
	gcc -ggdb -o client6 client6.c -I.

server6: server6.c
	gcc -ggdb -o server6 server6.c -I.
	
clean:
	rm -rf client6
	rm -rf server6
	rm -rf *.o

