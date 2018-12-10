#ifndef RPCSYNCWERK_NAMED_PIPE_TRANSPORT_H
#define RPCSYNCWERK_NAMED_PIPE_TRANSPORT_H

#include <pthread.h>
#include <glib.h>
#include <glib-object.h>
#include <jansson.h>

#if defined(WIN32)
#include <windows.h>
#endif

// Implementatin of a rpcsyncwerk transport based on named pipe. It uses unix domain
// sockets on linux/osx, and named pipes on windows.
//
// On the server side, there is a thread that listens for incoming connections,
// and it would create a new thread to handle each connection. Thus the RPC
// functions on the server side may be called from different threads, and it's
// the RPC functions implementation's responsibility to guarantee thread safety
// of the RPC calls. (e.g. using mutexes).

#if defined(WIN32)
typedef HANDLE RpcsyncwerkNamedPipe;
#else
typedef int RpcsyncwerkNamedPipe;
#endif

// Server side interface.

struct _RpcsyncwerkNamedPipeServer {
    char path[4096];
    pthread_t listener_thread;
    GList *handlers;
    RpcsyncwerkNamedPipe pipe_fd;
};

typedef struct _RpcsyncwerkNamedPipeServer RpcsyncwerkNamedPipeServer;

RpcsyncwerkNamedPipeServer* rpcsyncwerk_create_named_pipe_server(const char *path);

int rpcsyncwerk_named_pipe_server_start(RpcsyncwerkNamedPipeServer *server);

// Client side interface.

struct _RpcsyncwerkNamedPipeClient {
    char path[4096];
    RpcsyncwerkNamedPipe pipe_fd;
};

typedef struct _RpcsyncwerkNamedPipeClient RpcsyncwerkNamedPipeClient;

RpcsyncwerkNamedPipeClient* rpcsyncwerk_create_named_pipe_client(const char *path);

RpcsyncwerkClient * rpcsyncwerk_client_with_named_pipe_transport(RpcsyncwerkNamedPipeClient *client, const char *service);

int rpcsyncwerk_named_pipe_client_connect(RpcsyncwerkNamedPipeClient *client);

void rpcsyncwerk_free_client_with_pipe_transport (RpcsyncwerkClient *client);

#endif // RPCSYNCWERK_NAMED_PIPE_TRANSPORT_H
