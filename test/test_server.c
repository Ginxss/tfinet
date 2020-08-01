#include "../tfinet.h"

void connection_func(tfi_client *client) {
	send(client->socket.s, "Welcome!", 8, 0);
	
	char buffer[8];
	buffer[7] = 0;
	recv(client->socket.s, buffer, 7, MSG_WAITALL);
	printf("Received: %s\n", buffer);
}

int main(int argc, char *argv[]) {
	tfi_server *server = tfi_start_server(1234, 10);
	tfi_client *client = tfi_accept(server->socket);
	tfi_client_thread *ct = tfi_start_client_thread(client, connection_func);

	tfi_wait_for_client_thread(ct);

	tfi_close_server(server);
	tfi_cleanup();
	return 0;
}