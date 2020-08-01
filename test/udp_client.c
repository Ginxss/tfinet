#include "../tfinet.h"

int main(int argc, char *argv[]) {
    tfi_client *client = tfi_start_client("127.0.0.1", 1234, "udp");

	int rv = tfi_sendto_all(client->socket, client->address, "SYN", 3);
	if (rv < 0) {
		tfi_close_client(client);
		tfi_cleanup();
		return 1;
	}

    tfi_address address;
	char buffer[256];
	rv = tfi_recvfrom_all(client->socket, &address, buffer, 3);
	if (rv <= 0) {
		tfi_close_client(client);
		tfi_cleanup();
		return 1;
	}
	buffer[rv] = 0;
	printf("Received: %s\n", buffer);

    tfi_close_client(client);
    tfi_cleanup();
	return 0;
}