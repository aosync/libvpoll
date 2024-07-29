#include "vpoll.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>

struct handler {
	void (*handler)(int fd);
};

void _conn_handler(int fd) {
	// Accept the connection, write a message, and close
	int connfd = accept(fd, NULL, NULL);
	write(connfd, "Hello!\r\n", 8);
	close(connfd);
	printf("served\n");
}
struct handler conn_handler = (struct handler) {
	.handler = _conn_handler,
};

void _stdin_handler(int fd) {
	// Read and print the stdin character
	char r;
	read(fd, &r, 1);
	printf("%c\n", r);
}
struct handler stdin_handler = (struct handler) {
	.handler = _stdin_handler,
};

int main() {
	struct vpoll *vpoll = vpoll_create();

	// Create a socket
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd < 0)
		fprintf(stderr, "error creating the socket");
	struct sockaddr_in addr = (struct sockaddr_in) {
		.sin_family = AF_INET,
		.sin_addr.s_addr = htonl(INADDR_ANY),
		.sin_port = htons(5000),
	};
	if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)))
		fprintf(stderr, "error binding the socket");
	listen(sfd, 128);

	// Register the socket
	vpoll_register(vpoll, (struct vevent) {
		.ident = sfd,
		.events = VPOLL_READ,
		.mode = VPOLL_KEEP,
		.udata = &conn_handler,
	});
	// Register stdin
	vpoll_register(vpoll, (struct vevent) {
		.ident = 0,
		.events = VPOLL_READ,
		.mode = VPOLL_KEEP,
		.udata = &stdin_handler,
	});

	// Event-loop: react to either pending connection or stdin data
	struct vevent ev;
	while (vpoll_wait(vpoll, &ev, 1, 20000)) {
		printf("ident(%d) ", ev.ident);
		((struct handler *)ev.udata)->handler(ev.ident);
	}

	close(sfd);
	vpoll_destroy(vpoll);
}
