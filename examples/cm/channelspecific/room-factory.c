/*
 * room-factory.c: example channel factory for chatrooms
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "room-factory.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>

#include "room.h"

/* FIXME: we really ought to have a base class in the library for this,
 * it's such a common pattern... */

static void iface_init (gpointer iface, gpointer data);

G_DEFINE_TYPE_WITH_CODE (ExampleCSHRoomFactory,
    example_csh_room_factory,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_FACTORY_IFACE, iface_init))

/* type definition stuff */

enum
{
  PROP_CONNECTION = 1,
  N_PROPS
};

struct _ExampleCSHRoomFactoryPrivate
{
  TpBaseConnection *conn;

  /* GUINT_TO_POINTER (room handle) => ExampleCSHRoomChannel */
  GHashTable *channels;
};

static void
example_csh_room_factory_init (ExampleCSHRoomFactory *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EXAMPLE_TYPE_CSH_ROOM_FACTORY, ExampleCSHRoomFactoryPrivate);

  self->priv->channels = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, g_object_unref);
}

static void
dispose (GObject *object)
{
  ExampleCSHRoomFactory *self = EXAMPLE_CSH_ROOM_FACTORY (object);

  tp_channel_factory_iface_close_all ((TpChannelFactoryIface *) object);
  g_assert (self->priv->channels == NULL);

  ((GObjectClass *) example_csh_room_factory_parent_class)->dispose (object);
}

static void
get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *pspec)
{
  ExampleCSHRoomFactory *self = EXAMPLE_CSH_ROOM_FACTORY (object);

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
  ExampleCSHRoomFactory *self = EXAMPLE_CSH_ROOM_FACTORY (object);

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
example_csh_room_factory_class_init (ExampleCSHRoomFactoryClass *klass)
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

  g_type_class_add_private (klass, sizeof (ExampleCSHRoomFactoryPrivate));
}

static void
example_csh_room_factory_close_all (TpChannelFactoryIface *iface)
{
  ExampleCSHRoomFactory *self = EXAMPLE_CSH_ROOM_FACTORY (iface);

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
foreach_helper (gpointer key,
                gpointer value,
                gpointer user_data)
{
  struct _ForeachData *data = user_data;
  TpChannelIface *chan = TP_CHANNEL_IFACE (value);

  data->callback (chan, data->user_data);
}

static void
example_csh_room_factory_foreach (TpChannelFactoryIface *iface,
                                  TpChannelFunc callback,
                                  gpointer user_data)
{
  ExampleCSHRoomFactory *self = EXAMPLE_CSH_ROOM_FACTORY (iface);
  struct _ForeachData data = { user_data, callback };

  g_hash_table_foreach (self->priv->channels, foreach_helper, &data);
}

static void
channel_closed_cb (ExampleCSHRoomChannel *chan, ExampleCSHRoomFactory *self)
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

static ExampleCSHRoomChannel *
new_channel (ExampleCSHRoomFactory *self, TpHandle handle)
{
  ExampleCSHRoomChannel *chan;
  gchar *object_path;

  object_path = g_strdup_printf ("%s/CSHRoomChannel%u",
      self->priv->conn->object_path, handle);

  chan = g_object_new (EXAMPLE_TYPE_CSH_ROOM_CHANNEL,
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
example_csh_room_factory_request (TpChannelFactoryIface *iface,
                                  const gchar *chan_type,
                                  TpHandleType handle_type,
                                  guint handle,
                                  gpointer request_id,
                                  TpChannelIface **ret,
                                  GError **error)
{
  ExampleCSHRoomFactory *self = EXAMPLE_CSH_ROOM_FACTORY (iface);
  ExampleCSHRoomChannel *chan;
  TpChannelFactoryRequestStatus status;
  TpHandleRepoIface *room_repo = tp_base_connection_get_handles
      (self->priv->conn, TP_HANDLE_TYPE_ROOM);

  if (tp_strdiff (chan_type, TP_IFACE_CHANNEL_TYPE_TEXT))
    return TP_CHANNEL_FACTORY_REQUEST_STATUS_NOT_IMPLEMENTED;

  if (handle_type != TP_HANDLE_TYPE_ROOM)
    return TP_CHANNEL_FACTORY_REQUEST_STATUS_NOT_IMPLEMENTED;

  if (!tp_handle_is_valid (room_repo, handle, error))
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

  klass->close_all = example_csh_room_factory_close_all;
  klass->foreach = example_csh_room_factory_foreach;
  klass->request = example_csh_room_factory_request;
}
