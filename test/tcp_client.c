#include "../tfinet.h"

int main(int argc, char *argv[]) {
    tfi_client *client = tfi_start_client("127.0.0.1", 1234, "tcp");

    char buffer[9];
	buffer[8] = 0;
	tfi_recv_all(client->socket, buffer, 8);
	printf("Received: %s\n", buffer);

    tfi_send_all(client->socket, "Thanks!", 7);

    tfi_close_client(client);
    tfi_cleanup();
	return 0;
}