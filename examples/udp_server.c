// #define TFI_WINSOCK
#include "../tfinet.h"

// TODO: make recvrom that returns tfi_client, so that it can easily be used in a thread as well
int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s port\n", argv[0]);
		return 1;
	}

	tfi_server *server = tfi_start_server(atoi(argv[1]), TFI_UDP, 0);

	tfi_address address;
	char buffer[4];
	buffer[3] = 0;
	if (tfi_recvfrom_all(server->socket, &address, buffer, 3) < 0) {
		return 1;
	}
	printf("Received: %s\n", buffer);

	if (tfi_sendto_all(server->socket, address, "ACK", 3) < 0) {
		return 1;
	}
	printf("Sent: %s\n", "ACK");

	tfi_close_server(server);
	tfi_cleanup();
	return 0;
}