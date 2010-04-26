/*
 * Base class for Client implementations
 *
 * Copyright © 2010 Collabora Ltd.
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
 * SECTION:base-client
 * @title: TpBaseClient
 * @short_description: base class for Telepathy clients on D-Bus
 *
 * FIXME
 */

#include "telepathy-glib/base-client.h"

#include <telepathy-glib/observe-channels-context-internal.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/channel-request.h>
#include <telepathy-glib/svc-client.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CLIENT
#include "telepathy-glib/debug-internal.h"

typedef struct _TpBaseClientClassPrivate TpBaseClientClassPrivate;

struct _TpBaseClientClassPrivate {
    /*<private>*/
    TpBaseClientClassObserveChannelsImpl observe_channels_impl;
};

static void observer_iface_init (gpointer, gpointer);
static void approver_iface_init (gpointer, gpointer);
static void handler_iface_init (gpointer, gpointer);
static void requests_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE(TpBaseClient, tp_base_client, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
      tp_dbus_properties_mixin_iface_init);
    G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CLIENT, NULL);
    G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CLIENT_OBSERVER, observer_iface_init);
    G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CLIENT_APPROVER, approver_iface_init);
    G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CLIENT_HANDLER, handler_iface_init);
    G_IMPLEMENT_INTERFACE(TP_TYPE_SVC_CLIENT_INTERFACE_REQUESTS,
      requests_iface_init);
    g_type_add_class_private (g_define_type_id, sizeof (
        TpBaseClientClassPrivate));)

enum {
    PROP_DBUS_DAEMON = 1,
    PROP_NAME,
    PROP_UNIQUIFY_NAME,
    N_PROPS
};

typedef enum {
    CLIENT_IS_OBSERVER = 1 << 0,
    CLIENT_IS_APPROVER = 1 << 1,
    CLIENT_IS_HANDLER = 1 << 2,
    CLIENT_HANDLER_WANTS_REQUESTS = 1 << 3,
    CLIENT_HANDLER_BYPASSES_APPROVAL = 1 << 4,
    CLIENT_OBSERVER_RECOVER = 1 << 5,
} ClientFlags;

struct _TpBaseClientPrivate
{
  TpDBusDaemon *dbus;
  gchar *name;
  gboolean uniquify_name;

  gboolean registered;
  ClientFlags flags;
  /* array of TP_HASH_TYPE_CHANNEL_CLASS */
  GPtrArray *observer_filters;
  /* array of TP_HASH_TYPE_CHANNEL_CLASS */
  GPtrArray *approver_filters;
  /* array of TP_HASH_TYPE_CHANNEL_CLASS */
  GPtrArray *handler_filters;
  /* array of g_strdup(token), plus NULL included in length */
  GPtrArray *handler_caps;
};

static GHashTable *
_tp_base_client_copy_filter (GHashTable *filter)
{
  GHashTable *copy;

  copy = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) tp_g_value_slice_free);
  tp_g_hash_table_update (copy, filter, (GBoxedCopyFunc) g_strdup,
      (GBoxedCopyFunc) tp_g_value_slice_dup);
  return copy;
}

void
tp_base_client_add_observer_filter (TpBaseClient *self,
    GHashTable *filter)
{
  g_return_if_fail (filter != NULL);
  tp_base_client_take_observer_filter (self,
      _tp_base_client_copy_filter (filter));
}

/**
 * tp_base_client_take_observer_filter: (skip)
 * @self: a client
 * @filter: a %TP_HASH_TYPE_CHANNEL_CLASS, ownership of which is taken by @self
 *
 * The same as tp_base_client_add_observer_filter(), but ownership of @filter
 * is taken by @self. This makes it convenient to call using tp_asv_new():
 *
 * |[
 * tp_base_client_take_observer_filter (client,
 *    tp_asv_new (
 *        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
 *            TP_IFACE_CHANNEL_TYPE_TEXT,
 *        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
 *            TP_HANDLE_TYPE_CONTACT,
 *        ...));
 * ]|
 */
void
tp_base_client_take_observer_filter (TpBaseClient *self,
    GHashTable *filter)
{
  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);

  self->priv->flags |= CLIENT_IS_OBSERVER;
  g_ptr_array_add (self->priv->observer_filters, filter);
}

void
tp_base_client_set_observer_recover (TpBaseClient *self,
    gboolean recover)
{
  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);

  self->priv->flags |= (CLIENT_IS_OBSERVER | CLIENT_OBSERVER_RECOVER);
}

void
tp_base_client_add_approver_filter (TpBaseClient *self,
    GHashTable *filter)
{
  g_return_if_fail (filter != NULL);
  tp_base_client_take_approver_filter (self,
      _tp_base_client_copy_filter (filter));
}

void
tp_base_client_take_approver_filter (TpBaseClient *self,
    GHashTable *filter)
{
  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);

  self->priv->flags |= CLIENT_IS_APPROVER;
  g_ptr_array_add (self->priv->approver_filters, filter);
}

void
tp_base_client_be_a_handler (TpBaseClient *self)
{
  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);

  self->priv->flags |= CLIENT_IS_HANDLER;
}

void
tp_base_client_add_handler_filter (TpBaseClient *self,
    GHashTable *filter)
{
  g_return_if_fail (filter != NULL);
  tp_base_client_take_handler_filter (self,
      _tp_base_client_copy_filter (filter));
}

void
tp_base_client_take_handler_filter (TpBaseClient *self,
    GHashTable *filter)
{
  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);

  self->priv->flags |= CLIENT_IS_HANDLER;
  g_ptr_array_add (self->priv->handler_filters, filter);
}

void
tp_base_client_set_handler_bypass_approval (TpBaseClient *self,
    gboolean bypass_approval)
{
  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);

  self->priv->flags |= (CLIENT_IS_HANDLER | CLIENT_HANDLER_BYPASSES_APPROVAL);
}

void
tp_base_client_set_handler_request_notification (TpBaseClient *self)
{
  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);

  self->priv->flags |= (CLIENT_IS_HANDLER | CLIENT_HANDLER_WANTS_REQUESTS);
}

static void
_tp_base_client_add_handler_capability (TpBaseClient *self,
    const gchar *token)
{
  self->priv->flags |= CLIENT_IS_HANDLER;

  g_assert (g_ptr_array_index (self->priv->handler_caps,
        self->priv->handler_caps->len - 1) == NULL);
  g_ptr_array_index (self->priv->handler_caps,
      self->priv->handler_caps->len - 1) = g_strdup (token);
  g_ptr_array_add (self->priv->handler_caps, NULL);
}

/**
 * tp_base_client_add_handler_capability:
 * @self: a client, which must not have been registered with
 *  tp_base_client_register() yet
 * @token: a capability token as defined by the Telepathy D-Bus API
 *  Specification
 *
 * Add one capability token to this client, as if via
 * tp_base_client_add_handler_capabilities().
 */
void
tp_base_client_add_handler_capability (TpBaseClient *self,
    const gchar *token)
{
  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);

  _tp_base_client_add_handler_capability (self, token);
}

/**
 * tp_base_client_add_handler_capabilities:
 * @self: a client, which must not have been registered with
 *  tp_base_client_register() yet
 * @tokens: (array zero-terminated=1) (element-type utf8): capability
 *  tokens as defined by the Telepathy D-Bus API Specification
 *
 * Add several capability tokens to this client. These are used to signal
 * that Telepathy connection managers should advertise certain capabilities
 * to other contacts, such as the ability to receive audio/video calls using
 * particular streaming protocols and codecs.
 */
void
tp_base_client_add_handler_capabilities (TpBaseClient *self,
    const gchar * const *tokens)
{
  const gchar * const *iter;

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);

  if (tokens == NULL)
    return;

  for (iter = tokens; *iter != NULL; iter++)
    _tp_base_client_add_handler_capability (self, *iter);
}

/**
 * tp_base_client_add_handler_capabilities_varargs: (skip)
 * @self: a client, which must not have been registered with
 *  tp_base_client_register() yet
 * @first_token: a capability token from the Telepathy D-Bus API Specification
 * @...: more tokens, ending with %NULL
 *
 * Convenience C API equivalent to calling
 * tp_base_client_add_handler_capability() for each capability token.
 */
void
tp_base_client_add_handler_capabilities_varargs (TpBaseClient *self,
    const gchar *first_token, ...)
{
  va_list ap;
  const gchar *token = first_token;

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);

  va_start (ap, first_token);

  for (token = first_token; token != NULL; token = va_arg (ap, gchar *))
    _tp_base_client_add_handler_capability (self, token);

  va_end (ap);
}

/**
 * tp_base_client_register:
 * @self: a client, which must not have been registered with
 *  tp_base_client_register() yet
 *
 * Publish @self as an available client. After this method is called, as long
 * as it continues to exist, it will receive and process whatever events were
 * requested via the various filters.
 *
 * Methods that set the filters and other immutable state cannot be called
 * after this one.
 */
void
tp_base_client_register (TpBaseClient *self)
{
  GString *string;
  GError *error = NULL;
  gchar *path;
  static guint unique_counter = 0;

  g_return_if_fail (TP_IS_BASE_CLIENT (self));
  g_return_if_fail (!self->priv->registered);
  /* Client should at least be an Observer, Approver or Handler */
  g_return_if_fail (self->priv->flags != 0);

  string = g_string_new (TP_CLIENT_BUS_NAME_BASE);
  g_string_append (string, self->priv->name);

  if (self->priv->uniquify_name)
    {
      const gchar *unique;

      unique = tp_dbus_daemon_get_unique_name (self->priv->dbus);
      g_string_append_printf (string, ".%s", tp_escape_as_identifier (unique));
      g_string_append_printf (string, ".n%u", unique_counter++);
    }

  DEBUG ("request name %s", string->str);

  if (!tp_dbus_daemon_request_name (self->priv->dbus, string->str, TRUE,
        &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      g_string_free (string, TRUE);
      return;
    }

  path = g_strdup_printf ("/%s", g_strdelimit (string->str, ".", '/'));

  tp_dbus_daemon_register_object (self->priv->dbus, path, G_OBJECT (self));

  g_string_free (string, TRUE);
  g_free (path);

  self->priv->registered = TRUE;
}

/**
 * Only works after tp_base_client_set_handler_request_notification().
 *
 * Returns: (transfer container) (element-type Tp.ChannelRequest): the
 *  requests
 */
GList *
tp_base_client_get_pending_requests (TpBaseClient *self)
{
  /* FIXME */
  return NULL;
}

/**
 * Returns the set of channels currently handled by this base client or by any
 * other #TpBaseClient with which it shares a unique name.
 *
 * Returns: (transfer container) (element-type Tp.Channel): the handled
 *  channels
 */
GList *
tp_base_client_get_handled_channels (TpBaseClient *self)
{
  /* FIXME */
  return NULL;
}

static void
tp_base_client_init (TpBaseClient *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_BASE_CLIENT,
      TpBaseClientPrivate);

  /* wild guess: most clients won't need more than one of each filter */
  self->priv->observer_filters = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_hash_table_unref);
  self->priv->approver_filters = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_hash_table_unref);
  self->priv->handler_filters = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_hash_table_unref);
  self->priv->handler_caps = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (self->priv->handler_caps, NULL);
}

static void
tp_base_client_dispose (GObject *object)
{
  TpBaseClient *self = TP_BASE_CLIENT (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_base_client_parent_class)->dispose;

  if (self->priv->dbus != NULL)
    {
      g_object_unref (self->priv->dbus);
      self->priv->dbus = NULL;
    }

  if (dispose != NULL)
    dispose (object);
}

static void
tp_base_client_finalize (GObject *object)
{
  TpBaseClient *self = TP_BASE_CLIENT (object);
  void (*finalize) (GObject *) =
    G_OBJECT_CLASS (tp_base_client_parent_class)->finalize;

  g_free (self->priv->name);

  g_ptr_array_free (self->priv->observer_filters, TRUE);
  g_ptr_array_free (self->priv->approver_filters, TRUE);
  g_ptr_array_free (self->priv->handler_filters, TRUE);
  g_ptr_array_free (self->priv->handler_caps, TRUE);

  if (finalize != NULL)
    finalize (object);
}

static void
tp_base_client_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpBaseClient *self = TP_BASE_CLIENT (object);

  switch (property_id)
    {
      case PROP_DBUS_DAEMON:
        g_value_set_object (value, self->priv->dbus);
        break;
      case PROP_NAME:
        g_value_set_string (value, self->priv->name);
        break;
      case PROP_UNIQUIFY_NAME:
        g_value_set_boolean (value, self->priv->uniquify_name);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_base_client_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpBaseClient *self = TP_BASE_CLIENT (object);

  switch (property_id)
    {
      case PROP_DBUS_DAEMON:
        self->priv->dbus = g_value_dup_object (value);
        break;
      case PROP_NAME:
        self->priv->name = g_value_dup_string (value);
        break;
      case PROP_UNIQUIFY_NAME:
        self->priv->uniquify_name = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

typedef enum {
    DP_INTERFACES,
    DP_APPROVER_CHANNEL_FILTER,
    DP_HANDLER_CHANNEL_FILTER,
    DP_BYPASS_APPROVAL,
    DP_CAPABILITIES,
    DP_HANDLED_CHANNELS,
    DP_OBSERVER_CHANNEL_FILTER,
    DP_OBSERVER_RECOVER,
} ClientDBusProp;

static void
tp_base_client_get_dbus_properties (GObject *object,
    GQuark iface,
    GQuark name,
    GValue *value,
    gpointer getter_data)
{
  TpBaseClient *self = TP_BASE_CLIENT (object);
  ClientDBusProp which = GPOINTER_TO_INT (getter_data);

  switch (which)
    {
    case DP_INTERFACES:
        {
          GPtrArray *arr = g_ptr_array_sized_new (5);

          if (self->priv->flags & CLIENT_IS_OBSERVER)
            g_ptr_array_add (arr, g_strdup (TP_IFACE_CLIENT_OBSERVER));

          if (self->priv->flags & CLIENT_IS_APPROVER)
            g_ptr_array_add (arr, g_strdup (TP_IFACE_CLIENT_APPROVER));

          if (self->priv->flags & CLIENT_IS_HANDLER)
            g_ptr_array_add (arr, g_strdup (TP_IFACE_CLIENT_HANDLER));

          if (self->priv->flags & CLIENT_HANDLER_WANTS_REQUESTS)
            g_ptr_array_add (arr, g_strdup (
                  TP_IFACE_CLIENT_INTERFACE_REQUESTS));

          g_ptr_array_add (arr, NULL);
          g_value_take_boxed (value, g_ptr_array_free (arr, FALSE));
        }
      break;

    case DP_OBSERVER_CHANNEL_FILTER:
      g_value_set_boxed (value, self->priv->observer_filters);
      break;

    case DP_APPROVER_CHANNEL_FILTER:
      g_value_set_boxed (value, self->priv->approver_filters);
      break;

    case DP_HANDLER_CHANNEL_FILTER:
      g_value_set_boxed (value, self->priv->handler_filters);
      break;

    case DP_BYPASS_APPROVAL:
      g_value_set_boolean (value,
          (self->priv->flags & CLIENT_HANDLER_BYPASSES_APPROVAL) != 0);
      break;

    case DP_CAPABILITIES:
      /* this is already NULL-terminated */
      g_value_set_boxed (value, (GStrv) self->priv->handler_caps->pdata);
      break;

    case DP_HANDLED_CHANNELS:
        {
          GList *channels = tp_base_client_get_handled_channels (self);
          GList *iter;
          GPtrArray *arr = g_ptr_array_sized_new (g_list_length (channels));

          for (iter = channels; iter != NULL; iter = iter->next)
            g_ptr_array_add (arr,
                g_strdup (tp_proxy_get_object_path (iter->data)));

          g_value_take_boxed (value, arr);
          g_list_free (channels);
        }
      break;

    case DP_OBSERVER_RECOVER:
      g_value_set_boolean (value,
          (self->priv->flags & CLIENT_OBSERVER_RECOVER) != 0);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
tp_base_client_class_init (TpBaseClientClass *cls)
{
  GParamSpec *param_spec;
  static TpDBusPropertiesMixinPropImpl client_properties[] = {
        { "Interfaces", GINT_TO_POINTER (DP_INTERFACES) },
        { NULL }
  };
  static TpDBusPropertiesMixinPropImpl handler_properties[] = {
        { "HandlerChannelFilter",
          GINT_TO_POINTER (DP_HANDLER_CHANNEL_FILTER) },
        { "BypassApproval",
          GINT_TO_POINTER (DP_BYPASS_APPROVAL) },
        { "Capabilities",
          GINT_TO_POINTER (DP_CAPABILITIES) },
        { "HandledChannels",
          GINT_TO_POINTER (DP_HANDLED_CHANNELS) },
        { NULL }
  };
  static TpDBusPropertiesMixinPropImpl approver_properties[] = {
        { "ApproverChannelFilter",
          GINT_TO_POINTER (DP_APPROVER_CHANNEL_FILTER) },
        { NULL }
  };
  static TpDBusPropertiesMixinPropImpl observer_properties[] = {
        { "ObserverChannelFilter",
          GINT_TO_POINTER (DP_OBSERVER_CHANNEL_FILTER) },
        { "Recover",
          GINT_TO_POINTER (DP_OBSERVER_RECOVER) },
        { NULL }
  };
  static TpDBusPropertiesMixinIfaceImpl prop_ifaces[] = {
        { TP_IFACE_CLIENT, tp_base_client_get_dbus_properties, NULL,
          client_properties },
        { TP_IFACE_CLIENT_OBSERVER, tp_base_client_get_dbus_properties, NULL,
          observer_properties },
        { TP_IFACE_CLIENT_APPROVER, tp_base_client_get_dbus_properties, NULL,
          approver_properties },
        { TP_IFACE_CLIENT_HANDLER, tp_base_client_get_dbus_properties, NULL,
          handler_properties },
        { NULL }
  };
  GObjectClass *object_class = G_OBJECT_CLASS (cls);

  g_type_class_add_private (cls, sizeof (TpBaseClientPrivate));

  object_class->get_property = tp_base_client_get_property;
  object_class->set_property = tp_base_client_set_property;
  object_class->dispose = tp_base_client_dispose;
  object_class->finalize = tp_base_client_finalize;

  param_spec = g_param_spec_object ("dbus-daemon", "TpDBusDaemon object",
      "The dbus daemon associated with this client",
      TP_TYPE_DBUS_DAEMON,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DBUS_DAEMON, param_spec);

 param_spec = g_param_spec_string ("name", "name",
      "The name of the client",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_NAME, param_spec);

 param_spec = g_param_spec_boolean ("uniquify-name", "Uniquify name",
      "if TRUE, append a unique token to the name",
      FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_UNIQUIFY_NAME,
      param_spec);

  cls->dbus_properties_class.interfaces = prop_ifaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpBaseClientClass, dbus_properties_class));
}

static GList *
ptr_array_to_list (GPtrArray *arr)
{
  guint i;
  GList *result = NULL;

  for (i = 0; i > arr->len; i++)
    result = g_list_prepend (result, g_ptr_array_index (arr, i));

  return g_list_reverse (result);
}

static void
context_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpBaseClient *self = user_data;
  TpBaseClientClassPrivate *klass_pv = G_TYPE_CLASS_GET_PRIVATE (
      TP_BASE_CLIENT_GET_CLASS (self), TP_TYPE_BASE_CLIENT,
      TpBaseClientClassPrivate);
  TpObserveChannelsContext *ctx = TP_OBSERVE_CHANNELS_CONTEXT (source);
  GError *error = NULL;
  GList *channels_list, *requests_list;

  if (!tp_observe_channels_context_prepare_finish (ctx, result, &error))
    {
      DEBUG ("Failed to prepare TpObserveChannelsContext: %s", error->message);
      dbus_g_method_return_error (ctx->dbus_context, error);
      return;
    }

  channels_list = ptr_array_to_list (ctx->channels);
  requests_list = ptr_array_to_list (ctx->requests);

  klass_pv->observe_channels_impl (self, ctx->account, ctx->connection,
      channels_list, ctx->dispatch_operation, requests_list, ctx);

  g_list_free (channels_list);
  g_list_free (requests_list);

  if (_tp_observe_channels_context_get_state (ctx) ==
      TP_BASE_CLIENT_CONTEXT_STATE_NONE)
    g_warning ("Implementation of ObserveChannels didn't call "
        "tp_observe_channels_context_{accept,fail,delay}");
}

static void
_tp_base_client_observe_channels (TpSvcClientObserver *iface,
    const gchar *account_path,
    const gchar *connection_path,
    const GPtrArray *channels_arr,
    const gchar *dispatch_operation_path,
    const GPtrArray *requests_arr,
    GHashTable *observer_info,
    DBusGMethodInvocation *context)
{
  TpBaseClient *self = TP_BASE_CLIENT (iface);
  TpObserveChannelsContext *ctx;
  TpBaseClientClassPrivate *klass_pv = G_TYPE_CLASS_GET_PRIVATE (
      TP_BASE_CLIENT_GET_CLASS (self), TP_TYPE_BASE_CLIENT,
      TpBaseClientClassPrivate);
  GError *error = NULL;
  TpAccount *account = NULL;
  TpConnection *connection = NULL;
  GPtrArray *channels = NULL, *requests = NULL;
  TpChannelDispatchOperation *dispatch_operation = NULL;
  guint i;

  if (!(self->priv->flags & CLIENT_IS_OBSERVER))
    {
      /* Pretend that the method is not implemented if we are not supposed to
       * be an Observer. */
      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  if (klass_pv->observe_channels_impl == NULL)
    {
      DEBUG ("ObserveChannels has not be implemented");
      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  if (channels_arr->len == 0)
    {
      g_set_error (&error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Channels should contain at least one channel");
      DEBUG ("%s", error->message);
      goto out;
    }

  account = tp_account_new (self->priv->dbus, account_path, &error);
  if (account == NULL)
    {
      DEBUG ("Failed to create TpAccount: %s", error->message);
      goto out;
    }

  connection = tp_connection_new (self->priv->dbus, NULL, connection_path,
      &error);
  if (connection == NULL)
    {
      DEBUG ("Failed to create TpConnection: %s", error->message);
      goto out;
    }

  channels = g_ptr_array_new_with_free_func (g_object_unref);
  for (i = 0; i < channels_arr->len; i++)
    {
      const gchar *chan_path;
      GHashTable *chan_props;
      TpChannel *channel;

      tp_value_array_unpack (g_ptr_array_index (channels_arr, i), 2,
          &chan_path, &chan_props);

      channel = tp_channel_new_from_properties (connection,
          chan_path, chan_props, &error);
      if (channel == NULL)
        {
          DEBUG ("Failed to create TpChannel: %s", error->message);
          goto out;
        }

      g_ptr_array_add (channels, channel);
    }

  if (!tp_strdiff (dispatch_operation_path, "/"))
    {
      dispatch_operation = NULL;
    }
  else
    {
      dispatch_operation = tp_channel_dispatch_operation_new (self->priv->dbus,
            dispatch_operation_path, NULL, &error);
     if (dispatch_operation == NULL)
       {
         DEBUG ("Failed to create TpChannelDispatchOperation: %s",
             error->message);
         goto out;
        }
    }

  requests = g_ptr_array_new_with_free_func (g_object_unref);
  for (i = 0; i < requests_arr->len; i++)
    {
      const gchar *req_path = g_ptr_array_index (requests_arr, i);
      TpChannelRequest *request;

      request = tp_channel_request_new (self->priv->dbus, req_path, NULL,
          &error);
      if (request == NULL)
        {
          DEBUG ("Failed to create TpChannelRequest: %s", error->message);
          goto out;
        }

      g_ptr_array_add (requests, request);
    }

  ctx = _tp_observe_channels_context_new (account, connection, channels,
      dispatch_operation, requests, observer_info, context);

  tp_observe_channels_context_prepare_async (ctx, context_prepare_cb, self);

  g_object_unref (ctx);

out:
  if (account != NULL)
    g_object_unref (account);

  if (connection != NULL)
    g_object_unref (connection);

  if (channels != NULL)
    g_ptr_array_unref (channels);

  if (dispatch_operation != NULL)
    g_object_unref (dispatch_operation);

  if (requests != NULL)
    g_ptr_array_unref (requests);

  if (error == NULL)
    return;

  dbus_g_method_return_error (context, error);
  g_error_free (error);
}

static void
observer_iface_init (gpointer g_iface,
    gpointer unused G_GNUC_UNUSED)
{
#define IMPLEMENT(x) tp_svc_client_observer_implement_##x (\
  g_iface, _tp_base_client_##x)
  IMPLEMENT (observe_channels);
#undef IMPLEMENT
}

static void
_tp_base_client_add_dispatch_operation (TpSvcClientApprover *iface,
    const GPtrArray *channels,
    const gchar *dispatch_operation,
    GHashTable *properties,
    DBusGMethodInvocation *context)
{
  /* FIXME */
  tp_dbus_g_method_return_not_implemented (context);
}

static void
approver_iface_init (gpointer g_iface,
    gpointer unused G_GNUC_UNUSED)
{
#define IMPLEMENT(x) tp_svc_client_approver_implement_##x (\
  g_iface, _tp_base_client_##x)
  IMPLEMENT (add_dispatch_operation);
#undef IMPLEMENT
}

static void
_tp_base_client_handle_channels (TpSvcClientHandler *iface,
    const gchar *account,
    const gchar *connection,
    const GPtrArray *channels,
    const GPtrArray *requests,
    guint64 user_action_time,
    GHashTable *handler_info,
    DBusGMethodInvocation *context)
{
  /* FIXME */
  tp_dbus_g_method_return_not_implemented (context);
}

static void
handler_iface_init (gpointer g_iface,
    gpointer unused G_GNUC_UNUSED)
{
#define IMPLEMENT(x) tp_svc_client_handler_implement_##x (\
  g_iface, _tp_base_client_##x)
  IMPLEMENT (handle_channels);
#undef IMPLEMENT
}

static void
_tp_base_client_add_request (TpSvcClientInterfaceRequests *iface,
    const gchar *request,
    GHashTable *properties,
    DBusGMethodInvocation *context)
{
  /* FIXME: emit a signal first */
  tp_svc_client_interface_requests_return_from_add_request (context);
}

static void
_tp_base_client_remove_request (TpSvcClientInterfaceRequests *iface,
    const gchar *request,
    const gchar *error,
    const gchar *reason,
    DBusGMethodInvocation *context)
{
  /* FIXME: emit a signal first */
  tp_svc_client_interface_requests_return_from_remove_request (context);
}

static void
requests_iface_init (gpointer g_iface,
    gpointer unused G_GNUC_UNUSED)
{
#define IMPLEMENT(x) tp_svc_client_interface_requests_implement_##x (\
  g_iface, _tp_base_client_##x)
  IMPLEMENT (add_request);
  IMPLEMENT (remove_request);
#undef IMPLEMENT
}

void
tp_base_client_implement_observe_channels (TpBaseClientClass *klass,
    TpBaseClientClassObserveChannelsImpl impl)
{
  TpBaseClientClassPrivate *klass_pv = G_TYPE_CLASS_GET_PRIVATE (
      klass, TP_TYPE_BASE_CLIENT, TpBaseClientClassPrivate);

  klass_pv->observe_channels_impl = impl;
}
