#define TFI_WINSOCK
#include "../tfinet.h"

void connection_func(tfi_client *client) {
	if (tfi_send_all(client->socket, "Welcome!", 8) < 0) {
		return;
	}
	printf("Sent: %s\n", "Welcome!");
	
	char buffer[8];
	buffer[7] = 0;
	if (tfi_recv_all(client->socket, buffer, 7) < 0) {
		return;
	}
	printf("Received: %s\n", buffer);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s port\n", argv[0]);
		return 1;
	}

	tfi_server *server = tfi_start_server(atoi(argv[1]), TFI_TCP, 1);
	tfi_client *client = tfi_accept(server);
	tfi_client_thread *ct = tfi_start_client_thread(client, connection_func);

	tfi_wait_for_client_thread(ct);

	tfi_close_server(server);
	tfi_cleanup();
	return 0;
}