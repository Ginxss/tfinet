#define TFI_WINSOCK
#include "../tfinet.h"

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s port\n", argv[0]);
		return 1;
	}

	tfi_server *server = tfi_start_server((short)atoi(argv[1]), TFI_UDP, TFI_IPv6, 0);

	tfi_address address;
	address.length = sizeof(struct sockaddr_in6);
	char buffer[4];
	buffer[3] = 0;
	if (tfi_recvfrom(server->socket, &address, buffer, 3) < 0) {
		return 1;
	}
	printf("Received: %s\n", buffer);

	if (tfi_sendto(server->socket, address, "ACK", 3) < 0) {
		return 1;
	}
	printf("Sent: %s\n", "ACK");

	tfi_close_server(server);
	tfi_cleanup();
	return 0;
}