#ifndef TFINET_DEFINED_H
#define TFINET_DEFINED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// return value: 0 = tcp, 1 = udp
int tfi_get_connection_type(char *transport_protocol) {
    if (strcmp(transport_protocol, "tcp") == 0) {
        return 0;
    }
    else if (strcmp(transport_protocol, "udp") == 0) {
        return 1;
    }
    else {
        printf("tfi_get_connection_type(): invalid transport protocol - only 'tcp' and 'udp' are allowed\n");
        return -1;
    }
}

#ifdef TFI_WINSOCK

// *************** //
// *** WINDOWS *** //
// *************** //

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

typedef struct {
	SOCKET s;
} tfi_socket;

typedef struct {
    SOCKADDR_IN a;
} tfi_address;

typedef struct {
    tfi_socket socket;
    tfi_address address;
} tfi_server;
typedef tfi_server tfi_client;

typedef struct {
    HANDLE thread;
	void (*function)(tfi_client *);
	tfi_client *param;
} tfi_client_thread;




// *** SOCKETS *** //

void tfi_close_server(tfi_server *tfi) {
	closesocket(tfi->socket.s);
	free(tfi);
}

// does not need to be called only if the client is handled in a thread
void tfi_close_client(tfi_client *tfi) {
	closesocket(tfi->socket.s);
	free(tfi);
}

void tfi_cleanup() {
    WSACleanup();
}

// 'transport_protocol' = 'tcp': after this call, you can call accept
// 'transport_protocol' = 'udp': after this call, you can communicate through the server socket
tfi_server *tfi_start_server(int port, char *transport_protocol, int tcp_max_connections) {
	tfi_server *tfi = (tfi_server *)malloc(sizeof(tfi_server));
	int rv;

    // tcp or udp
    int type = tfi_get_connection_type(transport_protocol);
    if (type < 0) {
        free(tfi);
        return NULL;
    }

    // start WinSock
    WSADATA wsa;
    rv = WSAStartup(MAKEWORD(2, 0), &wsa);
    if (rv != 0) {
        printf("tfi_start_server(): WSAStartup(): Error code %d\n", rv);
        free(tfi);
        return NULL;
    }

    // create accept socket
    switch (type) {
    case 0: // tcp
        tfi->socket.s = socket(AF_INET, SOCK_STREAM, 0);
        break;

    case 1: // udp
        tfi->socket.s = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (tfi->socket.s == INVALID_SOCKET) {
        printf("tfi_start_server(): socket(): Error code %d\n", WSAGetLastError());
        free(tfi);
        WSACleanup();
        return NULL;
    }

    // fill own address structure
    memset(tfi->address.a.sin_zero, 0, 8);
    tfi->address.a.sin_family = AF_INET;
    tfi->address.a.sin_port = htons(port);
    tfi->address.a.sin_addr.s_addr = INADDR_ANY;

    // bind accept socket to the address / port
    rv = bind(tfi->socket.s, (SOCKADDR *)&tfi->address.a, sizeof(SOCKADDR));
    if (rv == SOCKET_ERROR) {
        printf("tfi_start_server(): bind(): Error code %d\n", WSAGetLastError());
        tfi_close_server(tfi);
        WSACleanup();
        return NULL;
    }

    if (type == 0) { // tcp
        // listen with accept socket
        rv = listen(tfi->socket.s, tcp_max_connections);
        if (rv == SOCKET_ERROR) {
            printf("tfi_start_server(): listen(): Error code %d\n", WSAGetLastError());
            tfi_close_server(tfi);
            WSACleanup();
            return NULL;
        }
    }

	return tfi;
}

// accept new tcp connection (blocking)
// after this call, you can communicate through the client socket
tfi_client *tfi_accept(tfi_socket accept_socket) {
    tfi_client *tfi = (tfi_client *)malloc(sizeof(tfi_client));

    // accept new connection and fill the corresponding address structure
    int addrlen = sizeof(SOCKADDR);
    tfi->socket.s = accept(accept_socket.s, (SOCKADDR *)&tfi->address.a, &addrlen);
    if (tfi->socket.s == INVALID_SOCKET) {
        printf("tfi_accept(): accept(): Error code %d\n", WSAGetLastError());
        free(tfi);
        return NULL;
    }	

    return tfi;
}

// after this call, you can communicate through the client socket
tfi_client *tfi_start_client(char *host, int port, char *transport_protocol) {
    tfi_client *tfi = (tfi_client *)malloc(sizeof(tfi_client));
    int rv;

    // tcp or udp
    int type = tfi_get_connection_type(transport_protocol);
    if (type < 0) {
        free(tfi);
        return NULL;
    }

    // start WinSock
    WSADATA wsa;
    rv = WSAStartup(MAKEWORD(2, 0), &wsa);
    if (rv != 0) {
        printf("tfi_start_client(): WSAStartup(): Error code %d\n", rv);
        free(tfi);
        return NULL;
    }

    // create socket
    switch (type) {
    case 0: // tcp
        tfi->socket.s = socket(AF_INET, SOCK_STREAM, 0);
        break;

    case 1: // udp
        tfi->socket.s = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (tfi->socket.s == INVALID_SOCKET) {
        printf("tfi_start_client(): socket(): Error code %d\n", WSAGetLastError());
        free(tfi);
        WSACleanup();
        return NULL;
    }

    // fill server address structure
    memset(tfi->address.a.sin_zero, 0, 8);
	tfi->address.a.sin_family = AF_INET;
	tfi->address.a.sin_port = htons(port);
    inet_pton(AF_INET, host, &tfi->address.a.sin_addr);

    if (type == 0) { // tcp
        // connect to host
        rv = connect(tfi->socket.s, (SOCKADDR *)&tfi->address.a, sizeof(SOCKADDR));
        if (rv == SOCKET_ERROR) {
            printf("tfi_start_client(): connect(): Error code %d\n", WSAGetLastError());
            tfi_close_client(tfi);
            WSACleanup();
            return NULL;
        }
    }

    return tfi;
}




// *** THREADS *** //

// internal thread function
DWORD WINAPI thread_function(void* data) {
	tfi_client_thread *ct = (tfi_client_thread *)data;

	ct->function(ct->param);

	CloseHandle(ct->thread);
    tfi_close_client(ct->param);
    free(ct);
	return 0;
}

// starts a new thread that calls 'function'
// after completion of 'function', the client and the client thread are automatically closed
tfi_client_thread *tfi_start_client_thread(tfi_client *client, void (*function)(tfi_client *)) {
    tfi_client_thread *ct = (tfi_client_thread *)malloc(sizeof(tfi_client_thread));

    ct->function = function;
    ct->param = client;
    ct->thread = CreateThread(NULL, 0, thread_function, ct, 0, NULL);

    return ct;
}

void tfi_wait_for_client_thread(tfi_client_thread *ct) {
    WaitForSingleObject(ct->thread, INFINITE);
}

// this is called automatically if the thread function completes
// however, this can be used manually to terminate the thread
void tfi_close_client_thread(tfi_client_thread *ct) {
    TerminateThread(ct->thread, 1);

    CloseHandle(ct->thread);
    tfi_close_client(ct->param);
    free(ct);
}




// *** SEND & RECEIVE *** //

// tcp send entire message
// returns the number of bytes sent (which should always be msglen) or -1 if an error occurred
int tfi_send_all(tfi_socket socket, char *msg, int msglen) {
    int rv;

    int sent = 0;
    while (sent < msglen) {
        rv = send(socket.s, msg + sent, msglen - sent, 0);
        if (rv == SOCKET_ERROR) {
            printf("tfi_send_all(): send(): Error code %d\n", WSAGetLastError());
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
        if (rv == SOCKET_ERROR) {
            printf("tfi_recv_all(): recv(): Error code %d\n", WSAGetLastError());
            return -1;
        }
        if (rv == 0) {
            return 0;
        }

        received += rv;
    }

    return received;
}

// udp send entire message
// returns the number of bytes sent (which should always be msglen) or -1 if an error occurred
int tfi_sendto_all(tfi_socket socket, tfi_address addr, char *msg, int msglen) {
    int rv;

    int sent = 0;
    while (sent < msglen) {
        rv = sendto(socket.s, msg + sent, msglen - sent, 0, (SOCKADDR *)&addr.a, sizeof(addr));
        if (rv == SOCKET_ERROR) {
            printf("tfi_sendto_all(): sendto(): Error code %d\n", WSAGetLastError());
            return -1;
        }

        sent += rv;
    }

    return sent;
}

// udp receive entire message
// fills addr
// returns the number of bytes received (which should always be msglen) or 0 if the connection was closed gracefully or -1 if an error occurred
int tfi_recvfrom_all(tfi_socket socket, tfi_address *addr, char *buffer, int msglen) {
    int rv;
    int addrlen = sizeof(SOCKADDR);

    int received = 0;
    while (received < msglen) {
        rv = recvfrom(socket.s, buffer + received, msglen - received, 0, (SOCKADDR *)&addr->a, &addrlen);
        if (rv == SOCKET_ERROR) {
            printf("tfi_recvfrom_all(): recvfrom(): Error code %d\n", WSAGetLastError());
            return -1;
        }
        if (rv == 0) {
            return 0;
        }

        received += rv;
    }

    return received;
}

#else

// ************* //
// *** LINUX *** //
// ************* //

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

typedef struct {
	int s;
} tfi_socket;

typedef struct {
    struct sockaddr_in a;
} tfi_address;

typedef struct {
    tfi_socket socket;
    tfi_address address;
} tfi_server;
typedef tfi_server tfi_client;

typedef struct {
    pthread_t thread;
	void (*function)(tfi_client *);
	tfi_client *param;
} tfi_client_thread;




// *** SOCKETS *** //

void tfi_close_server(tfi_server *tfi) {
	close(tfi->socket.s);
	free(tfi);
}

// does not need to be called only if the client is handled in a thread
void tfi_close_client(tfi_client *tfi) {
	close(tfi->socket.s);
	free(tfi);
}

void tfi_cleanup() {
    // Empty
}

// 'transport_protocol' = 'tcp': after this call, you can call accept
// 'transport_protocol' = 'udp': after this call, you can communicate through the server socket
tfi_server *tfi_start_server(int port, char *transport_protocol, int tcp_max_connections) {
	tfi_server *tfi = (tfi_server *)malloc(sizeof(tfi_server));
	int rv;

    // tcp or udp
    int type = tfi_get_connection_type(transport_protocol);
    if (type < 0) {
        free(tfi);
        return NULL;
    }

    // create accept socket
    switch (type) {
    case 0: // tcp
        tfi->socket.s = socket(AF_INET, SOCK_STREAM, 0);
        break;

    case 1: // udp
        tfi->socket.s = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (tfi->socket.s < 0) {
        printf("tfi_start_server(): socket(): Error code %d\n", errno);
        free(tfi);
        return NULL;
    }

    // fill own address structure
    memset(tfi->address.a.sin_zero, 0, 8);
    tfi->address.a.sin_family = AF_INET;
    tfi->address.a.sin_port = htons(port);
    tfi->address.a.sin_addr.s_addr = INADDR_ANY;

    // bind accept socket to the address / port
    rv = bind(tfi->socket.s, (struct sockaddr *)&tfi->address.a, sizeof(struct sockaddr));
    if (rv < 0) {
        printf("tfi_start_server(): bind(): Error code %d\n", errno);
        tfi_close_server(tfi);
        return NULL;
    }

    if (type == 0) { // tcp
        // listen with accept socket
        rv = listen(tfi->socket.s, tcp_max_connections);
        if (rv < 0) {
            printf("tfi_start_server(): listen(): Error code %d\n", errno);
            tfi_close_server(tfi);
            return NULL;
        }
    }

	return tfi;
}

// accept new tcp connection (blocking)
// after this call, you can communicate through the client socket
tfi_client *tfi_accept(tfi_socket accept_socket) {
    tfi_client *tfi = (tfi_client *)malloc(sizeof(tfi_client));

    // accept new connection and fill the corresponding address structure
    socklen_t addrlen = sizeof(struct sockaddr);
    tfi->socket.s = accept(accept_socket.s, (struct sockaddr *)&tfi->address.a, &addrlen);
    if (tfi->socket.s < 0) {
        printf("tfi_accept(): accept(): Error code %d\n", errno);
        free(tfi);
        return NULL;
    }	

    return tfi;
}

// after this call, you can communicate through the client socket
tfi_client *tfi_start_client(char *host, int port, char *transport_protocol) {
    tfi_client *tfi = (tfi_client *)malloc(sizeof(tfi_client));
    int rv;

    // tcp or udp
    int type = tfi_get_connection_type(transport_protocol);
    if (type < 0) {
        free(tfi);
        return NULL;
    }

    // create socket
    switch (type) {
    case 0: // tcp
        tfi->socket.s = socket(AF_INET, SOCK_STREAM, 0);
        break;

    case 1: // udp
        tfi->socket.s = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (tfi->socket.s < 0) {
        printf("tfi_start_client(): socket(): Error code %d\n", errno);
        free(tfi);
        return NULL;
    }

    // fill server address structure
    memset(tfi->address.a.sin_zero, 0, 8);
	tfi->address.a.sin_family = AF_INET;
	tfi->address.a.sin_port = htons(port);
    inet_pton(AF_INET, host, &tfi->address.a.sin_addr);

    if (type == 0) { // tcp
        // connect to host
        rv = connect(tfi->socket.s, (struct sockaddr *)&tfi->address.a, sizeof(struct sockaddr));
        if (rv < 0) {
            printf("tfi_start_client(): connect(): Error code %d\n", errno);
            tfi_close_client(tfi);
            return NULL;
        }
    }

    return tfi;
}




// *** THREADS *** //

// internal thread function
void *thread_function(void *data) {
	tfi_client_thread *ct = (tfi_client_thread *)data;

	ct->function(ct->param);

    tfi_close_client(ct->param);
    free(ct);
	return 0;
}

// starts a new thread that calls 'function'
// after completion of 'function', the client and the client thread are automatically closed
tfi_client_thread *tfi_start_client_thread(tfi_client *client, void (*function)(tfi_client *)) {
    tfi_client_thread *ct = (tfi_client_thread *)malloc(sizeof(tfi_client_thread));

    ct->function = function;
    ct->param = client;
    int rv = pthread_create(&ct->thread, NULL, thread_function, ct);
    if (rv != 0) {
        printf("tfi_start_client_thread(): pthread_create(): Error code %d\n", rv);
        free(ct);
        return NULL;
    }

    return ct;
}

void tfi_wait_for_client_thread(tfi_client_thread *ct) {
    pthread_join(ct->thread, NULL);
}

// this is called automatically if the thread function completes
// however, this can be used manually to terminate the thread
void tfi_close_client_thread(tfi_client_thread *ct) {
    pthread_cancel(ct->thread);

    tfi_close_client(ct->param);
    free(ct);
}




// *** SEND & RECEIVE *** //

// tcp send entire message
// returns the number of bytes sent (which should always be msglen) or -1 if an error occurred
int tfi_send_all(tfi_socket socket, char *msg, int msglen) {
    int rv;

    int sent = 0;
    while (sent < msglen) {
        rv = send(socket.s, msg + sent, msglen - sent, 0);
        if (rv < 0) {
            printf("tfi_send_all(): send(): Error code %d\n", errno);
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
        if (rv < 0) {
            printf("tfi_recv_all(): recv(): Error code %d\n", errno);
            return -1;
        }
        if (rv == 0) {
            return 0;
        }

        received += rv;
    }

    return received;
}

// udp send entire message
// returns the number of bytes sent (which should always be msglen) or -1 if an error occurred
int tfi_sendto_all(tfi_socket socket, tfi_address addr, char *msg, int msglen) {
    int rv;

    int sent = 0;
    while (sent < msglen) {
        rv = sendto(socket.s, msg + sent, msglen - sent, 0, (struct sockaddr *)&addr.a, sizeof(addr));
        if (rv < 0) {
            printf("tfi_sendto_all(): sendto(): Error code %d\n", errno);
            return -1;
        }

        sent += rv;
    }

    return sent;
}

// udp receive entire message
// fills addr
// returns the number of bytes received (which should always be msglen) or 0 if the connection was closed gracefully or -1 if an error occurred
int tfi_recvfrom_all(tfi_socket socket, tfi_address *addr, char *buffer, int msglen) {
    int rv;
    socklen_t addrlen = sizeof(struct sockaddr);

    int received = 0;
    while (received < msglen) {
        rv = recvfrom(socket.s, buffer + received, msglen - received, 0, (struct sockaddr *)&addr->a, &addrlen);
        if (rv < 0) {
            printf("tfi_recvfrom_all(): recvfrom(): Error code %d\n", errno);
            return -1;
        }
        if (rv == 0) {
            return 0;
        }

        received += rv;
    }

    return received;
}

#endif

#endif