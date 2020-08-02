#define TFI_WINSOCK
#include "../tfinet.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
		printf("Usage: %s host port\n", argv[0]);
        return 1;
	}

    tfi_client *client = tfi_start_client(argv[1], atoi(argv[2]), TFI_TCP);

    char buffer[9];
	buffer[8] = 0;
	tfi_recv_all(client->socket, buffer, 8);
	printf("Received: %s\n", buffer);

    tfi_send_all(client->socket, "Thanks!", 7);

    tfi_close_client(client);
    tfi_cleanup();
	return 0;
}