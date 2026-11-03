server: server.c
	$(CC) -O2 -Wall -Wextra -o server server.c
clean:
	rm -f server
