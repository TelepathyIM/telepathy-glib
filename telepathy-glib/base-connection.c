/*
 * base-connection.c - Source for TpBaseConnection
 *
 * Copyright (C) 2005, 2006, 2007 Collabora Ltd.
 * Copyright (C) 2005, 2006, 2007 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * SECTION:base-connection
 * @title: TpBaseConnection
 * @short_description: base class for #TpSvcConnection implementations
 * @see_also: #TpBaseConnectionManager, #TpSvcConnection
 *
 * This base class makes it easier to write #TpSvcConnection implementations
 * by managing connection status, channel factories and handle tracking.
 * A subclass should often not need to implement any of the Connection
 * methods itself.
 *
 * However, methods may be reimplemented if needed: for instance, Gabble
 * overrides RequestHandles so it can validate MUC rooms, which must be done
 * asynchronously.
 */

#include <telepathy-glib/base-connection.h>

#include <string.h>

#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/contacts-mixin.h>
#include <telepathy-glib/channel-factory-iface.h>
#include <telepathy-glib/channel-manager.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/exportable-channel.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CONNECTION
#include "debug-internal.h"

static void service_iface_init (gpointer, gpointer);
static void requests_iface_init (gpointer, gpointer);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE(TpBaseConnection,
    tp_base_connection,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION,
      service_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
      tp_dbus_properties_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_REQUESTS,
      requests_iface_init));

enum
{
    PROP_PROTOCOL = 1,
    PROP_SELF_HANDLE,
};

/* signal enum */
enum
{
    INVALID_SIGNAL,
    SHUTDOWN_FINISHED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

#define TP_BASE_CONNECTION_GET_PRIVATE(obj) \
    ((TpBaseConnectionPrivate *)obj->priv)

typedef struct _ChannelRequest ChannelRequest;

typedef enum {
    METHOD_REQUEST_CHANNEL,
    METHOD_CREATE_CHANNEL,
#if 0
    METHOD_ENSURE_CHANNEL,
#endif
    NUM_METHODS
} ChannelRequestMethod;

struct _ChannelRequest
{
  DBusGMethodInvocation *context;
  ChannelRequestMethod method;

  /* relevant if the method is METHOD_REQUEST_CHANNEL */
  gchar *channel_type;
  guint handle_type;
  guint handle;
  /* always TRUE for CREATE */
  gboolean suppress_handler;
};

static ChannelRequest *
channel_request_new (DBusGMethodInvocation *context,
                     ChannelRequestMethod method,
                     const char *channel_type,
                     guint handle_type,
                     guint handle,
                     gboolean suppress_handler)
{
  ChannelRequest *ret;

  g_assert (NULL != context);
  g_assert (NULL != channel_type);
  g_assert (method < NUM_METHODS);

  ret = g_slice_new0 (ChannelRequest);
  ret->context = context;
  ret->method = method;
  ret->channel_type = g_strdup (channel_type);
  ret->handle_type = handle_type;
  ret->handle = handle;
  ret->suppress_handler = suppress_handler;

  DEBUG("New channel request at %p: ctype=%s htype=%d handle=%d suppress=%d",
        ret, channel_type, handle_type, handle, suppress_handler);

  return ret;
}

static void
channel_request_free (ChannelRequest *request)
{
  g_assert (NULL == request->context);
  DEBUG("Freeing channel request at %p: ctype=%s htype=%d handle=%d "
        "suppress=%d", request, request->channel_type, request->handle_type,
        request->handle, request->suppress_handler);
  g_free (request->channel_type);
  g_slice_free (ChannelRequest, request);
}

static void
channel_request_cancel (gpointer data, gpointer user_data)
{
  ChannelRequest *request = (ChannelRequest *) data;
  GError error = { TP_ERRORS, TP_ERROR_DISCONNECTED,
      "unable to service this channel request, we're disconnecting!" };

  DEBUG ("cancelling request at %p for %s/%u/%u", request,
      request->channel_type, request->handle_type, request->handle);

  dbus_g_method_return_error (request->context, &error);
  request->context = NULL;

  channel_request_free (request);
}

struct _TpBaseConnectionPrivate
{
  /* Telepathy properties */
  gchar *protocol;

  /* if TRUE, the object has gone away */
  gboolean dispose_has_run;
  /* array of (TpChannelFactoryIface *) */
  GPtrArray *channel_factories;
  /* array of (TpChannelManager *) */
  GPtrArray *channel_managers;
  /* array of (ChannelRequest *) */
  GPtrArray *channel_requests;

  TpHandleRepoIface *handles[NUM_TP_HANDLE_TYPES];

  /* If not %NULL, contains strings representing our interfaces.
   * If %NULL, we have no interfaces except those in
   * klass->interfaces_always_present (i.e. this is lazily allocated).
   *
   * Note that this is a GArray of gchar*, not a GPtrArray,
   * so that we can use GArray's convenient auto-null-termination. */
  GArray *interfaces;

  /* Array of DBusGMethodInvocation * representing Disconnect calls.
   * If NULL and we are in a state != DISCONNECTED, then we have not started
   * shutting down yet.
   * If NULL and we are in state DISCONNECTED, then we have finished shutting
   * down.
   * If not NULL, we are trying to shut down (and must be in state
   * DISCONNECTED). */
  GPtrArray *disconnect_requests;
};

static void
tp_base_connection_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
  TpBaseConnection *self = (TpBaseConnection *) object;
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (self);

  switch (property_id) {
    case PROP_PROTOCOL:
      g_value_set_string (value, priv->protocol);
      break;

    case PROP_SELF_HANDLE:
      g_value_set_uint (value, self->self_handle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_base_connection_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  TpBaseConnection *self = (TpBaseConnection *) object;
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (self);

  switch (property_id) {
    case PROP_PROTOCOL:
      g_free (priv->protocol);
      priv->protocol = g_value_dup_string (value);
      g_assert (priv->protocol != NULL);
      break;

    case PROP_SELF_HANDLE:
      {
        TpHandle new_self_handle = g_value_get_uint (value);

        if (self->status == TP_CONNECTION_STATUS_CONNECTED)
          g_return_if_fail (new_self_handle != 0);

        if (self->self_handle == new_self_handle)
          return;

        if (self->self_handle != 0)
          tp_handle_unref (priv->handles[TP_HANDLE_TYPE_CONTACT],
              self->self_handle);

        self->self_handle = new_self_handle;

        if (self->self_handle != 0)
          tp_handle_ref (priv->handles[TP_HANDLE_TYPE_CONTACT],
              self->self_handle);

        tp_svc_connection_emit_self_handle_changed (self, self->self_handle);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_base_connection_dispose (GObject *object)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (object);
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (self);
  DBusGProxy *bus_proxy = tp_get_bus_proxy ();
  guint i;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  g_assert ((self->status == TP_CONNECTION_STATUS_DISCONNECTED) ||
            (self->status == TP_INTERNAL_CONNECTION_STATUS_NEW));
  g_assert (self->self_handle == 0);

  if (NULL != self->bus_name)
    {
      dbus_g_proxy_call_no_reply (bus_proxy, "ReleaseName",
                                  G_TYPE_STRING, self->bus_name,
                                  G_TYPE_INVALID);
    }

  g_ptr_array_foreach (priv->channel_factories, (GFunc) g_object_unref, NULL);
  g_ptr_array_free (priv->channel_factories, TRUE);
  priv->channel_factories = NULL;

  g_ptr_array_foreach (priv->channel_managers, (GFunc) g_object_unref, NULL);
  g_ptr_array_free (priv->channel_managers, TRUE);
  priv->channel_managers = NULL;

  if (priv->channel_requests)
    {
      g_assert (priv->channel_requests->len == 0);
      g_ptr_array_free (priv->channel_requests, TRUE);
      priv->channel_requests = NULL;
    }

  for (i = 0; i < NUM_TP_HANDLE_TYPES; i++)
    {
      if (priv->handles[i])
        {
          g_object_unref ((GObject *)priv->handles[i]);
          priv->handles[i] = NULL;
        }
    }

  if (priv->interfaces)
    {
      g_array_free (priv->interfaces, TRUE);
    }

  if (G_OBJECT_CLASS (tp_base_connection_parent_class)->dispose)
    G_OBJECT_CLASS (tp_base_connection_parent_class)->dispose (object);
}

static void
tp_base_connection_finalize (GObject *object)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (object);
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (self);

  g_free (priv->protocol);
  g_free (self->bus_name);
  g_free (self->object_path);

  G_OBJECT_CLASS (tp_base_connection_parent_class)->finalize (object);
}


/**
 * exportable_channel_get_old_info:
 * @channel: a channel
 * @object_path_out:  address at which to store the channel's object path
 * @channel_type_out: address at which to store the channel's type
 * @handle_type_out:  address at which to store the channel's associated handle
 *                    type
 * @handle_out:       address at which to store the channel's associated
 *                    handle, if any
 *
 * Given a new-style exportable channel, as used by the Requests interface's
 * API, fetches the information needed for the old-style ListChannels method
 * on Connections.
 */
static void
exportable_channel_get_old_info (TpExportableChannel *channel,
                                 gchar **object_path_out,
                                 gchar **channel_type_out,
                                 guint *handle_type_out,
                                 guint *handle_out)
{
  gchar *object_path;
  GHashTable *channel_properties;
  gboolean valid;

  g_object_get (channel,
      "object-path", &object_path,
      "channel-properties", &channel_properties,
      NULL);

  g_assert (object_path != NULL);
  g_assert (tp_dbus_check_valid_object_path (object_path, NULL));

  if (object_path_out != NULL)
    *object_path_out = object_path;
  else
    g_free (object_path);

  if (channel_type_out != NULL)
    {
      *channel_type_out = g_strdup (tp_asv_get_string (channel_properties,
          TP_IFACE_CHANNEL ".ChannelType"));
      g_assert (*channel_type_out != NULL);
      g_assert (tp_dbus_check_valid_interface_name (*channel_type_out, NULL));
    }

  if (handle_type_out != NULL)
    {
      *handle_type_out = tp_asv_get_uint32 (channel_properties,
          TP_IFACE_CHANNEL ".TargetHandleType", &valid);
      g_assert (valid);
    }

  if (handle_out != NULL)
    {
      *handle_out = tp_asv_get_uint32 (channel_properties,
          TP_IFACE_CHANNEL ".TargetHandle", &valid);
      g_assert (valid);

      if (handle_type_out != NULL)
        {
          if (*handle_type_out == TP_HANDLE_TYPE_NONE)
            g_assert (*handle_out == 0);
          else
            g_assert (*handle_out != 0);
        }
    }

  g_hash_table_destroy (channel_properties);
}


static GValueArray *
get_channel_details (GObject *obj)
{
  GValueArray *structure = g_value_array_new (1);
  GHashTable *table;
  GValue *value;
  gchar *object_path;

  g_object_get (obj,
      "object-path", &object_path,
      NULL);

  g_value_array_append (structure, NULL);
  value = g_value_array_get_nth (structure, 0);
  g_value_init (value, DBUS_TYPE_G_OBJECT_PATH);
  g_value_take_boxed (value, object_path);
  object_path = NULL;

  g_assert (TP_IS_EXPORTABLE_CHANNEL (obj) || TP_IS_CHANNEL_IFACE (obj));

  if (TP_IS_EXPORTABLE_CHANNEL (obj))
    {
      g_object_get (obj,
          "channel-properties", &table,
          NULL);
    }
  else
    {
     table = g_hash_table_new_full (g_str_hash, g_str_equal,
          NULL, (GDestroyNotify) tp_g_value_slice_free);

      value = tp_g_value_slice_new (G_TYPE_UINT);
      g_object_get_property (obj, "handle", value);
      g_hash_table_insert (table, TP_IFACE_CHANNEL ".TargetHandle", value);

      value = tp_g_value_slice_new (G_TYPE_UINT);
      g_object_get_property (obj, "handle-type", value);
      g_hash_table_insert (table, TP_IFACE_CHANNEL ".TargetHandleType", value);

      value = tp_g_value_slice_new (G_TYPE_STRING);
      g_object_get_property (obj, "channel-type", value);
      g_hash_table_insert (table, TP_IFACE_CHANNEL ".ChannelType", value);
    }

  g_value_array_append (structure, NULL);
  value = g_value_array_get_nth (structure, 1);
  g_value_init (value, TP_HASH_TYPE_STRING_VARIANT_MAP);
  g_value_take_boxed (value, table);

  return structure;
}


static GPtrArray *
find_matching_channel_requests (TpBaseConnection *conn,
                                const gchar *channel_type,
                                guint handle_type,
                                guint handle,
                                ChannelRequest *channel_request,
                                gboolean *suppress_handler)
{
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (conn);
  GPtrArray *requests;
  guint i;

  requests = g_ptr_array_sized_new (1);

  if (handle_type == 0)
    {
      /* It's an anonymous channel, which can only satisfy the request for
       * which it was created (or if it's returned as EXISTING, it can only
       * satisfy the request for which it was returned as EXISTING).
       */
      g_assert (handle == 0);
      g_assert (channel_request == NULL ||
          tp_g_ptr_array_contains (priv->channel_requests, channel_request));

      if (channel_request)
        {
          g_ptr_array_add (requests, channel_request);

          if (suppress_handler && channel_request->suppress_handler)
            *suppress_handler = TRUE;
        }

      /* whether we've put any matches in requests or not */
      return requests;
    }

  /* for identifiable channels (those which are to a particular handle),
   * satisfy any queued requests.
   */
  for (i = 0; i < priv->channel_requests->len; i++)
    {
      ChannelRequest *request = g_ptr_array_index (priv->channel_requests, i);

      if (tp_strdiff (request->channel_type, channel_type))
        continue;

      if (handle_type != request->handle_type)
        continue;

      if (handle != request->handle)
        continue;

      if (request->suppress_handler && suppress_handler)
        *suppress_handler = TRUE;

      g_ptr_array_add (requests, request);
    }

  /* if this channel was created or returned as a result of a particular
   * request, that request had better be among the matching ones in the queue
   */
  g_assert (channel_request == NULL ||
      tp_g_ptr_array_contains (requests, channel_request));

  return requests;
}


static void
satisfy_request (TpBaseConnection *conn,
                 ChannelRequest *request,
                 GObject *channel,
                 const gchar *object_path)
{
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (conn);
  DEBUG ("completing queued request %p with success, "
      "channel_type=%s, handle_type=%u, "
      "handle=%u, suppress_handler=%u", request, request->channel_type,
      request->handle_type, request->handle, request->suppress_handler);

  switch (request->method)
    {
    case METHOD_REQUEST_CHANNEL:
      tp_svc_connection_return_from_request_channel (request->context,
          object_path);
      break;

    case METHOD_CREATE_CHANNEL:
        {
          GHashTable *properties;

          g_assert (TP_IS_EXPORTABLE_CHANNEL (channel));
          g_object_get (channel,
              "channel-properties", &properties,
              NULL);
          tp_svc_connection_interface_requests_return_from_create_channel (
              request->context, object_path, properties);
          g_hash_table_destroy (properties);
        }
        break;

    default:
      g_assert_not_reached ();
    }
  request->context = NULL;

  g_ptr_array_remove (priv->channel_requests, request);

  channel_request_free (request);
}


static void
satisfy_requests (TpBaseConnection *conn,
                  TpChannelFactoryIface *factory,
                  TpChannelIface *chan,
                  ChannelRequest *channel_request,
                  gboolean is_new)
{
  gchar *object_path = NULL, *channel_type = NULL;
  guint handle_type = 0, handle = 0;
  gboolean suppress_handler = FALSE;
  GPtrArray *tmp;
  guint i;

  g_object_get (chan,
      "object-path", &object_path,
      "channel-type", &channel_type,
      "handle-type", &handle_type,
      "handle", &handle,
      NULL);

  DEBUG ("called for %s", object_path);

  tmp = find_matching_channel_requests (conn, channel_type, handle_type,
                                        handle, channel_request,
                                        &suppress_handler);

  if (is_new)
    {
      GPtrArray *array = g_ptr_array_sized_new (1);

      tp_svc_connection_emit_new_channel (conn, object_path, channel_type,
          handle_type, handle, suppress_handler);

      g_ptr_array_add (array, get_channel_details (G_OBJECT (chan)));
      tp_svc_connection_interface_requests_emit_new_channels (conn, array);
      g_value_array_free (g_ptr_array_index (array, 0));
      g_ptr_array_free (array, TRUE);
    }

  for (i = 0; i < tmp->len; i++)
    satisfy_request (conn, g_ptr_array_index (tmp, i), G_OBJECT (chan),
        object_path);

  g_ptr_array_free (tmp, TRUE);

  g_free (object_path);
  g_free (channel_type);
}


/* Channel factory signal handlers */

static void
channel_closed_cb (GObject *channel,
                   TpBaseConnection *conn)
{
  gchar *object_path;

  g_object_get (channel,
      "object-path", &object_path,
      NULL);

  tp_svc_connection_interface_requests_emit_channel_closed (conn,
      object_path);

  g_free (object_path);
}

static void
connection_new_channel_cb (TpChannelFactoryIface *factory,
                           GObject *chan,
                           ChannelRequest *channel_request,
                           gpointer data)
{
  satisfy_requests (TP_BASE_CONNECTION (data), factory,
      TP_CHANNEL_IFACE (chan), channel_request, TRUE);

  g_signal_connect (chan, "closed", (GCallback) channel_closed_cb, data);
}


static void
fail_channel_request (TpBaseConnection *conn,
                      ChannelRequest *request,
                      GError *error)
{
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (conn);
  DEBUG ("completing queued request %p with error, channel_type=%s, "
      "handle_type=%u, handle=%u, suppress_handler=%u",
      request, request->channel_type,
      request->handle_type, request->handle, request->suppress_handler);

  dbus_g_method_return_error (request->context, error);
  request->context = NULL;

  g_ptr_array_remove (priv->channel_requests, request);

  channel_request_free (request);
}


static void
connection_channel_error_cb (TpChannelFactoryIface *factory,
                             GObject *chan,
                             GError *error,
                             ChannelRequest *channel_request,
                             gpointer data)
{
  TpBaseConnection *conn = TP_BASE_CONNECTION (data);
  gchar *channel_type = NULL;
  guint handle_type = 0, handle = 0;
  GPtrArray *tmp;
  guint i;

  DEBUG ("channel_type=%s, handle_type=%u, handle=%u, error_code=%u, "
      "error_message=\"%s\"", channel_type, handle_type, handle,
      error->code, error->message);

  g_object_get (chan,
      "channel-type", &channel_type,
      "handle-type", &handle_type,
      "handle", &handle,
      NULL);

  tmp = find_matching_channel_requests (conn, channel_type, handle_type,
                                        handle, channel_request, NULL);

  for (i = 0; i < tmp->len; i++)
    fail_channel_request (conn, g_ptr_array_index (tmp, i), error);

  g_ptr_array_free (tmp, TRUE);
  g_free (channel_type);
}


/* Channel manager signal handlers */

static void
manager_new_channel (gpointer key,
                     gpointer value,
                     gpointer data)
{
  TpExportableChannel *channel = TP_EXPORTABLE_CHANNEL (key);
  GSList *request_tokens = value;
  TpBaseConnection *self = TP_BASE_CONNECTION (data);
  gchar *object_path, *channel_type;
  guint handle_type, handle;
  GSList *iter;
  gboolean suppress_handler = FALSE;

  exportable_channel_get_old_info (channel, &object_path, &channel_type,
      &handle_type, &handle);

  for (iter = request_tokens; iter != NULL; iter = iter->next)
    {
      ChannelRequest *request = iter->data;

      if (request->suppress_handler)
        {
          suppress_handler = TRUE;
          break;
        }
    }

  tp_svc_connection_emit_new_channel (self, object_path, channel_type,
      handle_type, handle, suppress_handler);

  for (iter = request_tokens; iter != NULL; iter = iter->next)
    {
      satisfy_request (self, iter->data, G_OBJECT (channel),
          object_path);
    }

  g_free (object_path);
  g_free (channel_type);
}


static void
manager_new_channels_foreach (gpointer key,
                              gpointer value,
                              gpointer data)
{
  GPtrArray *details = data;

  g_ptr_array_add (details, get_channel_details (G_OBJECT (key)));
}


static void
manager_new_channels_cb (TpChannelManager *manager,
                         GHashTable *channels,
                         TpBaseConnection *self)
{
  GPtrArray *array;

  g_assert (TP_IS_CHANNEL_MANAGER (manager));
  g_assert (TP_IS_BASE_CONNECTION (self));

  array = g_ptr_array_sized_new (g_hash_table_size (channels));
  g_hash_table_foreach (channels, manager_new_channels_foreach, array);
  tp_svc_connection_interface_requests_emit_new_channels (self,
      array);
  g_ptr_array_foreach (array, (GFunc) g_value_array_free, NULL);
  g_ptr_array_free (array, TRUE);

  g_hash_table_foreach (channels, manager_new_channel, self);
}


static void
manager_request_already_satisfied_cb (TpChannelManager *manager,
                                      gpointer request_token,
                                      TpExportableChannel *channel,
                                      TpBaseConnection *self)
{
  gchar *object_path;

  g_assert (TP_IS_CHANNEL_MANAGER (manager));
  g_assert (TP_IS_EXPORTABLE_CHANNEL (channel));
  g_assert (TP_IS_BASE_CONNECTION (self));

  g_object_get (channel,
      "object-path", &object_path,
      NULL);

  satisfy_request (self, request_token, G_OBJECT (channel), object_path);
  g_free (object_path);
}


static void
manager_request_failed_cb (TpChannelManager *manager,
                           gpointer request_token,
                           guint domain,
                           gint code,
                           gchar *message,
                           TpBaseConnection *self)
{
  GError error = { domain, code, message };

  g_assert (TP_IS_CHANNEL_MANAGER (manager));
  g_assert (domain > 0);
  g_assert (message != NULL);
  g_assert (TP_IS_BASE_CONNECTION (self));

  fail_channel_request (self, request_token, &error);
}


static void
manager_channel_closed_cb (TpChannelManager *manager,
                           const gchar *path,
                           TpBaseConnection *self)
{
  g_assert (TP_IS_CHANNEL_MANAGER (manager));
  g_assert (path != NULL);
  g_assert (TP_IS_BASE_CONNECTION (self));

  tp_svc_connection_interface_requests_emit_channel_closed (self, path);
}


static GObject *
tp_base_connection_constructor (GType type, guint n_construct_properties,
    GObjectConstructParam *construct_params)
{
  guint i;
  TpBaseConnection *self = TP_BASE_CONNECTION (
      G_OBJECT_CLASS (tp_base_connection_parent_class)->constructor (
        type, n_construct_properties, construct_params));
  TpBaseConnectionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CONNECTION, TpBaseConnectionPrivate);
  TpBaseConnectionClass *cls = TP_BASE_CONNECTION_GET_CLASS (self);

  DEBUG("Post-construction: (TpBaseConnection *)%p", self);

  g_assert (cls->create_handle_repos != NULL);
  g_assert (cls->create_channel_factories != NULL ||
            cls->create_channel_managers  != NULL);
  g_assert (cls->shut_down != NULL);
  g_assert (cls->start_connecting != NULL);

  (cls->create_handle_repos) (self, priv->handles);

  /* a connection that doesn't support contacts is no use to anyone */
  g_assert (priv->handles[TP_HANDLE_TYPE_CONTACT] != NULL);

  if (DEBUGGING)
    {
      for (i = 0; i < NUM_TP_HANDLE_TYPES; i++)
      {
        DEBUG("Handle repo for type #%u at %p", i, priv->handles[i]);
      }
    }

  if (cls->create_channel_factories != NULL)
    priv->channel_factories = cls->create_channel_factories (self);
  else
    priv->channel_factories = g_ptr_array_sized_new (0);

  if (cls->create_channel_managers != NULL)
    priv->channel_managers = cls->create_channel_managers (self);
  else
    priv->channel_managers = g_ptr_array_sized_new (0);

  for (i = 0; i < priv->channel_factories->len; i++)
    {
      GObject *factory = g_ptr_array_index (priv->channel_factories, i);
      DEBUG("Channel factory #%u at %p", i, factory);
      g_signal_connect (factory, "new-channel", G_CALLBACK
          (connection_new_channel_cb), self);
      g_signal_connect (factory, "channel-error", G_CALLBACK
          (connection_channel_error_cb), self);
    }

  for (i = 0; i < priv->channel_managers->len; i++)
    {
      TpChannelManager *manager = TP_CHANNEL_MANAGER (
          g_ptr_array_index (priv->channel_managers, i));

      DEBUG("Channel manager #%u at %p", i, manager);

      g_signal_connect (manager, "new-channels",
          (GCallback) manager_new_channels_cb, self);
      g_signal_connect (manager, "request-already-satisfied",
          (GCallback) manager_request_already_satisfied_cb, self);
      g_signal_connect (manager, "request-failed",
          (GCallback) manager_request_failed_cb, self);
      g_signal_connect (manager, "channel-closed",
          (GCallback) manager_channel_closed_cb, self);
    }

  return (GObject *)self;
}


/* D-Bus properties for the Requests interface */

static void
factory_get_channel_details_foreach (TpChannelIface *chan,
                                     gpointer data)
{
  GPtrArray *details = data;

  g_ptr_array_add (details, get_channel_details (G_OBJECT (chan)));
}


static void
manager_get_channel_details_foreach (TpExportableChannel *chan,
                                     gpointer data)
{
  GPtrArray *details = data;

  g_ptr_array_add (details, get_channel_details (G_OBJECT (chan)));
}


static GPtrArray *
conn_requests_get_channel_details (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (self);
  /* guess that each ChannelManager and each ChannelFactory has two
   * channels, on average */
  GPtrArray *details = g_ptr_array_sized_new (priv->channel_managers->len * 2
      + priv->channel_factories->len * 2);
  guint i;

  for (i = 0; i < priv->channel_factories->len; i++)
    {
      TpChannelFactoryIface *factory = TP_CHANNEL_FACTORY_IFACE (
          g_ptr_array_index (priv->channel_factories, i));

      tp_channel_factory_iface_foreach (factory,
          factory_get_channel_details_foreach, details);
    }

  for (i = 0; i < priv->channel_managers->len; i++)
    {
      TpChannelManager *manager = TP_CHANNEL_MANAGER (
          g_ptr_array_index (priv->channel_managers, i));

      tp_channel_manager_foreach_channel (manager,
          manager_get_channel_details_foreach, details);
    }

  return details;
}


static void
get_requestables_foreach (TpChannelManager *manager,
                          GHashTable *fixed_properties,
                          const gchar * const *allowed_properties,
                          gpointer user_data)
{
  GPtrArray *details = user_data;
  GValueArray *requestable = g_value_array_new (2);
  GValue *value;

  g_value_array_append (requestable, NULL);
  value = g_value_array_get_nth (requestable, 0);
  g_value_init (value, TP_HASH_TYPE_CHANNEL_CLASS);
  g_value_set_boxed (value, fixed_properties);

  g_value_array_append (requestable, NULL);
  value = g_value_array_get_nth (requestable, 1);
  g_value_init (value, G_TYPE_STRV);
  g_value_set_boxed (value, allowed_properties);

  g_ptr_array_add (details, requestable);
}


static GPtrArray *
conn_requests_get_requestables (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (self);
  /* generously guess that each ChannelManager has about 2 ChannelClasses */
  GPtrArray *details = g_ptr_array_sized_new (priv->channel_managers->len * 2);
  guint i;

  for (i = 0; i < priv->channel_managers->len; i++)
    {
      TpChannelManager *manager = TP_CHANNEL_MANAGER (
          g_ptr_array_index (priv->channel_managers, i));

      tp_channel_manager_foreach_channel_class (manager,
          get_requestables_foreach, details);
    }

  return details;
}


static void
conn_requests_get_dbus_property (GObject *object,
                                 GQuark interface,
                                 GQuark name,
                                 GValue *value,
                                 gpointer unused G_GNUC_UNUSED)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (object);

  g_return_if_fail (interface == TP_IFACE_QUARK_CONNECTION_INTERFACE_REQUESTS);

  if (name == g_quark_from_static_string ("Channels"))
    {
      g_value_take_boxed (value, conn_requests_get_channel_details (self));
    }
  else if (name == g_quark_from_static_string ("RequestableChannelClasses"))
    {
      g_value_take_boxed (value, conn_requests_get_requestables (self));
    }
  else
    {
      g_return_if_reached ();
    }
}


static void
tp_base_connection_class_init (TpBaseConnectionClass *klass)
{
  static TpDBusPropertiesMixinPropImpl connection_properties[] = {
      { "SelfHandle", "self-handle", NULL },
      { NULL }
  };
  static TpDBusPropertiesMixinPropImpl requests_properties[] = {
        { "Channels", NULL, NULL },
        { "RequestableChannelClasses", NULL, NULL },
        { NULL }
  };
  GParamSpec *param_spec;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  DEBUG("Initializing (TpBaseConnectionClass *)%p", klass);

  g_type_class_add_private (klass, sizeof (TpBaseConnectionPrivate));
  object_class->dispose = tp_base_connection_dispose;
  object_class->finalize = tp_base_connection_finalize;
  object_class->constructor = tp_base_connection_constructor;
  object_class->get_property = tp_base_connection_get_property;
  object_class->set_property = tp_base_connection_set_property;

  /**
   * TpBaseConnection:protocol:
   *
   * Identifier used in the Telepathy protocol when this connection's protocol
   * name is required.
   */
  param_spec = g_param_spec_string ("protocol",
      "Telepathy identifier for protocol",
      "Identifier string used when the protocol name is required.",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_PROTOCOL, param_spec);

  /**
   * TpBaseConnection:self-handle:
   *
   * The handle of type %TP_HANDLE_TYPE_CONTACT representing the local user.
   * Must be set nonzero by the subclass before moving to state CONNECTED.
   *
   * Since: 0.7.UNRELEASED
   */
  param_spec = g_param_spec_uint ("self-handle",
      "Connection.SelfHandle",
      "The handle of type %TP_HANDLE_TYPE_CONTACT representing the local user.",
      0, G_MAXUINT, 0,
      G_PARAM_READWRITE |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_SELF_HANDLE, param_spec);

  /* signal definitions */

  /**
   * TpBaseConnection::shutdown-finished:
   *
   * Emitted by tp_base_connection_finish_shutdown() when the underlying
   * network connection has been closed; #TpBaseConnectionManager listens
   * for this signal and removes connections from its table of active
   * connections when it is received.
   */
  signals[SHUTDOWN_FINISHED] =
    g_signal_new ("shutdown-finished",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  tp_dbus_properties_mixin_class_init (object_class, 0);
  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CONNECTION,
      tp_dbus_properties_mixin_getter_gobject_properties, NULL,
      connection_properties);
  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CONNECTION_INTERFACE_REQUESTS,
      conn_requests_get_dbus_property, NULL,
      requests_properties);
}

static void
tp_base_connection_init (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CONNECTION, TpBaseConnectionPrivate);
  guint i;

  DEBUG("Initializing (TpBaseConnection *)%p", self);

  self->priv = priv;

  self->status = TP_INTERNAL_CONNECTION_STATUS_NEW;

  for (i = 0; i < NUM_TP_HANDLE_TYPES; i++)
    {
      priv->handles[i] = NULL;
    }

  priv->channel_requests = g_ptr_array_new ();
}

/**
 * tp_base_connection_register:
 * @self: A connection
 * @cm_name: The name of the connection manager in the Telepathy protocol
 * @bus_name: Used to return the bus name corresponding to the connection
 *  if %TRUE is returned; must not be %NULL. To be freed by the caller.
 * @object_path: Used to return the object path of the connection if
 *  %TRUE is returned; must not be %NULL. To be freed by the caller.
 * @error: Used to return an error if %FALSE is returned; may be %NULL
 *
 * Make the connection object appear on the bus, returning the bus
 * name and object path used. If %TRUE is returned, the connection owns the
 * bus name, and will release it when destroyed.
 *
 * Returns: %TRUE on success, %FALSE on error.
 */
gboolean
tp_base_connection_register (TpBaseConnection *self,
                             const gchar *cm_name,
                             gchar **bus_name,
                             gchar **object_path,
                             GError **error)
{
  TpBaseConnectionClass *cls = TP_BASE_CONNECTION_GET_CLASS (self);
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (self);
  DBusGConnection *bus;
  DBusGProxy *bus_proxy;
  gchar *tmp;
  gchar *safe_proto;
  gchar *unique_name;
  guint request_name_result;
  GError *request_error = NULL;

  if (tp_connection_manager_check_valid_protocol_name (priv->protocol, NULL))
    {
      safe_proto = g_strdelimit (g_strdup (priv->protocol), "-", '_');
    }
  else
    {
      g_warning ("Protocol name %s is not valid - should match "
          "[A-Za-z][A-Za-z0-9-]+", priv->protocol);
      safe_proto = tp_escape_as_identifier (priv->protocol);
    }

  if (cls->get_unique_connection_name)
    {
      tmp = cls->get_unique_connection_name (self);
      g_assert (tmp != NULL);
      unique_name = tp_escape_as_identifier (tmp);
      g_free (tmp);
    }
  else
    {
      unique_name = g_strdup_printf ("_%p", self);
    }

  bus = tp_get_bus ();
  bus_proxy = tp_get_bus_proxy ();

  self->bus_name = g_strdup_printf (TP_CONN_BUS_NAME_BASE "%s.%s.%s",
      cm_name, safe_proto, unique_name);
  self->object_path = g_strdup_printf (TP_CONN_OBJECT_PATH_BASE "%s/%s/%s",
      cm_name, safe_proto, unique_name);

  g_free (safe_proto);
  g_free (unique_name);

  if (!dbus_g_proxy_call (bus_proxy, "RequestName", &request_error,
                          G_TYPE_STRING, self->bus_name,
                          G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
                          G_TYPE_INVALID,
                          G_TYPE_UINT, &request_name_result,
                          G_TYPE_INVALID))
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "Error acquiring bus name %s: %s", self->bus_name,
          request_error->message);

      g_error_free (request_error);

      g_free (self->bus_name);
      self->bus_name = NULL;

      return FALSE;
    }

  if (request_name_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
      gchar *msg;

      switch (request_name_result)
        {
        case DBUS_REQUEST_NAME_REPLY_IN_QUEUE:
          msg = "Request has been queued, though we request non-queueing.";
          break;
        case DBUS_REQUEST_NAME_REPLY_EXISTS:
          msg = "A connection manger already has this busname.";
          break;
        case DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER:
          msg = "Connection manager already has a connection to this account.";
          break;
        default:
          msg = "Unknown error return from ReleaseName";
        }

      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "Error acquiring bus name %s: %s", self->bus_name, msg);

      g_free (self->bus_name);
      self->bus_name = NULL;

      return FALSE;
    }

  DEBUG ("bus name %s", self->bus_name);

  dbus_g_connection_register_g_object (bus, self->object_path,
      G_OBJECT (self));

  DEBUG ("object path %s", self->object_path);

  *bus_name = g_strdup (self->bus_name);
  *object_path = g_strdup (self->object_path);

  return TRUE;
}

static void
tp_base_connection_close_all_channels (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CONNECTION, TpBaseConnectionPrivate);

  /* We deliberately don't iterate over channel managers here -
   * they don't need this, and are expected to listen to status-changed
   * for themselves.
   */

  /* trigger close_all on all channel factories */
  g_ptr_array_foreach (priv->channel_factories, (GFunc)
      tp_channel_factory_iface_close_all, NULL);

  /* cancel all queued channel requests */
  if (priv->channel_requests->len > 0)
    {
      g_ptr_array_foreach (priv->channel_requests, (GFunc)
        channel_request_cancel, NULL);
      g_ptr_array_remove_range (priv->channel_requests, 0,
        priv->channel_requests->len);
    }
}

/* D-Bus methods on Connection interface ----------------------------*/

static inline TpConnectionStatusReason
conn_status_reason_from_g_error (GError *error)
{
  if (error->domain == TP_ERRORS)
    {
      switch (error->code)
        {
        case TP_ERROR_NETWORK_ERROR:
          return TP_CONNECTION_STATUS_REASON_NETWORK_ERROR;
        case TP_ERROR_PERMISSION_DENIED:
          return TP_CONNECTION_STATUS_REASON_AUTHENTICATION_FAILED;
        }
    }

  return TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED;
}

/**
 * tp_base_connection_connect:
 *
 * @context: Used to return the result onto D-Bus.
 *
 * Implements D-Bus method Connect
 * on interface org.freedesktop.Telepathy.Connection
 */
static void
tp_base_connection_connect (TpSvcConnection *iface,
                            DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (iface);
  TpBaseConnectionClass *cls = TP_BASE_CONNECTION_GET_CLASS (self);
  GError *error = NULL;

  g_assert (TP_IS_BASE_CONNECTION (self));

  if (self->status == TP_INTERNAL_CONNECTION_STATUS_NEW)
    {
      if (cls->start_connecting (self, &error))
        {
          if (self->status == TP_INTERNAL_CONNECTION_STATUS_NEW)
            {
              tp_base_connection_change_status (self,
                TP_CONNECTION_STATUS_CONNECTING,
                TP_CONNECTION_STATUS_REASON_REQUESTED);
            }
        }
      else
        {
          if (self->status != TP_CONNECTION_STATUS_DISCONNECTED)
            {
              tp_base_connection_change_status (self,
                TP_CONNECTION_STATUS_DISCONNECTED,
                conn_status_reason_from_g_error (error));
            }
          dbus_g_method_return_error (context, error);
          g_error_free (error);
          return;
        }
    }
  tp_svc_connection_return_from_connect (context);
}


/**
 * tp_base_connection_disconnect
 *
 * Implements D-Bus method Disconnect
 * on interface org.freedesktop.Telepathy.Connection
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occurred, D-Bus will throw the error only if this
 *         function returns FALSE.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
static void
tp_base_connection_disconnect (TpSvcConnection *iface,
                               DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (iface);

  g_assert (TP_IS_BASE_CONNECTION (self));

  if (self->priv->disconnect_requests != NULL)
    {
      g_assert (self->status == TP_CONNECTION_STATUS_DISCONNECTED);
      g_ptr_array_add (self->priv->disconnect_requests, context);
      return;
    }

  if (self->status == TP_CONNECTION_STATUS_DISCONNECTED)
    {
      /* status DISCONNECTED and disconnect_requests NULL => already dead */
      tp_svc_connection_return_from_disconnect (context);
      return;
    }

  self->priv->disconnect_requests = g_ptr_array_sized_new (1);
  g_ptr_array_add (self->priv->disconnect_requests, context);

  tp_base_connection_change_status (self,
      TP_CONNECTION_STATUS_DISCONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);
}

/**
 * tp_base_connection_get_interfaces
 *
 * Implements D-Bus method GetInterfaces
 * on interface org.freedesktop.Telepathy.Connection
 */
static void
tp_base_connection_get_interfaces (TpSvcConnection *iface,
                                   DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (iface);
  TpBaseConnectionPrivate *priv;
  TpBaseConnectionClass *klass;
  const gchar **interfaces;

  g_assert (TP_IS_BASE_CONNECTION (self));

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (self, context);

  priv = TP_BASE_CONNECTION_GET_PRIVATE (self);
  klass = TP_BASE_CONNECTION_GET_CLASS (self);

  if (priv->interfaces)
    {
      /* There are some extra interfaces for this channel */
      interfaces = (const gchar **)(priv->interfaces->data);
    }
  else
    {
      /* We only have the interfaces that are always present.
       * Instead of bothering to duplicate the static
       * array into the GArray, we just use it directly */
      interfaces = klass->interfaces_always_present;
    }

  tp_svc_connection_return_from_get_interfaces (context, interfaces);
}

/**
 * tp_base_connection_get_protocol
 *
 * Implements D-Bus method GetProtocol
 * on interface org.freedesktop.Telepathy.Connection
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occurred, D-Bus will throw the error only if this
 *         function returns FALSE.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
static void
tp_base_connection_get_protocol (TpSvcConnection *iface,
                                 DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (iface);
  TpBaseConnectionPrivate *priv;

  g_assert (TP_IS_BASE_CONNECTION (self));

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (self, context);

  priv = TP_BASE_CONNECTION_GET_PRIVATE (self);

  tp_svc_connection_return_from_get_protocol (context, priv->protocol);
}

/**
 * tp_base_connection_dbus_get_self_handle
 *
 * Implements D-Bus method GetSelfHandle
 * on interface org.freedesktop.Telepathy.Connection
 */
static void
tp_base_connection_dbus_get_self_handle (TpSvcConnection *iface,
                                         DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (iface);

  g_assert (TP_IS_BASE_CONNECTION (self));

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (self, context);

  tp_svc_connection_return_from_get_self_handle (
      context, self->self_handle);
}

/**
 * tp_base_connection_get_status
 *
 * Implements D-Bus method GetStatus
 * on interface org.freedesktop.Telepathy.Connection
 */
static void
tp_base_connection_get_status (TpSvcConnection *iface,
                               DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (iface);

  if (self->status == TP_INTERNAL_CONNECTION_STATUS_NEW)
    {
      tp_svc_connection_return_from_get_status (
          context, TP_CONNECTION_STATUS_DISCONNECTED);
    }
  else
    {
      tp_svc_connection_return_from_get_status (
          context, self->status);
    }
}


/**
 * tp_base_connection_hold_handles
 *
 * Implements D-Bus method HoldHandles
 * on interface org.freedesktop.Telepathy.Connection
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_base_connection_hold_handles (TpSvcConnection *iface,
                                 guint handle_type,
                                 const GArray *handles,
                                 DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (iface);
  TpBaseConnectionPrivate *priv;
  GError *error = NULL;
  gchar *sender;

  g_assert (TP_IS_BASE_CONNECTION (self));

  priv = TP_BASE_CONNECTION_GET_PRIVATE (self);

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (self, context);

  if (!tp_handles_supported_and_valid (priv->handles,
        handle_type, handles, FALSE, &error))
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  sender = dbus_g_method_get_sender (context);
  if (!tp_handles_client_hold (priv->handles[handle_type], sender,
        handles, &error))
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      g_free (sender);
      return;
    }

  g_free (sender);

  tp_svc_connection_return_from_hold_handles (context);
}

/**
 * tp_base_connection_inspect_handles
 *
 * Implements D-Bus method InspectHandles
 * on interface org.freedesktop.Telepathy.Connection
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
static void
tp_base_connection_inspect_handles (TpSvcConnection *iface,
                                    guint handle_type,
                                    const GArray *handles,
                                    DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (iface);
  TpBaseConnectionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CONNECTION, TpBaseConnectionPrivate);
  GError *error = NULL;
  const gchar **ret;
  guint i;

  g_assert (TP_IS_BASE_CONNECTION (self));

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (self, context);

  if (!tp_handles_supported_and_valid (priv->handles,
        handle_type, handles, FALSE, &error))
    {
      dbus_g_method_return_error (context, error);

      g_error_free (error);

      return;
    }

  ret = g_new (const gchar *, handles->len + 1);

  for (i = 0; i < handles->len; i++)
    {
      TpHandle handle;
      const gchar *tmp;

      handle = g_array_index (handles, TpHandle, i);
      tmp = tp_handle_inspect (priv->handles[handle_type], handle);
      g_assert (tmp != NULL);

      ret[i] = tmp;
    }

  ret[i] = NULL;

  tp_svc_connection_return_from_inspect_handles (context, ret);

  g_free (ret);
}

/**
 * list_channel_factory_foreach_one:
 * @chan: a channel
 * @data: a GPtrArray in which channel information should be stored
 *
 * Called by the exported ListChannels function for each channel in a channel
 * factory, this should add to the GPtrArray (in the data pointer) a
 * GValueArray containing the following:
 *  a D-Bus object path for the channel object on this service
 *  a D-Bus interface name representing the channel type
 *  an integer representing the handle type this channel communicates with,
 *    or zero
 *  an integer handle representing the contact, room or list this channel
 *    communicates with, or zero
 */
static void
list_channel_factory_foreach_one (TpChannelIface *chan,
                                  gpointer data)
{
  GObject *channel = G_OBJECT (chan);
  GPtrArray *values = (GPtrArray *) data;
  gchar *path, *type;
  guint handle_type, handle;
  GValue *entry = tp_dbus_specialized_value_slice_new
      (TP_STRUCT_TYPE_CHANNEL_INFO);

  g_object_get (channel,
      "object-path", &path,
      "channel-type", &type,
      "handle-type", &handle_type,
      "handle", &handle,
      NULL);

  dbus_g_type_struct_set (entry,
      0, path,
      1, type,
      2, handle_type,
      3, handle,
      G_MAXUINT);

  g_ptr_array_add (values, entry);

  g_free (path);
  g_free (type);
}


/**
 * list_channel_manager_foreach_one:
 * @chan: a channel
 * @data: a GPtrArray in which channel information should be stored
 *
 * Called by the exported ListChannels function for each channel in a channel
 * manager, this should add to the GPtrArray (in the data pointer) a
 * GValueArray containing the following:
 *  a D-Bus object path for the channel object on this service
 *  a D-Bus interface name representing the channel type
 *  an integer representing the handle type this channel communicates with,
 *    or zero
 *  an integer handle representing the contact, room or list this channel
 *    communicates with, or zero
 */
static void
list_channel_manager_foreach_one (TpExportableChannel *channel,
                                  gpointer data)
{
  GPtrArray *values = (GPtrArray *) data;
  gchar *path, *type;
  guint handle_type, handle;
  GValue *entry = tp_dbus_specialized_value_slice_new
      (TP_STRUCT_TYPE_CHANNEL_INFO);

  g_assert (TP_IS_EXPORTABLE_CHANNEL (channel));

  exportable_channel_get_old_info (channel, &path, &type, &handle_type,
      &handle);

  dbus_g_type_struct_set (entry,
      0, path,
      1, type,
      2, handle_type,
      3, handle,
      G_MAXUINT);

  g_ptr_array_add (values, entry);

  g_free (path);
  g_free (type);
}


static void
tp_base_connection_list_channels (TpSvcConnection *iface,
                                  DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (iface);
  TpBaseConnectionPrivate *priv;
  GPtrArray *channels, *values;
  guint i;

  g_assert (TP_IS_BASE_CONNECTION (self));

  priv = TP_BASE_CONNECTION_GET_PRIVATE (self);

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (self, context);

  /* I think on average, each factory will have 2 channels :D */
  values = g_ptr_array_sized_new (priv->channel_factories->len * 2
      + priv->channel_managers->len * 2);

  for (i = 0; i < priv->channel_factories->len; i++)
    {
      TpChannelFactoryIface *factory = g_ptr_array_index
        (priv->channel_factories, i);

      tp_channel_factory_iface_foreach (factory,
          list_channel_factory_foreach_one, values);
    }

  for (i = 0; i < priv->channel_managers->len; i++)
    {
      TpChannelManager *manager = g_ptr_array_index
        (priv->channel_managers, i);

      tp_channel_manager_foreach_channel (manager,
          list_channel_manager_foreach_one, values);
    }

  channels = g_ptr_array_sized_new (values->len);

  for (i = 0; i < values->len; i++)
    {
      g_ptr_array_add (channels, g_value_get_boxed (g_ptr_array_index
            (values, i)));
    }

  tp_svc_connection_return_from_list_channels (context, channels);

  g_ptr_array_free (channels, TRUE);
  for (i = 0; i < values->len; i++)
    {
      tp_g_value_slice_free (g_ptr_array_index (values, i));
    }
  g_ptr_array_free (values, TRUE);
}


/**
 * tp_base_connection_request_channel
 *
 * Implements D-Bus method RequestChannel
 * on interface org.freedesktop.Telepathy.Connection
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_base_connection_request_channel (TpSvcConnection *iface,
                                    const gchar *type,
                                    guint handle_type,
                                    guint handle,
                                    gboolean suppress_handler,
                                    DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (iface);
  TpBaseConnectionPrivate *priv;
  TpChannelFactoryRequestStatus status =
    TP_CHANNEL_FACTORY_REQUEST_STATUS_NOT_IMPLEMENTED;
  GError *error = NULL;
  guint i;
  ChannelRequest *request;
  GHashTable *request_properties;
  GValue *v;

  g_assert (TP_IS_BASE_CONNECTION (self));

  priv = TP_BASE_CONNECTION_GET_PRIVATE (self);

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (self, context);

  request = channel_request_new (context, METHOD_REQUEST_CHANNEL,
      type, handle_type, handle, suppress_handler);
  g_ptr_array_add (priv->channel_requests, request);

  /* First try the channel managers */

  request_properties = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) tp_g_value_slice_free);

  v = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (v, type);
  g_hash_table_insert (request_properties, TP_IFACE_CHANNEL ".ChannelType", v);

  v = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (v, handle_type);
  g_hash_table_insert (request_properties,
      TP_IFACE_CHANNEL ".TargetHandleType", v);

  v = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (v, handle);
  g_hash_table_insert (request_properties,
      TP_IFACE_CHANNEL ".TargetHandle", v);

  for (i = 0; i < priv->channel_managers->len; i++)
    {
      TpChannelManager *manager = TP_CHANNEL_MANAGER (
          g_ptr_array_index (priv->channel_managers, i));

      if (tp_channel_manager_request_channel (manager, request,
            request_properties))
        return;
    }

  g_hash_table_destroy (request_properties);

  /* OK, none of them wanted it. Now try the channel factories */

  for (i = 0; i < priv->channel_factories->len; i++)
    {
      TpChannelFactoryIface *factory = g_ptr_array_index
        (priv->channel_factories, i);
      TpChannelFactoryRequestStatus cur_status;
      TpChannelIface *chan = NULL;

      cur_status = tp_channel_factory_iface_request (factory, type,
          (TpHandleType) handle_type, handle, request, &chan, &error);

      switch (cur_status)
        {
        case TP_CHANNEL_FACTORY_REQUEST_STATUS_EXISTING:
          {
            g_assert (NULL != chan);
            satisfy_requests (self, factory, chan, request, FALSE);
            /* satisfy_requests should remove the request */
            g_assert (!tp_g_ptr_array_contains (priv->channel_requests,
                  request));
            return;
          }
        case TP_CHANNEL_FACTORY_REQUEST_STATUS_CREATED:
          g_assert (NULL != chan);
          /* the signal handler should have completed the queued request
           * and freed the ChannelRequest already */
          g_assert (!tp_g_ptr_array_contains (priv->channel_requests,
                request));
          return;
        case TP_CHANNEL_FACTORY_REQUEST_STATUS_QUEUED:
          DEBUG ("queued request, channel_type=%s, handle_type=%u, "
              "handle=%u, suppress_handler=%u", type, handle_type,
              handle, suppress_handler);
          return;
        case TP_CHANNEL_FACTORY_REQUEST_STATUS_ERROR:
          /* pass through error */
          goto ERROR;
        default:
          /* always return the most specific error */
          if (cur_status > status)
            status = cur_status;
        }
    }

  switch (status)
    {
      case TP_CHANNEL_FACTORY_REQUEST_STATUS_INVALID_HANDLE:
        DEBUG ("invalid handle %u", handle);

        error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_HANDLE,
                             "invalid handle %u", handle);

        break;

      case TP_CHANNEL_FACTORY_REQUEST_STATUS_NOT_AVAILABLE:
        DEBUG ("requested channel is unavailable with "
                 "handle type %u", handle_type);

        error = g_error_new (TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
                             "requested channel is not available with "
                             "handle type %u", handle_type);

        break;

      case TP_CHANNEL_FACTORY_REQUEST_STATUS_NOT_IMPLEMENTED:
        DEBUG ("unsupported channel type %s", type);

        error = g_error_new (TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
                             "unsupported channel type %s", type);

        break;

      default:
        g_assert_not_reached ();
    }

ERROR:
  g_assert (error != NULL);
  dbus_g_method_return_error (request->context, error);
  request->context = NULL;
  g_error_free (error);

  g_ptr_array_remove (priv->channel_requests, request);
  channel_request_free (request);
}


/**
 * tp_base_connection_release_handles
 *
 * Implements D-Bus method ReleaseHandles
 * on interface org.freedesktop.Telepathy.Connection
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
static void
tp_base_connection_release_handles (TpSvcConnection *iface,
                                    guint handle_type,
                                    const GArray * handles,
                                    DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (iface);
  TpBaseConnectionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CONNECTION, TpBaseConnectionPrivate);
  char *sender;
  GError *error = NULL;

  g_assert (TP_IS_BASE_CONNECTION (self));

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (self, context);

  if (!tp_handles_supported_and_valid (priv->handles,
        handle_type, handles, FALSE, &error))
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  sender = dbus_g_method_get_sender (context);
  if (!tp_handles_client_release (priv->handles[handle_type],
        sender, handles, &error))
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      g_free (sender);
      return;
    }

  g_free (sender);

  tp_svc_connection_return_from_release_handles (context);
}


/**
 * tp_base_connection_dbus_request_handles:
 * @iface: A pointer to #TpBaseConnection, cast to a pointer to
 *  #TpSvcConnection
 * @handle_type: The handle type (#TpHandleType) as a guint
 * @names: A strv of handle names
 * @context: The dbus-glib method invocation context
 *
 * Implements D-Bus method RequestHandles on interface
 * org.freedesktop.Telepathy.Connection. Exported so subclasses can
 * use it as a basis for their own implementations (for instance,
 * at the time of writing Gabble's GabbleConnection does its own processing
 * for room handles, in order to validate them asynchronously, but delegates
 * to this implementation for all other types).
 */
void
tp_base_connection_dbus_request_handles (TpSvcConnection *iface,
                                         guint handle_type,
                                         const gchar **names,
                                         DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (iface);
  TpHandleRepoIface *handle_repo = tp_base_connection_get_handles (self,
      handle_type);
  guint count = 0, i;
  const gchar **cur_name;
  GError *error = NULL;
  GArray *handles = NULL;
  gchar *sender;

  for (cur_name = names; *cur_name != NULL; cur_name++)
    {
      count++;
    }

  g_assert (TP_IS_BASE_CONNECTION (self));

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (self, context);

  if (!tp_handle_type_is_valid (handle_type, &error))
    {
      g_assert (error != NULL);
      goto out;
    }

  /* FIXME: NotAvailable is the wrong error, since that's meant to be for
   * transient errors which might go away later. It should raise
   * NotImplemented if the handle type is >0 and <NUM_TP_HANDLE_TYPES but we
   * don't have a repo for that type, but the spec doesn't currently allow us
   * to. It should.
   *
   * If the handle type is 0 or >= NUM_TP_HANDLE_TYPES we should raise
   * InvalidArgument.
   */
  if (handle_repo == NULL)
    {
      DEBUG ("unimplemented handle type %u", handle_type);

      error = g_error_new (TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
                          "unimplemented handle type %u", handle_type);
      goto out;
    }

  handles = g_array_sized_new (FALSE, FALSE, sizeof (guint), count);

  for (i = 0; i < count; i++)
    {
      TpHandle handle;
      const gchar *name = names[i];

      handle = tp_handle_ensure (handle_repo, name, NULL, &error);

      if (handle == 0)
        {
          DEBUG("RequestHandles of type %d failed because '%s' is invalid: %s",
              handle_type, name, error->message);
          g_assert (error != NULL);
          goto out;
        }
      g_array_append_val (handles, handle);
    }

  sender = dbus_g_method_get_sender (context);
  if (!tp_handles_client_hold (handle_repo, sender, handles, &error))
    {
      g_assert (error != NULL);
    }
  g_free (sender);

out:
  if (error == NULL)
    {
      tp_svc_connection_return_from_request_handles (context, handles);
    }
  else
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
    }

  if (handles != NULL)
    {
      tp_handles_unref (handle_repo, handles);
      g_array_free (handles, TRUE);
    }
}

/**
 * tp_base_connection_get_handles:
 * @self: A connection
 * @handle_type: The handle type
 *
 * <!---->
 *
 * Returns: the handle repository corresponding to the given handle type,
 * or #NULL if it's unsupported or invalid.
 */
TpHandleRepoIface *
tp_base_connection_get_handles (TpBaseConnection *self,
                                TpHandleType handle_type)
{
  TpBaseConnectionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CONNECTION, TpBaseConnectionPrivate);

  if (handle_type >= NUM_TP_HANDLE_TYPES)
    return NULL;
  return priv->handles[handle_type];
}


/**
 * tp_base_connection_get_self_handle:
 * @self: A connection
 *
 * Returns the #TpBaseConnection:self-handle property, which is guaranteed not
 * to be 0 once the connection has moved to the CONNECTED state.
 *
 * Returns: the current self handle of the connection.
 *
 * Since: 0.7.UNRELEASED
 */
TpHandle
tp_base_connection_get_self_handle (TpBaseConnection *self)
{
  return self->self_handle;
}

/**
 * tp_base_connection_set_self_handle:
 * @self: A connection
 * @self_handle: The new self handle for the connection.
 *
 * Sets the #TpBaseConnection:self-handle property.  self_handle may not be 0
 * once the connection has moved to the CONNECTED state.
 *
 * Since: 0.7.UNRELEASED
 */
void
tp_base_connection_set_self_handle (TpBaseConnection *self,
                                    TpHandle self_handle)
{
  g_object_set (self, "self-handle", self_handle, NULL);
}


/**
 * tp_base_connection_finish_shutdown:
 * @self: The connection
 *
 * Tell the connection manager that this Connection has been disconnected,
 * has emitted StatusChanged and is ready to be removed from D-Bus.
 */
void tp_base_connection_finish_shutdown (TpBaseConnection *self)
{
  GPtrArray *contexts = self->priv->disconnect_requests;
  guint i;

  g_return_if_fail (self->status == TP_CONNECTION_STATUS_DISCONNECTED);
  g_return_if_fail (contexts != NULL);

  self->priv->disconnect_requests = NULL;

  for (i = 0; i < contexts->len; i++)
    {
      tp_svc_connection_return_from_disconnect (g_ptr_array_index (contexts,
            i));
    }

  g_ptr_array_free (contexts, TRUE);

  g_signal_emit (self, signals[SHUTDOWN_FINISHED], 0);
}

/**
 * tp_base_connection_change_status:
 * @self: The connection
 * @status: The new status
 * @reason: The reason for the status change
 *
 * Change the status of the connection. The allowed state transitions are:
 *
 * <itemizedlist>
 * <listitem>NEW -> CONNECTING</listitem>
 * <listitem>CONNECTING -> CONNECTED</listitem>
 * <listitem>NEW -> CONNECTED (equivalent to both of the above one after the
 * other - see below)</listitem>
 * <listitem>(anything except DISCONNECTED) -> DISCONNECTED</listitem>
 * </itemizedlist>
 *
 * Before the transition to CONNECTED, the implementation must have discovered
 * the handle for the local user, obtained a reference to that handle and
 * stored it in the @self_handle member of #TpBaseConnection.
 *
 * Changing from NEW to CONNECTED is implemented by doing the transition from
 * NEW to CONNECTING, followed by the transition from CONNECTING to CONNECTED;
 * it's exactly equivalent to calling tp_base_connection_change_status for
 * those two transitions one after the other.
 *
 * Any other valid transition does the following, in this order:
 *
 * <itemizedlist>
 * <listitem>Update the @status member of #TpBaseConnection</listitem>
 * <listitem>If the new state is DISCONNECTED, call the close_all_channels
 * callback on all channel factories</listitem>
 * <listitem>If the new state is DISCONNECTED, unref the @self_handle, if
 * any, and set it to 0</listitem>
 * <listitem>Emit the D-Bus StatusChanged signal</listitem>
 * <listitem>Call the subclass' status change callback</listitem>
 * <listitem>Call the channel factories' status change callbacks</listitem>
 * <listitem>If the new state is DISCONNECTED, call the subclass'
 * @shut_down callback</listitem>
 * </itemizedlist>
 */
void
tp_base_connection_change_status (TpBaseConnection *self,
                                  TpConnectionStatus status,
                                  TpConnectionStatusReason reason)
{
  TpBaseConnectionPrivate *priv;
  TpBaseConnectionClass *klass;
  TpConnectionStatus prev_status;

  g_assert (TP_IS_BASE_CONNECTION (self));

  priv = TP_BASE_CONNECTION_GET_PRIVATE (self);
  klass = TP_BASE_CONNECTION_GET_CLASS (self);

  if (self->status == TP_INTERNAL_CONNECTION_STATUS_NEW
      && status == TP_CONNECTION_STATUS_CONNECTED)
    {
      /* going straight from NEW to CONNECTED would cause confusion, so before
       * we do anything else, go via CONNECTING */
      DEBUG("from NEW to CONNECTED: going via CONNECTING first");
      tp_base_connection_change_status (self, TP_CONNECTION_STATUS_CONNECTING,
          reason);
    }

  DEBUG("was %u, now %u, for reason %u", self->status, status, reason);
  g_return_if_fail (status != TP_INTERNAL_CONNECTION_STATUS_NEW);

  if (self->status == status)
    {
      g_warning ("%s: attempted to re-emit the current status %u, reason %u",
          G_STRFUNC, status, reason);
      return;
    }

  prev_status = self->status;
  self->status = status;

  /* make appropriate assertions about our state */
  switch (status)
    {
    case TP_CONNECTION_STATUS_DISCONNECTED:
      /* you can go from any state to DISCONNECTED, except DISCONNECTED;
       * and we already warned and returned if that was the case, so
       * nothing to do here */
      break;
    case TP_CONNECTION_STATUS_CONNECTED:
      /* you can only go to CONNECTED if you're CONNECTING (or NEW, but we
       * covered that by forcing a transition to CONNECTING above) */
      g_return_if_fail (prev_status == TP_CONNECTION_STATUS_CONNECTING);
      /* by the time we go CONNECTED we must have the self handle */
      g_return_if_fail (self->self_handle != 0);
      break;
    case TP_CONNECTION_STATUS_CONNECTING:
      /* you can't go CONNECTING if a connection attempt has been made
       * before */
      g_return_if_fail (prev_status == TP_INTERNAL_CONNECTION_STATUS_NEW);
      break;
    default:
      g_warning ("%s: invalid connection status %d", G_STRFUNC, status);
      g_assert_not_reached ();
      return;
    }

  if (status == TP_CONNECTION_STATUS_DISCONNECTED)
    {
      /* the presence of this array indicates that we are shutting down */
      if (self->priv->disconnect_requests == NULL)
        self->priv->disconnect_requests = g_ptr_array_sized_new (0);

      /* remove all channels and shut down all factories, so we don't get
       * any race conditions where method calls are delivered to a channel
       * after we've started disconnecting
       */
      tp_base_connection_close_all_channels (self);

      if (self->self_handle)
        {
          tp_handle_unref (priv->handles[TP_HANDLE_TYPE_CONTACT],
              self->self_handle);
          self->self_handle = 0;
        }
    }

  DEBUG("emitting status-changed to %u, for reason %u", status, reason);
  tp_svc_connection_emit_status_changed (self, status, reason);

  /* tell subclass and factories about the state change. In the case of
   * disconnection, shut down afterwards */
  switch (status)
    {
    case TP_CONNECTION_STATUS_CONNECTING:
      if (klass->connecting)
        (klass->connecting) (self);
      g_ptr_array_foreach (priv->channel_factories, (GFunc)
          tp_channel_factory_iface_connecting, NULL);
      break;

    case TP_CONNECTION_STATUS_CONNECTED:
      /* the implementation should have ensured we have a valid self_handle
       * before changing the state to CONNECTED */

      g_assert (self->self_handle != 0);
      g_assert (tp_handle_is_valid (priv->handles[TP_HANDLE_TYPE_CONTACT],
                self->self_handle, NULL));
      if (klass->connected)
        (klass->connected) (self);
      g_ptr_array_foreach (priv->channel_factories, (GFunc)
          tp_channel_factory_iface_connected, NULL);
      break;

    case TP_CONNECTION_STATUS_DISCONNECTED:
      if (prev_status != TP_INTERNAL_CONNECTION_STATUS_NEW)
        {
          if (klass->disconnected)
            (klass->disconnected) (self);
          g_ptr_array_foreach (priv->channel_factories, (GFunc)
              tp_channel_factory_iface_disconnected, NULL);
        }
      (klass->shut_down) (self);
      break;

    default:
      g_assert_not_reached ();
    }
}


/**
 * tp_base_connection_add_interfaces:
 * @self: A TpBaseConnection in state #TP_INTERNAL_CONNECTION_STATUS_NEW
 *  or #TP_CONNECTION_STATUS_CONNECTING
 * @interfaces: A %NULL-terminated array of D-Bus interface names, which
 *  must remain valid at least until the connection enters state
 *  #TP_CONNECTION_STATUS_DISCONNECTED (in practice, you should either
 *  use static strings, or use strdup'd strings and free them in the dispose
 *  callback).
 *
 * Add some interfaces to the list supported by this Connection. If you're
 * going to call this function at all, you must do so before moving to state
 * CONNECTED (or DISCONNECTED); if you don't call it, only the set of
 * interfaces always present (@interfaces_always_present in
 * #TpBaseConnectionClass) will be supported.
 */
void
tp_base_connection_add_interfaces (TpBaseConnection *self,
                                   const gchar **interfaces)
{
  guint i, n_new;
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (self);
  TpBaseConnectionClass *klass = TP_BASE_CONNECTION_GET_CLASS (self);

  g_return_if_fail (self->status != TP_CONNECTION_STATUS_CONNECTED);
  g_return_if_fail (self->status != TP_CONNECTION_STATUS_DISCONNECTED);

  if (interfaces == NULL || interfaces[0] == NULL)
    {
      /* If user tries to add no new interfaces, ignore it */
      return;
    }

  n_new = g_strv_length ((gchar **) interfaces);

  if (priv->interfaces)
    {
      guint size = priv->interfaces->len;

      g_array_set_size (priv->interfaces, size + n_new);
      for (i = 0; i < n_new; i++)
        {
          g_array_index (priv->interfaces, const gchar *, size + i) =
            interfaces[i];
        }
    }
  else
    {
      /* It's the first time anyone has added interfaces - create the array */
      guint n_static = 0;

      if (klass->interfaces_always_present)
        {
          n_static = g_strv_length (
              (gchar **) klass->interfaces_always_present);
        }
      priv->interfaces = g_array_sized_new (TRUE, FALSE, sizeof (gchar *),
          n_static + n_new);
      for (i = 0; i < n_static; i++)
        {
          g_array_append_val (priv->interfaces,
              klass->interfaces_always_present[i]);
        }
      for (i = 0; i < n_new; i++)
        {
          g_array_append_val (priv->interfaces, interfaces[i]);
        }
    }
}


static void
service_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcConnectionClass *klass = (TpSvcConnectionClass *)g_iface;

#define IMPLEMENT(prefix,x) tp_svc_connection_implement_##x (klass, \
    tp_base_connection_##prefix##x)
  IMPLEMENT(,connect);
  IMPLEMENT(,disconnect);
  IMPLEMENT(,get_interfaces);
  IMPLEMENT(,get_protocol);
  IMPLEMENT(dbus_,get_self_handle);
  IMPLEMENT(,get_status);
  IMPLEMENT(,hold_handles);
  IMPLEMENT(,inspect_handles);
  IMPLEMENT(,list_channels);
  IMPLEMENT(,request_channel);
  IMPLEMENT(,release_handles);
  IMPLEMENT(dbus_,request_handles);
#undef IMPLEMENT
}


static void
conn_requests_requestotron (TpBaseConnection *self,
                            GHashTable *requested_properties,
                            ChannelRequestMethod method,
                            DBusGMethodInvocation *context)
{
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (self);
  TpHandleRepoIface *handles;
  guint i;
  ChannelRequest *request = NULL;
  GHashTable *altered_properties = NULL;
  const gchar *type;
  TpChannelManagerRequestFunc func;
  gboolean suppress_handler;
  TpHandleType target_handle_type;
  TpHandle target_handle;
  GValue *target_handle_value = NULL;
  const gchar *target_id;
  gboolean valid;

  TP_BASE_CONNECTION_ERROR_IF_NOT_CONNECTED (self, context);

  type = tp_asv_get_string (requested_properties,
        TP_IFACE_CHANNEL ".ChannelType");

  if (type == NULL)
    {
      GError e = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "ChannelType is required" };

      dbus_g_method_return_error (context, &e);
      goto out;
    }

  target_handle_type = tp_asv_get_uint32 (requested_properties,
      TP_IFACE_CHANNEL ".TargetHandleType", &valid);

  /* Allow it to be missing, but not to be otherwise broken */
  if (!valid && tp_asv_lookup (requested_properties,
          TP_IFACE_CHANNEL ".TargetHandleType") != NULL)
    {
      GError e = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "TargetHandleType must be an integer in range 0 to 2**32-1" };

      dbus_g_method_return_error (context, &e);
      goto out;
    }

  target_handle = tp_asv_get_uint32 (requested_properties,
      TP_IFACE_CHANNEL ".TargetHandle", &valid);

  /* Allow it to be missing, but not to be otherwise broken */
  if (!valid && tp_asv_lookup (requested_properties,
          TP_IFACE_CHANNEL ".TargetHandle") != NULL)
    {
      GError e = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "TargetHandle must be an integer in range 0 to 2**32-1" };

      dbus_g_method_return_error (context, &e);
      goto out;
    }

  target_id = tp_asv_get_string (requested_properties,
      TP_IFACE_CHANNEL ".TargetID");

  /* Handle type 0 cannot have a handle */
  if (target_handle_type == TP_HANDLE_TYPE_NONE && target_handle != 0)
    {
      GError e = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "When TargetHandleType is NONE, TargetHandle must be omitted or 0" };

      dbus_g_method_return_error (context, &e);
      goto out;
    }

  /* Handle type 0 cannot have a target id */
  if (target_handle_type == TP_HANDLE_TYPE_NONE && target_id != NULL)
    {
      GError e = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "When TargetHandleType is NONE, TargetID must be omitted" };

      dbus_g_method_return_error (context, &e);
      goto out;
    }

  /* FIXME: when InitiatorHandle, InitiatorID and Requested are officially
   * supported, if the request has any of them, raise an error */

  if (target_handle_type != TP_HANDLE_TYPE_NONE)
    {
      GError *error = NULL;

      if ((target_handle == 0 && target_id == NULL) ||
          (target_handle != 0 && target_id != NULL))
        {
          GError e = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
            "Exactly one of TargetHandle and TargetID must be supplied" };

          dbus_g_method_return_error (context, &e);
          goto out;
        }

      handles = tp_base_connection_get_handles (self, target_handle_type);

      if (target_handle == 0)
        {
          /* Turn TargetID into TargetHandle */
          target_handle = tp_handle_ensure (handles, target_id, NULL, &error);

          if (target_handle == 0)
            {
              dbus_g_method_return_error (context, error);
              g_error_free (error);
              goto out;
            }

          altered_properties = g_hash_table_new_full (g_str_hash, g_str_equal,
              NULL, NULL);
          tp_g_hash_table_update (altered_properties, requested_properties,
              NULL, NULL);

          target_handle_value = tp_g_value_slice_new (G_TYPE_UINT);
          g_value_set_uint (target_handle_value, target_handle);
          g_hash_table_insert (altered_properties,
              TP_IFACE_CHANNEL ".TargetHandle", target_handle_value);

          g_hash_table_remove (altered_properties,
              TP_IFACE_CHANNEL ".TargetID");

          requested_properties = altered_properties;
        }
      else
        {
          /* Check the supplied TargetHandle is valid */
          if (!tp_handle_is_valid (handles, target_handle, &error))
            {
              dbus_g_method_return_error (context, error);
              g_error_free (error);
              goto out;
            }

          tp_handle_ref (handles, target_handle);
        }
    }


  switch (method)
    {
    case METHOD_CREATE_CHANNEL:
      func = tp_channel_manager_create_channel;
      suppress_handler = TRUE;
      break;

#if 0
    case METHOD_ENSURE_CHANNEL:
      func = tp_channel_manager_ensure_channel;
      suppress_handler = FALSE;
      break;
#endif

    default:
      g_assert_not_reached ();
    }

  request = channel_request_new (context, method,
      type, target_handle_type, target_handle, suppress_handler);
  g_ptr_array_add (priv->channel_requests, request);

  for (i = 0; i < priv->channel_managers->len; i++)
    {
      TpChannelManager *manager = TP_CHANNEL_MANAGER (
          g_ptr_array_index (priv->channel_managers, i));

      if (func (manager, request, requested_properties))
        goto out;
    }

  /* Nobody accepted the request */
  tp_dbus_g_method_return_not_implemented (context);

  request->context = NULL;
  g_ptr_array_remove (priv->channel_requests, request);
  channel_request_free (request);


out:
  if (target_handle != 0)
    tp_handle_unref (handles, target_handle);
  if (altered_properties != NULL)
    g_hash_table_destroy (altered_properties);
  if (target_handle_value != NULL)
    tp_g_value_slice_free (target_handle_value);

  return;
}


static void
conn_requests_create_channel (TpSvcConnectionInterfaceRequests *svc,
                              GHashTable *requested_properties,
                              DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (svc);

  return conn_requests_requestotron (self, requested_properties,
      METHOD_CREATE_CHANNEL, context);
}


#if 0
static void
conn_requests_ensure_channel (TpSvcConnectionInterfaceRequests *svc,
                              GHashTable *requested_properties,
                              DBusGMethodInvocation *context)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (svc);

  return conn_requests_requestotron (self, requested_properties,
      METHOD_ENSURE_CHANNEL, context);
}
#endif


static void
requests_iface_init (gpointer g_iface,
                     gpointer iface_data G_GNUC_UNUSED)
{
  TpSvcConnectionInterfaceRequestsClass *iface = g_iface;

#define IMPLEMENT(x) \
    tp_svc_connection_interface_requests_implement_##x (\
        iface, conn_requests_##x)
  IMPLEMENT (create_channel);
#if 0
  IMPLEMENT (ensure_channel);
#endif
#undef IMPLEMENT
}


static void
tp_base_connection_fill_contact_attributes (GObject *obj,
  const GArray *contacts, GHashTable *attributes_hash)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (obj);
  TpBaseConnectionPrivate *priv =
    TP_BASE_CONNECTION_GET_PRIVATE (self);
  guint i;

  for (i = 0; i < contacts->len; i++)
    {
      TpHandle handle;
      const gchar *tmp;
      GValue *val;


      handle = g_array_index (contacts, TpHandle, i);
      tmp = tp_handle_inspect (priv->handles[TP_HANDLE_TYPE_CONTACT], handle);
      g_assert (tmp != NULL);

      val = tp_g_value_slice_new (G_TYPE_STRING);
      g_value_set_static_string (val, tmp);

      tp_contacts_mixin_set_contact_attribute (attributes_hash,
          handle, TP_IFACE_CONNECTION"/contact-id", val);
    }
}

/**
 * tp_base_connection_register_with_contacts_mixin:
 * @self: An instance of the #TpBaseConnections that uses the Contacts
 * mixin
 *
 * Register the Connection interface with the Contacts interface to make it
 * inspectable. The Contacts mixin should be initialized before this function
 * is called
 */
void
tp_base_connection_register_with_contacts_mixin (TpBaseConnection *self)
{
  tp_contacts_mixin_add_contact_attributes_iface (G_OBJECT (self),
      TP_IFACE_CONNECTION,
      tp_base_connection_fill_contact_attributes);
}
