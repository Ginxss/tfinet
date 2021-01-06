#ifndef TFINET_DEFINED_H
#define TFINET_DEFINED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TFI_WINSOCK
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#endif


// *** STRUCTS *** //

typedef enum {
	TFI_IPv4, TFI_IPv6
} tfi_ip_version;

typedef enum {
	TFI_TCP, TFI_UDP
} tfi_transport_protocol;

#ifdef TFI_WINSOCK
typedef struct {
	SOCKET s;
} tfi_socket;
#else
typedef struct {
	int s;
} tfi_socket;
#endif

typedef struct {
	struct sockaddr_storage a;
	socklen_t length;
} tfi_address;

typedef struct {
	tfi_socket socket;
	tfi_address address;
} tfi_server, tfi_client;

#ifdef TFI_WINSOCK
typedef struct {
	HANDLE thread;
	void (*function)(tfi_client *);
	tfi_client *param;
} tfi_client_thread;
#else
typedef struct {
	pthread_t thread;
	void (*function)(tfi_client *);
	tfi_client *param;
} tfi_client_thread;
#endif


// *** CLEANUP & ERROR HANDLING *** //


void tfi_close_server(tfi_server *tfi) {
	#ifdef TFI_WINSOCK
	closesocket(tfi->socket.s);
	#else
	close(tfi->socket.s);
	#endif

	free(tfi);
}

// if the client is handled in a thread, this is automatically called when the function ends
void tfi_close_client(tfi_client *tfi) {
	#ifdef TFI_WINSOCK
	closesocket(tfi->socket.s);
	#else
	close(tfi->socket.s);
	#endif

	free(tfi);
}

void tfi_cleanup() {
	#ifdef TFI_WINSOCK
	WSACleanup();
	#endif
}

int invalid_socket(tfi_socket socket, char *function) {
	#ifdef TFI_WINSOCK
	if (socket.s == INVALID_SOCKET) {
		printf("%s: Error code %d\n", function, WSAGetLastError());
	#else
	if (socket.s < 0) {
		printf("%s: Error code %d\n", function, errno);
	#endif

		return 1;
	}

	return 0;
}

int socket_error(int rv, char *function) {
	#ifdef TFI_WINSOCK
	if (rv == SOCKET_ERROR) {
		printf("%s: Error code %d\n", function, WSAGetLastError());
	#else
	if (rv < 0) {
		printf("%s: Error code %d\n", function, errno);
	#endif

		return 1;
	}

	return 0;
}


// *** SOCKETS *** //


// protocol = TFI_TCP: after this call, you can call accept
// protocol = TFI_UDP: after this call, you can communicate through the server socket
tfi_server *tfi_start_server(short port, tfi_transport_protocol protocol, tfi_ip_version version, int tcp_max_connections) {
	tfi_server *tfi = (tfi_server *)malloc(sizeof(tfi_server));
	int rv;

	#ifdef TFI_WINSOCK
	// start WinSock
	WSADATA wsa;
	rv = WSAStartup(MAKEWORD(2, 0), &wsa);
	if (rv != 0) {
		printf("tfi_start_server(): WSAStartup(): Error code %d\n", rv);
		free(tfi);
		return NULL;
	}
	#endif

	// create server socket
	int af = (version == TFI_IPv6) ? AF_INET6 : AF_INET;
	int type = (protocol == TFI_UDP) ? SOCK_DGRAM : SOCK_STREAM;
	tfi->socket.s = socket(af, type, 0);
	if (invalid_socket(tfi->socket, "tfi_start_server(): socket()")) {
		free(tfi);
		tfi_cleanup();
		return NULL;
	}

	// fill own address structure
	memset(&tfi->address.a, 0, sizeof(tfi->address.a));
	if (version == TFI_IPv6) {
		struct sockaddr_in6 addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin6_family = AF_INET6;
		addr.sin6_port = htons(port);
		addr.sin6_addr = in6addr_any;

		memcpy(&tfi->address.a, &addr, sizeof(addr));
		tfi->address.length = sizeof(addr);
	}
	else { // TFI_IPv4
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_ANY;

		memcpy(&tfi->address.a, &addr, sizeof(addr));
		tfi->address.length = sizeof(addr);
	}

	// bind to address / port
	rv = bind(tfi->socket.s, (struct sockaddr *)&tfi->address.a, tfi->address.length);
	if (socket_error(rv, "tfi_start_server(): bind()")) {
		tfi_close_server(tfi);
		tfi_cleanup();
		return NULL;
	}

	if (protocol == TFI_TCP) {
		// listen with accept socket
		rv = listen(tfi->socket.s, tcp_max_connections);
		if (socket_error(rv, "tfi_start_server(): listen()")) {
			tfi_close_server(tfi);
			tfi_cleanup();
			return NULL;
		}
	}

	return tfi;
}

// accept new tcp connection (blocking)
// after this call, you can communicate through the client socket
tfi_client *tfi_accept(tfi_server *server) {
	tfi_client *tfi = (tfi_client *)malloc(sizeof(tfi_client));
	tfi->address.length = server->address.length;

	tfi->socket.s = accept(server->socket.s, (struct sockaddr *)&tfi->address.a, &tfi->address.length);
	if (invalid_socket(tfi->socket, "tfi_accept(): accept()")) {
		free(tfi);
		return NULL;
	}

	return tfi;
}

// after this call, you can communicate through the client socket
tfi_client *tfi_start_client(char *host, short port, tfi_transport_protocol protocol, tfi_ip_version version) {
	tfi_client *tfi = (tfi_client *)malloc(sizeof(tfi_client));
	int rv;

	#ifdef TFI_WINSOCK
	// start WinSock
	WSADATA wsa;
	rv = WSAStartup(MAKEWORD(2, 0), &wsa);
	if (rv != 0) {
		printf("tfi_start_client(): WSAStartup(): Error code %d\n", rv);
		free(tfi);
		return NULL;
	}
	#endif

	// create socket
	int af = (version == TFI_IPv6) ? AF_INET6 : AF_INET;
	int type = (protocol == TFI_UDP) ? SOCK_DGRAM : SOCK_STREAM;
	tfi->socket.s = socket(af, type, 0);
	if (invalid_socket(tfi->socket, "tfi_start_server(): socket()")) {
		free(tfi);
		tfi_cleanup();
		return NULL;
	}

	// fill server address structure
	memset(&tfi->address.a, 0, sizeof(tfi->address.a));
	if (version == TFI_IPv6) {
		struct sockaddr_in6 addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin6_family = AF_INET6;
		addr.sin6_port = htons(port);
		inet_pton(AF_INET6, host, &addr.sin6_addr);

		memcpy(&tfi->address.a, &addr, sizeof(addr));
		tfi->address.length = sizeof(addr);
	}
	else { // TFI_IPv4
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_pton(AF_INET, host, &addr.sin_addr);

		memcpy(&tfi->address.a, &addr, sizeof(addr));
		tfi->address.length = sizeof(addr);
	}

	if (protocol == TFI_TCP) {
		// connect to host
		rv = connect(tfi->socket.s, (struct sockaddr *)&tfi->address.a, tfi->address.length);
		if (socket_error(rv, "tfi_start_client(): connect()")) {
			tfi_close_client(tfi);
			tfi_cleanup();
			return NULL;
		}
	}

	return tfi;
}


// *** THREADS *** //


// internal thread function
#ifdef TFI_WINSOCK
DWORD WINAPI thread_function(void* data) {
	tfi_client_thread *ct = (tfi_client_thread *)data;

	ct->function(ct->param);

	CloseHandle(ct->thread);
	tfi_close_client(ct->param);
	free(ct);
	return 0;
}
#else
void *thread_function(void *data) {
	tfi_client_thread *ct = (tfi_client_thread *)data;

	ct->function(ct->param);

	tfi_close_client(ct->param);
	free(ct);
	return 0;
}
#endif

// starts a new thread that calls 'function'
// after completion of 'function', the client and the client thread are automatically closed
tfi_client_thread *tfi_start_client_thread(tfi_client *client, void (*function)(tfi_client *)) {
	tfi_client_thread *ct = (tfi_client_thread *)malloc(sizeof(tfi_client_thread));

	ct->function = function;
	ct->param = client;

	#ifdef TFI_WINSOCK
	ct->thread = CreateThread(NULL, 0, thread_function, ct, 0, NULL);
	if (ct->thread == NULL) {
		printf("tfi_start_client_thread(): CreateThread(): Error code %d\n", GetLastError());
		free(ct);
		return NULL;
	}
	#else
	int rv = pthread_create(&ct->thread, NULL, thread_function, ct);
	if (rv != 0) {
		printf("tfi_start_client_thread(): pthread_create(): Error code %d\n", rv);
		free(ct);
		return NULL;
	}
	#endif

	return ct;
}

void tfi_wait_for_client_thread(tfi_client_thread *ct) {
	#ifdef TFI_WINSOCK
	WaitForSingleObject(ct->thread, INFINITE);
	#else
	pthread_join(ct->thread, NULL);
	#endif
}

// this is called automatically when the thread function completes
// can also be used to manually terminate the thread and close the client
void tfi_close_client_thread(tfi_client_thread *ct) {
	#ifdef TFI_WINSOCK
	TerminateThread(ct->thread, 1);
	CloseHandle(ct->thread);
	#else
	pthread_cancel(ct->thread);
	#endif

	tfi_close_client(ct->param);
	free(ct);
}


// *** SEND & RECEIVE *** //

// tcp send message
// returns the number of bytes sent or -1 if an error occurred
int tfi_send(tfi_socket socket, char *msg, int msglen) {
	int sent = send(socket.s, msg, msglen, 0);
	if (socket_error(sent, "tfi_send(): send()")) {
		return -1;
	}

	return sent;
}

// tcp receive message
// returns the number of bytes received or -1 if an error occurred
int tfi_recv(tfi_socket socket, char *buffer, int msglen) {
	int received = recv(socket.s, buffer, msglen, 0);
	if (socket_error(received, "tfi_recv(): recv()")) {
		return -1;
	}

	return received;
}

// tcp send entire message
// returns the number of bytes sent (which should always be msglen) or -1 if an error occurred
int tfi_send_all(tfi_socket socket, char *msg, int msglen) {
	int rv;

	int sent = 0;
	while (sent < msglen) {
		rv = send(socket.s, msg + sent, msglen - sent, 0);
		if (socket_error(rv, "tfi_send_all(): send()")) {
			return -1;
		}

		sent += rv;
	}

	return sent;
}

// tcp receive entire message
// returns the number of bytes received (which should always be msglen) or 0 if the connection was closed gracefully or -1 if an error occurred
int tfi_recv_all(tfi_socket socket, char *buffer, int msglen) {
	int rv;

	int received = 0;
	while (received < msglen) {
		rv = recv(socket.s, buffer + received, msglen - received, 0);
		if (socket_error(rv, "tfi_recv_all(): recv()")) {
			return -1;
		}
		if (rv == 0) {
			return 0;
		}

		received += rv;
	}

	return received;
}

// udp send message
// returns the number of bytes sent or -1 if an error occurred
int tfi_sendto(tfi_socket socket, tfi_address to_address, char *msg, int msglen) {
	int sent = sendto(socket.s, msg, msglen, 0, (struct sockaddr *)&to_address.a, to_address.length);
	if (socket_error(sent, "tfi_sendto_all(): sendto()")) {
		return -1;
	}

	return sent;
}

// udp receive message and fill 'from'->a with client address
// must contain the correct address length beforehand (sizeof(sockaddr_in) or sizeof(sockaddr_in6))
// returns the number of bytes received or 0 if the connection was closed gracefully or -1 if an error occurred
int tfi_recvfrom(tfi_socket socket, tfi_address *from, char *buffer, int msglen) {
	socklen_t addrlen = from->length;
	int received = recvfrom(socket.s, buffer, msglen, 0, (struct sockaddr *)&from->a, &addrlen);
	if (socket_error(received, "tfi_recvfrom(): recvfrom()")) {
		return -1;
	}

	return received;
}

#endif