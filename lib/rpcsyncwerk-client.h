#ifndef RPCSYNCWERK_CLIENT_H
#define RPCSYNCWERK_CLIENT_H

#include <glib.h>
#include <glib-object.h>
#include <jansson.h>

#ifndef DFT_DOMAIN
#define DFT_DOMAIN g_quark_from_string(G_LOG_DOMAIN)
#endif

typedef char *(*TransportCB)(void *arg, const gchar *fcall_str,
                             size_t fcall_len, size_t *ret_len);

/**
 * @rpc_priv is used by the rpc_client to store information related to
 * this rpc call.
 * @fcall_str is an allocated string, and the sender should free it
 * when not needed.
 */
typedef int (*AsyncTransportSend)(void *arg, gchar *fcall_str,
                                  size_t fcall_len, void *rpc_priv);

typedef void (*AsyncCallback) (void *result, void *user_data, GError *error);

struct _RpcsyncwerkClient {
    TransportCB send;
    void *arg;
    
    AsyncTransportSend async_send;
    void *async_arg;
};

typedef struct _RpcsyncwerkClient RpcsyncwerkClient;

RpcsyncwerkClient *rpcsyncwerk_client_new ();

void rpcsyncwerk_client_free (RpcsyncwerkClient *client);

void
rpcsyncwerk_client_call (RpcsyncwerkClient *client, const char *fname,
                    const char *ret_type, GType gobject_type,
                    void *ret_ptr, GError **error,
                    int n_params, ...);

int
rpcsyncwerk_client_call__int (RpcsyncwerkClient *client, const char *fname,
                         GError **error, int n_params, ...);

gint64
rpcsyncwerk_client_call__int64 (RpcsyncwerkClient *client, const char *fname,
                           GError **error, int n_params, ...);

char *
rpcsyncwerk_client_call__string (RpcsyncwerkClient *client, const char *fname,
                            GError **error, int n_params, ...);

GObject *
rpcsyncwerk_client_call__object (RpcsyncwerkClient *client, const char *fname,
                            GType object_type,
                            GError **error, int n_params, ...);

GList*
rpcsyncwerk_client_call__objlist (RpcsyncwerkClient *client, const char *fname,
                             GType object_type,
                             GError **error, int n_params, ...);

json_t *
rpcsyncwerk_client_call__json (RpcsyncwerkClient *client, const char *fname,
                          GError **error, int n_params, ...);


char* rpcsyncwerk_client_transport_send (RpcsyncwerkClient *client,
                                    const gchar *fcall_str,
                                    size_t fcall_len,
                                    size_t *ret_len);



int
rpcsyncwerk_client_async_call__int (RpcsyncwerkClient *client,
                               const char *fname,
                               AsyncCallback callback, void *cbdata,
                               int n_params, ...);

int
rpcsyncwerk_client_async_call__int64 (RpcsyncwerkClient *client,
                                 const char *fname,
                                 AsyncCallback callback, void *cbdata,
                                 int n_params, ...);

int
rpcsyncwerk_client_async_call__string (RpcsyncwerkClient *client,
                                  const char *fname,
                                  AsyncCallback callback, void *cbdata,
                                  int n_params, ...);

int
rpcsyncwerk_client_async_call__object (RpcsyncwerkClient *client,
                                  const char *fname,
                                  AsyncCallback callback, 
                                  GType object_type, void *cbdata,
                                  int n_params, ...);

int
rpcsyncwerk_client_async_call__objlist (RpcsyncwerkClient *client,
                                   const char *fname,
                                   AsyncCallback callback, 
                                   GType object_type, void *cbdata,
                                   int n_params, ...);

int
rpcsyncwerk_client_async_call__json (RpcsyncwerkClient *client,
                                const char *fname,
                                AsyncCallback callback, void *cbdata,
                                int n_params, ...);


/* called by the transport layer, the rpc layer should be able to
 * modify the str, but not take ownership of it */
int
rpcsyncwerk_client_generic_callback (char *retstr, size_t len,
                                void *vdata, const char *errstr);


/* in case of transport error, the following code and message will be
 * set in GError */
#define TRANSPORT_ERROR  "Transport Error"
#define TRANSPORT_ERROR_CODE 500


#endif
