#include "../tfinet.h"

int main(int argc, char *argv[]) {
    tfi_client *client = tfi_connect("127.0.0.1", 1234);

    char buffer[9];
    buffer[8] = 0;
    recv(client->socket.s, buffer, 8, MSG_WAITALL);
    printf("Received: %s\n", buffer);

    send(client->socket.s, "Thanks!", 7, 0);

    tfi_close_client(client);
    tfi_cleanup();
	return 0;
}