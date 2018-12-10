#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#if !defined(WIN32)
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/un.h>
  #include <unistd.h>
#endif // !defined(WIN32)

#include <glib.h>
#include <glib/gstdio.h>
#include <jansson.h>

#include "rpcsyncwerk-utils.h"
#include "rpcsyncwerk-client.h"
#include "rpcsyncwerk-server.h"
#include "rpcsyncwerk-named-pipe-transport.h"

#if defined(WIN32)
static const int kPipeBufSize = 1024;
static char* formatErrorMessage();

#define G_WARNING_WITH_LAST_ERROR(fmt)                        \
    do {                                                      \
        char *error_msg__ = formatErrorMessage();             \
        g_warning(fmt ": %s\n", error_msg__);                 \
        g_free (error_msg__);                                 \
    } while(0);

#endif // defined(WIN32)

static void* named_pipe_listen(void *arg);
static void* named_pipe_client_handler(void *arg);
static char* rpcsyncwerk_named_pipe_send(void *arg, const gchar *fcall_str, size_t fcall_len, size_t *ret_len);

static char * request_to_json(const char *service, const char *fcall_str, size_t fcall_len);
static int request_from_json (const char *content, size_t len, char **service, char **fcall_str);
static void json_object_set_string_member (json_t *object, const char *key, const char *value);
static const char * json_object_get_string_member (json_t *object, const char *key);

static ssize_t pipe_write_n(RpcsyncwerkNamedPipe fd, const void *vptr, size_t n);
static ssize_t pipe_read_n(RpcsyncwerkNamedPipe fd, void *vptr, size_t n);

typedef struct {
    RpcsyncwerkNamedPipeClient* client;
    char *service;
} ClientTransportData;

RpcsyncwerkClient*
rpcsyncwerk_client_with_named_pipe_transport(RpcsyncwerkNamedPipeClient *pipe_client,
                                        const char *service)
{
    RpcsyncwerkClient *client= rpcsyncwerk_client_new();
    client->send = rpcsyncwerk_named_pipe_send;

    ClientTransportData *data = g_malloc(sizeof(ClientTransportData));
    data->client = pipe_client;
    data->service = g_strdup(service);

    client->arg = data;
    return client;
}

RpcsyncwerkNamedPipeClient* rpcsyncwerk_create_named_pipe_client(const char *path)
{
    RpcsyncwerkNamedPipeClient *client = g_malloc0(sizeof(RpcsyncwerkNamedPipeClient));
    memcpy(client->path, path, strlen(path) + 1);
    return client;
}

RpcsyncwerkNamedPipeServer* rpcsyncwerk_create_named_pipe_server(const char *path)
{
    RpcsyncwerkNamedPipeServer *server = g_malloc0(sizeof(RpcsyncwerkNamedPipeServer));
    memcpy(server->path, path, strlen(path) + 1);
    return server;
}

int rpcsyncwerk_named_pipe_server_start(RpcsyncwerkNamedPipeServer *server)
{
#if !defined(WIN32)
    int pipe_fd = socket (AF_UNIX, SOCK_STREAM, 0);
    const char *un_path = server->path;
    if (pipe_fd < 0) {
        g_warning ("Failed to create unix socket fd : %s\n",
                   strerror(errno));
        return -1;
    }

    struct sockaddr_un saddr;
    saddr.sun_family = AF_UNIX;

    if (strlen(server->path) > sizeof(saddr.sun_path)-1) {
        g_warning ("Unix socket path %s is too long."
                       "Please set or modify UNIX_SOCKET option in ccnet.conf.\n",
                       un_path);
        goto failed;
    }

    if (g_file_test (un_path, G_FILE_TEST_EXISTS)) {
        g_debug ("socket file exists, delete it anyway\n");
        if (g_unlink (un_path) < 0) {
            g_warning ("delete socket file failed : %s\n", strerror(errno));
            goto failed;
        }
    }

    g_strlcpy (saddr.sun_path, un_path, sizeof(saddr.sun_path));
    if (bind(pipe_fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        g_warning ("failed to bind unix socket fd to %s : %s\n",
                      un_path, strerror(errno));
        goto failed;
    }

    if (listen(pipe_fd, 10) < 0) {
        g_warning ("failed to listen to unix socket: %s\n", strerror(errno));
        goto failed;
    }

    if (chmod(un_path, 0700) < 0) {
        g_warning ("failed to set permisson for unix socket %s: %s\n",
                      un_path, strerror(errno));
        goto failed;
    }

    server->pipe_fd = pipe_fd;

#endif // !defined(WIN32)

    /* TODO: use glib thread pool */
    pthread_create(&server->listener_thread, NULL, named_pipe_listen, server);
    return 0;

#if !defined(WIN32)
failed:
    close(pipe_fd);
    return -1;
#endif
}

typedef struct {
    RpcsyncwerkNamedPipeServer *server;
    RpcsyncwerkNamedPipe connfd;
} ServerHandlerData;

static void* named_pipe_listen(void *arg)
{
    RpcsyncwerkNamedPipeServer *server = arg;
#if !defined(WIN32)
    while (1) {
        int connfd = accept (server->pipe_fd, NULL, 0);
        pthread_t *handler = g_malloc(sizeof(pthread_t));
        ServerHandlerData *data = g_malloc(sizeof(ServerHandlerData));
        data->server = server;
        data->connfd = connfd;
        // TODO(low priority): Instead of using a thread to handle each client,
        // use select(unix)/iocp(windows) to do it.
        pthread_create(handler, NULL, named_pipe_client_handler, data);
        server->handlers = g_list_append(server->handlers, handler);
    }

#else // !defined(WIN32)
    while (1) {
        HANDLE connfd = INVALID_HANDLE_VALUE;
        BOOL connected = FALSE;

        connfd = CreateNamedPipe(
            server->path,             // pipe name
            PIPE_ACCESS_DUPLEX,       // read/write access
            PIPE_TYPE_MESSAGE |       // message type pipe
            PIPE_READMODE_MESSAGE |   // message-read mode
            PIPE_WAIT,                // blocking mode
            PIPE_UNLIMITED_INSTANCES, // max. instances
            kPipeBufSize,             // output buffer size
            kPipeBufSize,             // input buffer size
            0,                        // client time-out
            NULL);                    // default security attribute

        if (connfd == INVALID_HANDLE_VALUE) {
            G_WARNING_WITH_LAST_ERROR ("Failed to create named pipe");
            break;
        }

        /* listening on this pipe */
        connected = ConnectNamedPipe(connfd, NULL) ?
            TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (!connected) {
            G_WARNING_WITH_LAST_ERROR ("failed to ConnectNamedPipe()");
            CloseHandle(connfd);
            break;
        }

        g_debug ("Accepted a named pipe client\n");

        pthread_t *handler = g_malloc(sizeof(pthread_t));
        ServerHandlerData *data = g_malloc(sizeof(ServerHandlerData));
        data->server = server;
        data->connfd = connfd;
        // TODO(low priority): Instead of using a thread to handle each client,
        // use select(unix)/iocp(windows) to do it.
        pthread_create(handler, NULL, named_pipe_client_handler, data);
        server->handlers = g_list_append(server->handlers, handler);
    }
#endif // !defined(WIN32)
    return NULL;
}

static void* named_pipe_client_handler(void *arg)
{
    ServerHandlerData *data = arg;
    // RpcsyncwerkNamedPipeServer *server = data->server;
    RpcsyncwerkNamedPipe connfd = data->connfd;

    guint32 len;
    guint32 bufsize = 4096;
    char *buf = g_malloc(bufsize);

    g_debug ("start to serve on pipe client\n");

    while (1) {
        len = 0;
        if (pipe_read_n(connfd, &len, sizeof(guint32)) < 0) {
            g_warning("failed to read rpc request size: %s", strerror(errno));
            break;
        }

        if (len == 0) {
            g_debug("EOF reached, pipe connection lost");
            break;
        }

        while (bufsize < len) {
            bufsize *= 2;
            buf = realloc(buf, bufsize);
        }

        if (pipe_read_n(connfd, buf, len) < 0 || len == 0) {
            g_warning("failed to read rpc request: %s", strerror(errno));
            g_free (buf);
            break;
        }

        char *service, *body;
        if (request_from_json (buf, len, &service, &body) < 0) {
            break;
        }

        gsize ret_len;
        char *ret_str = rpcsyncwerk_server_call_function (service, body, strlen(body), &ret_len);
        g_free (service);
        g_free (body);

        len = (guint32)ret_len;
        if (pipe_write_n(connfd, &len, sizeof(guint32)) < 0) {
            g_warning("failed to send rpc response(%s): %s", ret_str, strerror(errno));
            g_free (ret_str);
            break;
        }

        if (pipe_write_n(connfd, ret_str, ret_len) < 0) {
            g_warning("failed to send rpc response: %s", strerror(errno));
            g_free (ret_str);
            break;
        }

        g_free (ret_str);
    }

#if !defined(WIN32)
    close(connfd);
#else // !defined(WIN32)
    DisconnectNamedPipe(connfd);
    CloseHandle(connfd);
#endif // !defined(WIN32)

    return NULL;
}


int rpcsyncwerk_named_pipe_client_connect(RpcsyncwerkNamedPipeClient *client)
{
#if !defined(WIN32)
    client->pipe_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un servaddr;
    servaddr.sun_family = AF_UNIX;

    g_strlcpy (servaddr.sun_path, client->path, sizeof(servaddr.sun_path));
    if (connect(client->pipe_fd, (struct sockaddr *)&servaddr, (socklen_t)sizeof(servaddr)) < 0) {
        g_warning ("pipe client failed to connect to server: %s\n", strerror(errno));
        return -1;
    }

#else // !defined(WIN32)
    RpcsyncwerkNamedPipe pipe_fd;
    pipe_fd = CreateFile(
        client->path,           // pipe name
        GENERIC_READ |          // read and write access
        GENERIC_WRITE,
        0,                      // no sharing
        NULL,                   // default security attributes
        OPEN_EXISTING,          // opens existing pipe
        0,                      // default attributes
        NULL);                  // no template file

    if (pipe_fd == INVALID_HANDLE_VALUE) {
        G_WARNING_WITH_LAST_ERROR("Failed to connect to named pipe");
        return -1;
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe_fd, &mode, NULL, NULL)) {
        G_WARNING_WITH_LAST_ERROR("Failed to set named pipe mode");
        return -1;
    }

    client->pipe_fd = pipe_fd;

#endif // !defined(WIN32)

    g_debug ("pipe client connected to server\n");
    return 0;
}

void rpcsyncwerk_free_client_with_pipe_transport (RpcsyncwerkClient *client)
{
    ClientTransportData *data = (ClientTransportData *)(client->arg);
    RpcsyncwerkNamedPipeClient *pipe_client = data->client;
#if defined(WIN32)
    CloseHandle(pipe_client->pipe_fd);
#else
    close(pipe_client->pipe_fd);
#endif
    g_free (pipe_client);
    g_free (data);
    rpcsyncwerk_client_free (client);
}

char *rpcsyncwerk_named_pipe_send(void *arg, const gchar *fcall_str,
                             size_t fcall_len, size_t *ret_len)
{
    g_debug ("rpcsyncwerk_named_pipe_send is called\n");
    ClientTransportData *data = arg;
    RpcsyncwerkNamedPipeClient *client = data->client;

    char *json_str = request_to_json(data->service, fcall_str, fcall_len);
    guint32 len = (guint32)strlen(json_str);

    if (pipe_write_n(client->pipe_fd, &len, sizeof(guint32)) < 0) {
        g_warning("failed to send rpc call: %s", strerror(errno));
        free (json_str);
        return NULL;
    }

    if (pipe_write_n(client->pipe_fd, json_str, len) < 0) {
        g_warning("failed to send rpc call: %s", strerror(errno));
        free (json_str);
        return NULL;
    }

    free (json_str);

    if (pipe_read_n(client->pipe_fd, &len, sizeof(guint32)) < 0) {
        g_warning("failed to read rpc response: %s", strerror(errno));
        return NULL;
    }

    char *buf = g_malloc(len);

    if (pipe_read_n(client->pipe_fd, buf, len) < 0) {
        g_warning("failed to read rpc response: %s", strerror(errno));
        g_free (buf);
        return NULL;
    }

    *ret_len = len;
    return buf;
}

static char *
request_to_json (const char *service, const char *fcall_str, size_t fcall_len)
{
    json_t *object = json_object ();

    char *temp_request = g_malloc0(fcall_len + 1);
    memcpy(temp_request, fcall_str, fcall_len);

    json_object_set_string_member (object, "service", service);
    json_object_set_string_member (object, "request", temp_request);

    g_free (temp_request);

    char *str = json_dumps (object, 0);
    json_decref (object);
    return str;
}

static int
request_from_json (const char *content, size_t len, char **service, char **fcall_str)
{
    json_error_t jerror;
    json_t *object = json_loadb(content, len, 0, &jerror);
    if (!object) {
        g_warning ("Failed to parse request body: %s.\n", strlen(jerror.text) > 0 ? jerror.text : "");
        return -1;
    }

    *service = g_strdup(json_object_get_string_member (object, "service"));
    *fcall_str = g_strdup(json_object_get_string_member(object, "request"));

    json_decref (object);

    if (!*service || !*fcall_str) {
        g_free (*service);
        g_free (*fcall_str);
        return -1;
    }

    return 0;
}

static void json_object_set_string_member (json_t *object, const char *key, const char *value)
{
    json_object_set_new (object, key, json_string (value));
}

static const char *
json_object_get_string_member (json_t *object, const char *key)
{
    json_t *string = json_object_get (object, key);
    if (!string)
        return NULL;
    return json_string_value (string);
}

#if !defined(WIN32)

// Write "n" bytes to a descriptor.
ssize_t
pipe_write_n(int fd, const void *vptr, size_t n)
{
    size_t      nleft;
    ssize_t     nwritten;
    const char  *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;       /* and call write() again */
            else
                return(-1);         /* error */
        }

        nleft -= nwritten;
        ptr   += nwritten;
    }
    return(n);
}

// Read "n" bytes from a descriptor.
ssize_t
pipe_read_n(int fd, void *vptr, size_t n)
{
    size_t  nleft;
    ssize_t nread;
    char    *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;      /* and call read() again */
            else
                return(-1);
        } else if (nread == 0)
            break;              /* EOF */

        nleft -= nread;
        ptr   += nread;
    }
    return(n - nleft);      /* return >= 0 */
}

#else // !defined(WIN32)

ssize_t pipe_read_n (RpcsyncwerkNamedPipe fd, void *vptr, size_t n)
{
    DWORD bytes_read;
    BOOL success = ReadFile(
        fd,                     // handle to pipe
        vptr,                   // buffer to receive data
        (DWORD)n,               // size of buffer
        &bytes_read,            // number of bytes read
        NULL);                  // not overlapped I/O

    if (!success || bytes_read != (DWORD)n) {
        if (GetLastError() == ERROR_BROKEN_PIPE) {
            return 0;
        }
        G_WARNING_WITH_LAST_ERROR("failed to read from pipe");
        return -1;
    }

    return n;
}

ssize_t pipe_write_n(RpcsyncwerkNamedPipe fd, const void *vptr, size_t n)
{
    DWORD bytes_written;
    BOOL success = WriteFile(
        fd,                     // handle to pipe
        vptr,                   // buffer to receive data
        (DWORD)n,               // size of buffer
        &bytes_written,         // number of bytes written
        NULL);                  // not overlapped I/O

    if (!success || bytes_written != (DWORD)n) {
        G_WARNING_WITH_LAST_ERROR("failed to read command from the pipe");
    }

    FlushFileBuffers(fd);
    return 0;
}

static char *locale_to_utf8 (const gchar *src)
{
    if (!src)
        return NULL;

    gsize bytes_read = 0;
    gsize bytes_written = 0;
    GError *error = NULL;
    gchar *dst = NULL;

    dst = g_locale_to_utf8
        (src,                   /* locale specific string */
         strlen(src),           /* len of src */
         &bytes_read,           /* length processed */
         &bytes_written,        /* output length */
         &error);

    if (error) {
        return NULL;
    }

    return dst;
}


// http://stackoverflow.com/questions/3006229/get-a-text-from-the-error-code-returns-from-the-getlasterror-function
// The caller is responsible to free the returned message.
char* formatErrorMessage()
{
    DWORD error_code = GetLastError();
    if (error_code == 0) {
        return g_strdup("no error");
    }
    char buf[256] = {0};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                   NULL,
                   error_code,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buf,
                   sizeof(buf) - 1,
                   NULL);
    return locale_to_utf8(buf);
}

#endif // !defined(WIN32)
