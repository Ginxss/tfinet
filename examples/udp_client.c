#define TFI_WINSOCK
#include "../tfinet.h"

int main(int argc, char *argv[]) {
	if (argc != 3) {
		printf("Usage: %s host port\n", argv[0]);
		return 1;
	}

	tfi_client *client = tfi_start_client(argv[1], (short)atoi(argv[2]), TFI_UDP, TFI_IPv6);

	if (tfi_sendto(client->socket, client->address, "SYN", 3) < 0) {
		return 1;
	}
	printf("Sent: %s\n", "SYN");

	tfi_address address;
	address.length = sizeof(struct sockaddr_in6);
	char buffer[4];
	buffer[3] = 0;
	if (tfi_recvfrom(client->socket, &address, buffer, 3) < 0) {
		return 1;
	}
	printf("Received: %s\n", buffer);

	tfi_close_client(client);
	tfi_cleanup();
	return 0;
}