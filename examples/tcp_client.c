#define TFI_WINSOCK
#include "../tfinet.h"

int main(int argc, char *argv[]) {
	if (argc != 3) {
		printf("Usage: %s host port\n", argv[0]);
		return 1;
	}

	tfi_client *client = tfi_start_client(argv[1], (short)atoi(argv[2]), TFI_TCP, TFI_IPv6);

	char buffer[9];
	buffer[8] = 0;
	if (tfi_recv_all(client->socket, buffer, 8) < 0) {
		return 1;
	}
	printf("Received: %s\n", buffer);

	if (tfi_send_all(client->socket, "Thanks!", 7) < 0) {
		return 1;
	}
	printf("Sent: %s\n", "Thanks!");

	tfi_close_client(client);
	tfi_cleanup();
	return 0;
}