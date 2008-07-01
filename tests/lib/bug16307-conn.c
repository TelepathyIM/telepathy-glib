/*
 * bug16307-conn.c - connection that reproduces the #15307 bug
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */
#include "bug16307-conn.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/handle-repo-dynamic.h>
#include <telepathy-glib/util.h>

static void service_iface_init (gpointer, gpointer);
static void aliasing_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE (Bug16307Connection,
    bug16307_connection,
    SIMPLE_TYPE_CONNECTION,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION,
      service_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CONNECTION_INTERFACE_ALIASING,
      aliasing_iface_init);
    );

/* type definition stuff */

enum
{
  PROP_CONNECT = 1,
  N_PROPS
};

enum
{
  SIGNAL_GET_STATUS_RECEIVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

struct _Bug16307ConnectionPrivate
{
  /* In a real connection manager, the underlying implementation start
   * connecting, then go to state CONNECTED when finished. Here there isn't
   * actually a connection, so the connection process is fake and the time
   * when it connects can controlled by the code using Bug16307Connection.
   *
   * - If the connect property is not set, the default is to connect after
   *   a timeout of 500ms.
   * - If connect is "inject", it connects when
   *   bug16307_connection_inject_connect_succeed() is called
   * - If connect is "get_status", it connects the first time get_status is
   *   called
   */
  gchar *connect;

  /* get_status is run lately */
  DBusGMethodInvocation *get_status_invocation;
};

static void
bug16307_connection_init (Bug16307Connection *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, BUG16307_TYPE_CONNECTION,
      Bug16307ConnectionPrivate);
}

static void
get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *spec)
{
  Bug16307Connection *self = BUG16307_CONNECTION (object);

  switch (property_id) {
    case PROP_CONNECT:
      g_value_set_string (value, self->priv->connect);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
  }
}

static void
set_property (GObject *object,
              guint property_id,
              const GValue *value,
              GParamSpec *spec)
{
  Bug16307Connection *self = BUG16307_CONNECTION (object);

  switch (property_id) {
    case PROP_CONNECT:
      g_free (self->priv->connect);
      self->priv->connect = g_utf8_strdown (g_value_get_string (value), -1);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
  }
}

static void
finalize (GObject *object)
{
  Bug16307Connection *self = BUG16307_CONNECTION (object);

  g_free (self->priv->connect);

  G_OBJECT_CLASS (bug16307_connection_parent_class)->finalize (object);
}

static gboolean
pretend_connected (gpointer data)
{
  Bug16307Connection *self = BUG16307_CONNECTION (data);
  TpBaseConnection *conn = (TpBaseConnection *) self;
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (conn,
      TP_HANDLE_TYPE_CONTACT);
  gchar *account;

  g_object_get (self, "account", &account, NULL);

  conn->self_handle = tp_handle_ensure (contact_repo, account,
      NULL, NULL);

  g_free (account);

  tp_base_connection_change_status (conn, TP_CONNECTION_STATUS_CONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);

  return FALSE;
}

void
bug16307_connection_inject_connect_succeed (Bug16307Connection *self)
{
  g_assert (!tp_strdiff ("inject", self->priv->connect));
  pretend_connected (self);
}

void
bug16307_connection_inject_get_status_return (Bug16307Connection *self)
{
  TpBaseConnection *self_base = TP_BASE_CONNECTION (self);
  DBusGMethodInvocation *context;
  gulong get_signal_id;

  /* if we don't have a pending get_status yet, wait for it */
  if (self->priv->get_status_invocation == NULL)
    {
      GMainLoop *loop = g_main_loop_new (NULL, FALSE);

      get_signal_id = g_signal_connect_swapped (self, "get-status-received",
          G_CALLBACK (g_main_loop_quit), loop);

      g_main_loop_run (loop);

      g_signal_handler_disconnect (self, get_signal_id);
    }

  context = self->priv->get_status_invocation;
  g_assert (context != NULL);

  if (self_base->status == TP_INTERNAL_CONNECTION_STATUS_NEW)
    {
      tp_svc_connection_return_from_get_status (
          context, TP_CONNECTION_STATUS_DISCONNECTED);
    }
  else
    {
      tp_svc_connection_return_from_get_status (
          context, self_base->status);
    }

  self->priv->get_status_invocation = NULL;
}

static gboolean
start_connecting (TpBaseConnection *conn,
                  GError **error)
{
  Bug16307Connection *self = BUG16307_CONNECTION (conn);

  tp_base_connection_change_status (conn, TP_CONNECTION_STATUS_CONNECTING,
      TP_CONNECTION_STATUS_REASON_REQUESTED);

  /* In a real connection manager we'd ask the underlying implementation to
   * start connecting, then go to state CONNECTED when finished. Here there
   * isn't actually a connection, so we'll fake a connection process that
   * takes half a second. */
  if (self->priv->connect == NULL)
    g_timeout_add (500, pretend_connected, self);

  return TRUE;
}

static void
bug16307_connection_class_init (Bug16307ConnectionClass *klass)
{
  TpBaseConnectionClass *base_class =
      (TpBaseConnectionClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;
  static const gchar *interfaces_always_present[] = {
      TP_IFACE_CONNECTION_INTERFACE_ALIASING,
      TP_IFACE_CONNECTION_INTERFACE_CAPABILITIES,
      TP_IFACE_CONNECTION_INTERFACE_PRESENCE,
      TP_IFACE_CONNECTION_INTERFACE_AVATARS,
      NULL };

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->finalize = finalize;
  g_type_class_add_private (klass, sizeof (Bug16307ConnectionPrivate));

  base_class->start_connecting = start_connecting;

  base_class->interfaces_always_present = interfaces_always_present;

  param_spec = g_param_spec_string ("connect", "Connection type",
      "Connection type", NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_CONNECT, param_spec);

  signals[SIGNAL_GET_STATUS_RECEIVED] = g_signal_new ("get-status-received",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
}

/**
 * bug16307_connection_get_status
 *
 * Implements D-Bus method GetStatus
 * on interface org.freedesktop.Telepathy.Connection
 */
static void
bug16307_connection_get_status (TpSvcConnection *iface,
                              DBusGMethodInvocation *context)
{
  TpBaseConnection *self_base = TP_BASE_CONNECTION (iface);
  Bug16307Connection *self = BUG16307_CONNECTION (iface);

  /* auto-connect on get_status: it's the only difference with the base
   * implementation */
  if (!tp_strdiff ("get_status", self->priv->connect) &&
      (self_base->status == TP_INTERNAL_CONNECTION_STATUS_NEW ||
       self_base->status == TP_CONNECTION_STATUS_DISCONNECTED))
    {
      pretend_connected (self);
    }

  /* D-Bus return call later */
  g_assert (self->priv->get_status_invocation == NULL);
  g_assert (context != NULL);
  self->priv->get_status_invocation = context;

  g_signal_emit (self, signals[SIGNAL_GET_STATUS_RECEIVED], 0);
}


static void
service_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcConnectionClass *klass = (TpSvcConnectionClass *)g_iface;

#define IMPLEMENT(prefix,x) tp_svc_connection_implement_##x (klass, \
    bug16307_connection_##prefix##x)
  IMPLEMENT(,get_status);
#undef IMPLEMENT
}


static void
aliasing_iface_init (gpointer g_iface, gpointer iface_data)
{
  /* not implemented, just advertised */
}

