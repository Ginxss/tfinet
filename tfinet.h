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

// after this call, you can call accept
tfi_server *tfi_start_server(int port, int max_connections) {
	tfi_server *tfi = (tfi_server *)malloc(sizeof(tfi_server));
	int rv;

    // start WinSock
    WSADATA wsa;
    rv = WSAStartup(MAKEWORD(2, 0), &wsa);
    if (rv != 0) {
        printf("tfi_start_server(): WSAStartup(): Error code %d\n", rv);
        free(tfi);
        return NULL;
    }

    // create accept socket
    tfi->socket.s = socket(AF_INET, SOCK_STREAM, 0);
    if (tfi->socket.s == INVALID_SOCKET) {
        printf("tfi_start_server(): socket(): Error code %d\n", WSAGetLastError());
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
        free(tfi);
        return NULL;
    }

    // listen with accept socket
    rv = listen(tfi->socket.s, max_connections);
    if (rv == SOCKET_ERROR) {
        printf("tfi_start_server(): listen(): Error code %d\n", WSAGetLastError());
        free(tfi);
        return NULL;
    }

	return tfi;
}

// accept new connection (blocking)
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
tfi_client *tfi_connect(char *host, int port) {
    tfi_client *tfi = (tfi_client *)malloc(sizeof(tfi_client));
    int rv;

    // start WinSock
    WSADATA wsa;
    rv = WSAStartup(MAKEWORD(2, 0), &wsa);
    if (rv != 0) {
        printf("tfi_connect(): WSAStartup(): Error code %d\n", rv);
        free(tfi);
        return NULL;
    }

    // create socket
    tfi->socket.s = socket(AF_INET, SOCK_STREAM, 0);
    if (tfi->socket.s == INVALID_SOCKET) {
        printf("tfi_connect(): socket(): Error code %d\n", WSAGetLastError());
        free(tfi);
        return NULL;
    }

    // fill server address structure
    memset(tfi->address.a.sin_zero, 0, 8);
	tfi->address.a.sin_family = AF_INET;
	tfi->address.a.sin_port = htons(port);
    inet_pton(AF_INET, host, &tfi->address.a.sin_addr);

    // connect to host
	rv = connect(tfi->socket.s, (SOCKADDR *)&tfi->address.a, sizeof(SOCKADDR));
    if (rv == SOCKET_ERROR) {
        printf("tfi_connect(): connect(): Error code %d\n", WSAGetLastError());
        free(tfi);
        return NULL;
    }

    return tfi;
}

void tfi_close_server(tfi_server *tfi) {
	closesocket(tfi->socket.s);
	free(tfi);
}

// Does not need to be called only if the client is handled in a thread
void tfi_close_client(tfi_client *tfi) {
	closesocket(tfi->socket.s);
	free(tfi);
}

void tfi_cleanup() {
    WSACleanup();
}

typedef struct {
    HANDLE thread;
	void (*function)(tfi_client *);
	tfi_client *param;
} tfi_client_thread;

DWORD WINAPI thread_function(void* data) {
	tfi_client_thread *ct = (tfi_client_thread *)data;

	ct->function(ct->param);

	CloseHandle(ct->thread);
    tfi_close_client(ct->param);
    free(ct);
	return 0;
}

// starts a new thread that calls 'function'
// After completion of 'function', the client and the client thread are automatically closed
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

// This is called automatically if the thread function completes
// However, this can be used manually to terminate the thread
void tfi_close_client_thread(tfi_client_thread *ct) {
    TerminateThread(ct->thread, 1);

    CloseHandle(ct->thread);
    tfi_close_client(ct->param);
    free(ct);
}

// returns the number of bytes sent (which should always be msglen)
// or -1 if an error occurred
int tfi_send_all(tfi_socket *socket, char *msg, int msglen) {
    int rv;

    int sent = 0;
    while (sent < msglen) {
        rv = send(socket->s, msg + sent, msglen - sent, 0);
        if (rv == SOCKET_ERROR) {
            printf("tfi_send_all(): send(): Error code %d\n", WSAGetLastError());
            return -1;
        }

        sent += rv;
    }

    return sent;
}

// returns the number of bytes received (which should always be msglen)
// or 0 if the connection was closed gracefully or -1 if an error occurred
int tfi_recv_all(tfi_socket *socket, char *buffer, int msglen) {
    int rv;

    int received = 0;
    while (received < msglen) {
        rv = recv(socket->s, buffer + received, msglen - received, 0);
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

#endif