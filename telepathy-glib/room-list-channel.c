/*
 * room-list-channel.c - High level API for RoomList channels
 *
 * Copyright (C) 2012 Collabora Ltd. <http://www.collabora.co.uk/>
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
 * SECTION:room-list-channel
 * @title: TpRoomListChannel
 * @short_description: proxy object for a room list channel
 *
 * #TpRoomListChannel is a sub-class of #TpChannel providing convenient API
 * to list rooms.
 */

/**
 * TpRoomListChannel:
 *
 * Data structure representing a #TpRoomListChannel.
 *
 * Since: UNRELEASED
 */

/**
 * TpRoomListChannelClass:
 *
 * The class of a #TpRoomListChannel.
 *
 * Since: UNRELEASED
 */

#include <config.h>

#include "telepathy-glib/room-list-channel.h"
#include "telepathy-glib/room-list-channel-internal.h"

#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gnio-util.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/room-info-internal.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/util-internal.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "telepathy-glib/debug-internal.h"

#include <stdio.h>
#include <glib/gstdio.h>

G_DEFINE_TYPE (TpRoomListChannel, tp_room_list_channel, TP_TYPE_CHANNEL)

/**
 * TP_ROOM_LIST_CHANNEL_FEATURE_LISTING:
 *
 * Expands to a call to a function that returns a quark for the "listing"
 * feature on a #TpRoomListChannel.
 *
 * When this feature is prepared, the #TpRoomListChannel:listing property of the
 * Channel have been retrieved and is available for use.
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.UNRELEASED
 */
GQuark
tp_room_list_channel_get_feature_quark_listing (void)
{
  return g_quark_from_static_string ("tp-room-list-channel-feature-listing");
}

struct _TpRoomListChannelPrivate
{
  gboolean listing;
};

enum
{
  PROP_SERVER = 1,
  PROP_LISTING,
};

enum {
  SIG_GOT_ROOMS,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
tp_room_list_channel_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpRoomListChannel *self = (TpRoomListChannel *) object;

  switch (property_id)
    {
      case PROP_SERVER:
        g_value_set_string (value, tp_room_list_channel_get_server (self));
        break;

      case PROP_LISTING:
        g_value_set_boolean (value, self->priv->listing);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
got_rooms_cb (TpChannel *channel,
    const GPtrArray *rooms,
    gpointer user_data,
    GObject *weak_object)
{
  TpRoomListChannel *self = TP_ROOM_LIST_CHANNEL (channel);
  guint i;

  for (i = 0; i < rooms->len; i++)
    {
      TpRoomInfo *room;

      room = _tp_room_info_new (g_ptr_array_index (rooms, i));
      g_signal_emit (self, signals[SIG_GOT_ROOMS], 0, room);
      g_object_unref (room);
    }
}

static void
tp_room_list_channel_constructed (GObject *obj)
{
  TpChannel *channel = TP_CHANNEL (obj);
  GHashTable *props;
  const char *type;
  GError *error = NULL;

  props = tp_channel_borrow_immutable_properties (channel);
  g_assert (props != NULL);

  type = tp_asv_get_string (props, TP_PROP_CHANNEL_CHANNEL_TYPE);
  g_assert_cmpstr (type, ==, TP_IFACE_CHANNEL_TYPE_ROOM_LIST);

  if (tp_cli_channel_type_room_list_connect_to_got_rooms (channel,
        got_rooms_cb, NULL, NULL, NULL, &error) == NULL)
    {
      WARNING ("Failed to connect GotRooms signal: %s", error->message);
      g_error_free (error);
    }
}

enum {
    FEAT_LISTING,
    N_FEAT
};

static void
listing_rooms_cb (TpChannel *proxy,
    gboolean listing,
    gpointer user_data,
    GObject *weak_object)
{
  TpRoomListChannel *self = TP_ROOM_LIST_CHANNEL (proxy);

  if (self->priv->listing == listing)
    return;

  self->priv->listing = listing;
  g_object_notify (G_OBJECT (self), "listing");
}

static void
get_listing_rooms_cb (TpChannel *channel,
    gboolean in_progress,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpRoomListChannel *self = TP_ROOM_LIST_CHANNEL (channel);
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
    }
  else if (in_progress)
    {
      self->priv->listing = in_progress;
      g_object_notify (G_OBJECT (self), "listing");
    }

  g_simple_async_result_complete (result);
}

static void
tp_room_list_channel_prepare_listing_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpRoomListChannel *self = TP_ROOM_LIST_CHANNEL (proxy);
  TpChannel *channel = TP_CHANNEL (self);
  GError *error = NULL;
  GSimpleAsyncResult *result;

  tp_cli_channel_type_room_list_connect_to_listing_rooms (channel,
      listing_rooms_cb, proxy, NULL, G_OBJECT (proxy), &error);
  if (error != NULL)
    {
      g_simple_async_report_take_gerror_in_idle ((GObject *) self,
          callback, user_data, error);
      return;
    }

  result = g_simple_async_result_new ((GObject *) proxy, callback, user_data,
      tp_room_list_channel_prepare_listing_async);

  tp_cli_channel_type_room_list_call_get_listing_rooms (channel, -1,
      get_listing_rooms_cb, result, g_object_unref, G_OBJECT (channel));
}

static const TpProxyFeature *
tp_room_list_channel_list_features (TpProxyClass *cls G_GNUC_UNUSED)
{
  static TpProxyFeature features[N_FEAT + 1] = { { 0 } };

  if (G_LIKELY (features[0].name != 0))
    return features;

  features[FEAT_LISTING].name = TP_ROOM_LIST_CHANNEL_FEATURE_LISTING;
  features[FEAT_LISTING].prepare_async =
    tp_room_list_channel_prepare_listing_async;

  /* assert that the terminator at the end is there */
  g_assert (features[N_FEAT].name == 0);

  return features;
}

static void
tp_room_list_channel_class_init (TpRoomListChannelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  TpProxyClass *proxy_class = TP_PROXY_CLASS (klass);
  GParamSpec *param_spec;

  gobject_class->constructed = tp_room_list_channel_constructed;
  gobject_class->get_property = tp_room_list_channel_get_property;

  proxy_class->list_features = tp_room_list_channel_list_features;

  /**
   * TpRoomListChannel:server:
   *
   * The DNS name of the server whose rooms are listed by this channel, or
   * %NULL.
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_string ("server", "Server",
      "The server associated with the channel",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SERVER, param_spec);

  /**
   * TpRoomListChannel:listing:
   *
   * %TRUE if the channel is currently listing rooms.
   *
   * This property is meaningless until the
   * %TP_ROOM_LIST_CHANNEL_FEATURE_LISTING feature has been prepared.
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_boolean ("listing", "Listing",
      "TRUE if the channel is listing rooms",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_LISTING, param_spec);


  /**
   * TpRoomListChannel::got-rooms:
   * @self: a #TpRoomListChannel
   *
   * TODO
   *
   * Since: UNRELEASED
   */
  signals[SIG_GOT_ROOMS] = g_signal_new ("got-rooms",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL,
      G_TYPE_NONE,
      1, TP_TYPE_ROOM_INFO);

  g_type_class_add_private (gobject_class, sizeof (TpRoomListChannelPrivate));
}

static void
tp_room_list_channel_init (TpRoomListChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), TP_TYPE_ROOM_LIST_CHANNEL,
      TpRoomListChannelPrivate);
}

TpRoomListChannel *
_tp_room_list_channel_new (TpSimpleClientFactory *factory,
    TpConnection *conn,
    const gchar *object_path,
    const GHashTable *immutable_properties,
    GError **error)
{
  TpProxy *conn_proxy = (TpProxy *) conn;

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (object_path != NULL, NULL);
  g_return_val_if_fail (immutable_properties != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  return g_object_new (TP_TYPE_ROOM_LIST_CHANNEL,
      "factory", factory,
      "connection", conn,
      "dbus-daemon", conn_proxy->dbus_daemon,
      "bus-name", conn_proxy->bus_name,
      "object-path", object_path,
      "handle-type", (guint) TP_UNKNOWN_HANDLE_TYPE,
      "channel-properties", immutable_properties,
      NULL);
}

/**
 * tp_room_list_channel_get_server:
 * @self: a #TpRoomListChannel
 *
 * Return the #TpRoomListChannel:server property
 *
 * Returns: the value of #TpRoomListChannel:server property
 *
 * Since: UNRELEASED
 */
const gchar *
tp_room_list_channel_get_server (TpRoomListChannel *self)
{
  GHashTable *props;
  const gchar *server;

  props = tp_channel_borrow_immutable_properties (TP_CHANNEL (self));
  server = tp_asv_get_string (props, TP_PROP_CHANNEL_TYPE_ROOM_LIST_SERVER);

  return tp_str_empty (server) ? NULL : server;
}

/**
 * tp_room_list_channel_get_listing:
 * @self: a #TpRoomListChannel
 *
 * Return the #TpRoomListChannel:listing property
 *
 * Returns: the value of #TpRoomListChannel:listing property
 *
 * Since: UNRELEASED
 */
gboolean
tp_room_list_channel_get_listing (TpRoomListChannel *self)
{
  return self->priv->listing;
}

static void
list_rooms_cb (TpChannel *channel,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
    }

  g_simple_async_result_complete (result);
}

/**
 * tp_room_list_channel_start_listing_async:
 * @self: a #TpRoomListChannel
 * @callback: a callback to call when room listing have been started
 * @user_data: data to pass to @callback
 *
 * Start listing rooms using @self. Use the TpRoomListChannel::got-rooms
 * signal to get the rooms found.
 *
 * Since: UNRELEASED
 */
void
tp_room_list_channel_start_listing_async (TpRoomListChannel *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      tp_room_list_channel_start_listing_async);

  tp_cli_channel_type_room_list_call_list_rooms (TP_CHANNEL (self), -1,
      list_rooms_cb, result, g_object_unref, G_OBJECT (self));
}

/**
 * tp_room_list_channel_start_listing_finish:
 * @self: a #TpRoomListChannel
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * <!-- -->
 *
 * Returns: %TRUE if the room listing process has been started,
 * %FALSE otherwise.
 *
 * Since: UNRELEASED
 */
gboolean
tp_room_list_channel_start_listing_finish (TpRoomListChannel *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_room_list_channel_start_listing_async)
}
