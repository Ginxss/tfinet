#define TFI_WINSOCK
#include "../tfinet.h"

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s port\n", argv[0]);
		return 1;
	}

	tfi_server *server = tfi_start_server(atoi(argv[1]), TFI_UDP, 0);

	tfi_address address;
	char buffer[4];
	buffer[3] = 0;
	tfi_recvfrom_all(server->socket, &address, buffer, 3);
	printf("Received: %s\n", buffer);

	tfi_sendto_all(server->socket, address, "ACK", 3);

	tfi_close_server(server);
	tfi_cleanup();
	return 0;
}