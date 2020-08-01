#ifndef TFINET_DEFINED_H
#define TFINET_DEFINED_H

#include <stdio.h>
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




// *** SOCKETS *** //

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
        WSACleanup();
        free(tfi);
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
        closesocket(tfi->socket.s);
        WSACleanup();
        free(tfi);
        return NULL;
    }

    if (type == 0) { // tcp
        // listen with accept socket
        rv = listen(tfi->socket.s, tcp_max_connections);
        if (rv == SOCKET_ERROR) {
            printf("tfi_start_server(): listen(): Error code %d\n", WSAGetLastError());
            closesocket(tfi->socket.s);
            WSACleanup();
            free(tfi);
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
        printf("tfi_connect(): WSAStartup(): Error code %d\n", rv);
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
        printf("tfi_connect(): socket(): Error code %d\n", WSAGetLastError());
        WSACleanup();
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
        rv = connect(tfi->socket.s, (SOCKADDR *)&tfi->address.a, sizeof(SOCKADDR));
        if (rv == SOCKET_ERROR) {
            printf("tfi_connect(): connect(): Error code %d\n", WSAGetLastError());
            closesocket(tfi->socket.s);
            WSACleanup();
            free(tfi);
            return NULL;
        }
    }

    return tfi;
}

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
        rv = sendto(socket.s, msg + sent, msglen - sent, 0, (SOCKADDR *)&addr, sizeof(addr));
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

#endif