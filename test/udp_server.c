#include "../tfinet.h"

int main(int argc, char *argv[]) {
	tfi_server *server = tfi_start_server(1234, "udp", 0);

	tfi_address address;
	char buffer[256];
	int rv = tfi_recvfrom_all(server->socket, &address, buffer, 3);
	if (rv <= 0) {
		tfi_close_server(server);
		tfi_cleanup();
		return 1;
	}
	buffer[rv] = 0;
	printf("Received: %s\n", buffer);

	rv = tfi_sendto_all(server->socket, address, "ACK", 3);
	if (rv < 0) {
		tfi_close_server(server);
		tfi_cleanup();
		return 1;
	}

	tfi_close_server(server);
	tfi_cleanup();
	return 0;
}