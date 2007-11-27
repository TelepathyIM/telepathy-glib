/*
 * factory.c - an example channel factory for channels talking to a particular
 * contact. Similar code is used for 1-1 IM channels in many protocols
 * (IRC private messages ("/query"), XMPP IM etc.)
 *
 * Copyright (C) 2007 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "factory.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>

#include "chan.h"

/* FIXME: we really ought to have a base class in the library for this,
 * it's such a common pattern... */

static void iface_init (gpointer iface, gpointer data);

G_DEFINE_TYPE_WITH_CODE (ExampleEchoFactory,
    example_echo_factory,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_FACTORY_IFACE, iface_init))

/* type definition stuff */

enum
{
  PROP_CONNECTION = 1,
  N_PROPS
};

struct _ExampleEchoFactoryPrivate
{
  TpBaseConnection *conn;

  /* GUINT_TO_POINTER (handle) => ExampleEchoChannel */
  GHashTable *channels;
};

static void
example_echo_factory_init (ExampleEchoFactory *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EXAMPLE_TYPE_ECHO_FACTORY,
      ExampleEchoFactoryPrivate);

  self->priv->channels = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, g_object_unref);
}

static void
dispose (GObject *object)
{
  ExampleEchoFactory *self = EXAMPLE_ECHO_FACTORY (object);

  tp_channel_factory_iface_close_all ((TpChannelFactoryIface *) object);
  g_assert (self->priv->channels == NULL);

  ((GObjectClass *) example_echo_factory_parent_class)->dispose (object);
}

static void
get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *pspec)
{
  ExampleEchoFactory *self = EXAMPLE_ECHO_FACTORY (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->priv->conn);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
set_property (GObject *object,
              guint property_id,
              const GValue *value,
              GParamSpec *pspec)
{
  ExampleEchoFactory *self = EXAMPLE_ECHO_FACTORY (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      /* We don't ref the connection, because it owns a reference to the
       * factory, and it guarantees that the factory's lifetime is
       * less than its lifetime */
      self->priv->conn = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
example_echo_factory_class_init (ExampleEchoFactoryClass *klass)
{
  GParamSpec *param_spec;
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->dispose = dispose;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  param_spec = g_param_spec_object ("connection", "Connection object",
      "The connection that owns this channel factory",
      TP_TYPE_BASE_CONNECTION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_CONNECTION, param_spec);

  g_type_class_add_private (klass, sizeof (ExampleEchoFactoryPrivate));
}

static void
close_all (TpChannelFactoryIface *iface)
{
  ExampleEchoFactory *self = EXAMPLE_ECHO_FACTORY (iface);

  if (self->priv->channels != NULL)
    {
      GHashTable *tmp = self->priv->channels;

      self->priv->channels = NULL;
      g_hash_table_destroy (tmp);
    }
}

struct _ForeachData
{
  gpointer user_data;
  TpChannelFunc callback;
};

static void
_foreach (gpointer key,
          gpointer value,
          gpointer user_data)
{
  struct _ForeachData *data = user_data;
  TpChannelIface *chan = TP_CHANNEL_IFACE (value);

  data->callback (chan, data->user_data);
}

static void
foreach (TpChannelFactoryIface *iface,
         TpChannelFunc callback,
         gpointer user_data)
{
  ExampleEchoFactory *self = EXAMPLE_ECHO_FACTORY (iface);
  struct _ForeachData data = { user_data, callback };

  g_hash_table_foreach (self->priv->channels, _foreach, &data);
}

static void
channel_closed_cb (ExampleEchoChannel *chan, ExampleEchoFactory *self)
{
  TpHandle handle;

  if (self->priv->channels != NULL)
    {
      g_object_get (chan,
          "handle", &handle,
          NULL);

      g_hash_table_remove (self->priv->channels, GUINT_TO_POINTER (handle));
    }
}

static ExampleEchoChannel *
new_channel (ExampleEchoFactory *self, TpHandle handle)
{
  ExampleEchoChannel *chan;
  gchar *object_path;

  object_path = g_strdup_printf ("%s/EchoChannel%u",
      self->priv->conn->object_path, handle);

  chan = g_object_new (EXAMPLE_TYPE_ECHO_CHANNEL,
      "connection", self->priv->conn,
      "object-path", object_path,
      "handle", handle,
      NULL);

  g_free (object_path);

  g_signal_connect (chan, "closed", (GCallback) channel_closed_cb, self);

  g_hash_table_insert (self->priv->channels, GUINT_TO_POINTER (handle), chan);

  tp_channel_factory_iface_emit_new_channel (self, (TpChannelIface *) chan,
      NULL);

  return chan;
}

static TpChannelFactoryRequestStatus
request (TpChannelFactoryIface *iface,
         const gchar *chan_type,
         TpHandleType handle_type,
         guint handle,
         gpointer request,
         TpChannelIface **ret,
         GError **error)
{
  ExampleEchoFactory *self = EXAMPLE_ECHO_FACTORY (iface);
  ExampleEchoChannel *chan;
  TpChannelFactoryRequestStatus status;
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles
      (self->priv->conn, TP_HANDLE_TYPE_CONTACT);

  if (tp_strdiff (chan_type, TP_IFACE_CHANNEL_TYPE_TEXT))
    return TP_CHANNEL_FACTORY_REQUEST_STATUS_NOT_IMPLEMENTED;

  if (handle_type != TP_HANDLE_TYPE_CONTACT)
    return TP_CHANNEL_FACTORY_REQUEST_STATUS_NOT_IMPLEMENTED;

  if (!tp_handle_is_valid (contact_repo, handle, error))
    return TP_CHANNEL_FACTORY_REQUEST_STATUS_ERROR;

  chan = g_hash_table_lookup (self->priv->channels, GUINT_TO_POINTER (handle));

  status = TP_CHANNEL_FACTORY_REQUEST_STATUS_EXISTING;
  if (chan == NULL)
    {
      status = TP_CHANNEL_FACTORY_REQUEST_STATUS_CREATED;
      chan = new_channel (self, handle);
    }

  g_assert (chan != NULL);
  *ret = TP_CHANNEL_IFACE (chan);
  return status;
}

static void
iface_init (gpointer iface,
            gpointer data)
{
  TpChannelFactoryIfaceClass *klass = iface;

  klass->close_all = close_all;
  klass->foreach = foreach;
  klass->request = request;
}
