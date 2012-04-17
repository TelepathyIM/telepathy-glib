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
#include <telepathy-glib/room-info-internal.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/util-internal.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "telepathy-glib/debug-internal.h"

#include <stdio.h>
#include <glib/gstdio.h>

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (TpRoomListChannel, tp_room_list_channel, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

struct _TpRoomListChannelPrivate
{
  TpAccount *account;
  gchar *server;
  gboolean listing;

  TpChannel *channel;

  GSimpleAsyncResult *async_res;
};

enum
{
  PROP_ACCOUNT = 1,
  PROP_SERVER,
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
      case PROP_ACCOUNT:
        g_value_set_object (value, tp_room_list_channel_get_account (self));
        break;

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
tp_room_list_channel_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpRoomListChannel *self = (TpRoomListChannel *) object;

  switch (property_id)
    {
      case PROP_ACCOUNT:
        g_assert (self->priv->account == NULL); /* construct only */
        self->priv->account = g_value_dup_object (value);
        break;

      case PROP_SERVER:
        g_assert (self->priv->server == NULL); /* construct only */
        self->priv->server = g_value_dup_string (value);
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
  TpRoomListChannel *self = TP_ROOM_LIST_CHANNEL (weak_object);
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
listing_rooms_cb (TpChannel *proxy,
    gboolean listing,
    gpointer user_data,
    GObject *weak_object)
{
  TpRoomListChannel *self = TP_ROOM_LIST_CHANNEL (weak_object);

  if (self->priv->listing == listing)
    return;

  self->priv->listing = listing;
  g_object_notify (G_OBJECT (self), "listing");
}

static void
destroy_channel (TpRoomListChannel *self)
{
  if (self->priv->channel == NULL)
    return;

  DEBUG ("Destroying existing RoomList channel");

  tp_channel_destroy_async (self->priv->channel, NULL, NULL);
  tp_clear_object (&self->priv->channel);
}

static void
tp_room_list_channel_dispose (GObject *object)
{
  TpRoomListChannel *self = TP_ROOM_LIST_CHANNEL (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) tp_room_list_channel_parent_class)->dispose;

  destroy_channel (self);
  g_clear_object (&self->priv->account);

  if (chain_up != NULL)
    chain_up (object);
}

static void
tp_room_list_channel_finalize (GObject *object)
{
  TpRoomListChannel *self = TP_ROOM_LIST_CHANNEL (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) tp_room_list_channel_parent_class)->finalize;

  g_free (self->priv->server);

  if (chain_up != NULL)
    chain_up (object);
}

static void
tp_room_list_channel_class_init (TpRoomListChannelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  gobject_class->get_property = tp_room_list_channel_get_property;
  gobject_class->set_property = tp_room_list_channel_set_property;
  gobject_class->dispose = tp_room_list_channel_dispose;
  gobject_class->finalize = tp_room_list_channel_finalize;

  /**
   * TpRoomListChannel:account:
   *
   * The #TpAccount to use for the room listing.
   *
   * Since: UNRELEASED
   */
  param_spec = g_param_spec_object ("account", "account",
      "TpAccount",
      TP_TYPE_ACCOUNT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_ACCOUNT, param_spec);

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
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
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

/**
 * tp_room_list_channel_get_account:
 * @self: a #TpRoomListChannel
 *
 * Return the #TpRoomListChannel:account property
 *
 * Returns: (transfer none): the value of #TpRoomListChannel:account property
 *
 * Since: UNRELEASED
 */
TpAccount *
tp_room_list_channel_get_account (TpRoomListChannel *self)
{
  return self->priv->account;
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
  return self->priv->server;
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

  g_return_if_fail (self->priv->channel != NULL);

  result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      tp_room_list_channel_start_listing_async);

  tp_cli_channel_type_room_list_call_list_rooms (self->priv->channel, -1,
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

static void
create_channel_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpAccountChannelRequest *channel_request = TP_ACCOUNT_CHANNEL_REQUEST (
      source_object);
  TpRoomListChannel *self = user_data;
  GHashTable *properties;
  GError *error = NULL;
  const gchar *server;

  self->priv->channel =
      tp_account_channel_request_create_and_handle_channel_finish (
          channel_request, result, NULL, &error);

  if (self->priv->channel == NULL)
    {
      DEBUG ("Failed to create RoomList channel: %s", error->message);
      goto out;
    }

  DEBUG ("Got channel: %s", tp_proxy_get_object_path (self->priv->channel));

  if (tp_cli_channel_type_room_list_connect_to_got_rooms (self->priv->channel,
        got_rooms_cb, NULL, NULL, G_OBJECT (self), &error) == NULL)
    {
      DEBUG ("Failed to connect GotRooms signal: %s", error->message);
      goto out;
    }

  tp_cli_channel_type_room_list_connect_to_listing_rooms (self->priv->channel,
      listing_rooms_cb, NULL, NULL, G_OBJECT (self), &error);
  if (error != NULL)
    {
      DEBUG ("Failed to connect ListingRooms signal: %s", error->message);
      goto out;
    }

  properties = tp_channel_borrow_immutable_properties (self->priv->channel);

  server = tp_asv_get_string (properties,
      TP_PROP_CHANNEL_TYPE_ROOM_LIST_SERVER);
  if (tp_strdiff (server, self->priv->server))
    {
      if (tp_str_empty (server) && self->priv->server != NULL)
        {
          g_free (self->priv->server);
          self->priv->server = g_strdup (server);
          g_object_notify (G_OBJECT (self), "server");
        }
    }

 out:
  if (error != NULL)
    {
      g_simple_async_result_set_from_error (self->priv->async_res, error);
      g_error_free (error);
      /* This function is safe if self->priv->channel is NULL. */
      destroy_channel (self);
    }

  g_simple_async_result_complete (self->priv->async_res);
  tp_clear_object (&self->priv->async_res);
}

static void
open_new_channel (TpRoomListChannel *self)
{
  GHashTable *request;
  TpAccountChannelRequest *channel_request;

  DEBUG ("Requesting new RoomList channel");

  request = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
        TP_IFACE_CHANNEL_TYPE_ROOM_LIST,
      NULL);

  if (self->priv->server != NULL)
    tp_asv_set_string (request, TP_PROP_CHANNEL_TYPE_ROOM_LIST_SERVER,
        self->priv->server);

  channel_request = tp_account_channel_request_new (self->priv->account,
      request, TP_USER_ACTION_TIME_NOT_USER_ACTION);

  tp_account_channel_request_create_and_handle_channel_async (channel_request,
      NULL, create_channel_cb, G_OBJECT (self));
  g_object_unref (channel_request);

  g_hash_table_unref (request);
}

static void
room_list_channel_init_async (GAsyncInitable *initable,
    gint io_priority,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpRoomListChannel *self = TP_ROOM_LIST_CHANNEL (initable);

  self->priv->async_res = g_simple_async_result_new (G_OBJECT (self),
      callback, user_data, tp_room_list_channel_new_async);

  open_new_channel (self);
}

static gboolean
room_list_channel_init_finish (GAsyncInitable *initable,
    GAsyncResult *res,
    GError **error)
{
  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res),
          error))
    return FALSE;

  return TRUE;
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = room_list_channel_init_async;
  iface->init_finish = room_list_channel_init_finish;
}

/**
 * tp_room_list_channel_new_async:
 * @account: a #TpAccount for the room listing
 * @server: the DNS name of the server whose rooms should listed
 * @callback: a #GAsyncReadyCallback to call when the initialization
 * is finished
 * @user_data: data to pass to the callback function
 *
 * <!-- -->
 *
 * Since: UNRELEASED
 */
void
tp_room_list_channel_new_async (TpAccount *account,
    const gchar *server,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  g_return_if_fail (TP_IS_ACCOUNT (account));

  g_async_initable_new_async (TP_TYPE_ROOM_LIST_CHANNEL,
      G_PRIORITY_DEFAULT, NULL, callback, user_data,
      "account", account,
      "server", server,
      NULL);
}

/**
 * tp_room_list_channel_new_finish:
 * @result: the #GAsyncResult from the callback
 * @error: a #GError location to store an error, or %NULL
 *
 * <!-- -->
 *
 * Returns: (transfer full): a new #TpRoomListChannel object, or %NULL
 * in case of error.
 *
 * Since: UNRELEASED
 */
TpRoomListChannel *
tp_room_list_channel_new_finish (GAsyncResult *result,
    GError **error)
{
  GObject *object, *source_object;

  source_object = g_async_result_get_source_object (result);

  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
      result, error);
  g_object_unref (source_object);

  if (object != NULL)
    return TP_ROOM_LIST_CHANNEL (object);
  else
    return NULL;
}
