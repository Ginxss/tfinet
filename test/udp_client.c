#define TFI_WINSOCK
#include "../tfinet.h"

int main(int argc, char *argv[]) {
	if (argc != 3) {
		printf("Usage: %s host port\n", argv[0]);
        return 1;
	}

    tfi_client *client = tfi_start_client(argv[1], atoi(argv[2]), TFI_UDP);

	tfi_sendto_all(client->socket, client->address, "SYN", 3);

    tfi_address address;
	char buffer[4];
	buffer[3] = 0;
	tfi_recvfrom_all(client->socket, &address, buffer, 3);
	printf("Received: %s\n", buffer);

    tfi_close_client(client);
    tfi_cleanup();
	return 0;
}