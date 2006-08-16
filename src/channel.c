/*
 * channel.c - Source for TpStreamEngineChannel
 * Copyright (C) 2006 Collabora Ltd.
 * Copyright (C) 2006 Nokia Corporation
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

#include <stdlib.h>

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-constants.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-chan-type-streamed-media-gen.h>
#include <libtelepathy/tp-helpers.h>

#include "common/telepathy-errors.h"

#include "tp-media-session-handler-gen.h"
#include "tp-media-stream-handler-gen.h"

#include "types.h"
#include "session.h"

#include "channel.h"

G_DEFINE_TYPE (TpStreamEngineChannel, tp_stream_engine_channel, G_TYPE_OBJECT);

#define CHANNEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TP_STREAM_ENGINE_TYPE_CHANNEL, TpStreamEngineChannelPrivate))

typedef struct _TpStreamEngineChannelPrivate TpStreamEngineChannelPrivate;

struct _TpStreamEngineChannelPrivate
{
  GPtrArray *sessions;

  TpChan *channel_proxy;
  DBusGProxy *streamed_media_proxy;

  gulong channel_destroy_handler;

  guint output_volume;
  gboolean output_mute;
  gboolean input_mute;

  gchar *connection_path;
};

enum
{
  CLOSED,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

static void
tp_stream_engine_channel_dispose (GObject *object)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (object);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  g_debug (G_STRFUNC);

  if (priv->sessions)
    {
      guint i;

      for (i = 0; i < priv->sessions->len; i++)
        g_object_unref (g_ptr_array_index (priv->sessions, i));

      g_ptr_array_free (priv->sessions, TRUE);
      priv->sessions = NULL;
    }

  if (self->channel_path)
    {
      g_free (self->channel_path);
      self->channel_path = NULL;
    }

  if (priv->connection_path)
    {
      g_free (priv->connection_path);
      priv->connection_path = NULL;
    }

  if (priv->channel_proxy)
    {
      g_object_unref (priv->channel_proxy);
      priv->channel_proxy = NULL;
    }

  if (G_OBJECT_CLASS (tp_stream_engine_channel_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_channel_parent_class)->dispose (object);
}

static void
tp_stream_engine_channel_class_init (TpStreamEngineChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpStreamEngineChannelPrivate));

  object_class->dispose = tp_stream_engine_channel_dispose;

  signals[CLOSED] =
    g_signal_new ("closed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
tp_stream_engine_channel_init (TpStreamEngineChannel *self)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  priv->sessions = g_ptr_array_new ();

  /* sensible default */
  priv->output_volume = (65535 * 7) / 10;
}

static void
add_session (TpStreamEngineChannel *self,
             guint member,
             const gchar *session_handler_path,
             const gchar *type)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  TpStreamEngineSession *session;
  gchar *bus_name;

  g_debug("adding session for member %d, %s, %s", member, session_handler_path,
    type);

  g_object_get (priv->channel_proxy, "name", &bus_name, NULL);

  session = g_object_new (TP_STREAM_ENGINE_TYPE_SESSION, NULL);

  if (!tp_stream_engine_session_go (session, bus_name, priv->connection_path,
      session_handler_path, type))
    {
      g_critical ("couldn't create session");
    }

  g_ptr_array_add (priv->sessions, session);

  g_free (bus_name);
}

static void
new_media_session_handler (DBusGProxy *proxy, guint member, const char *session_handler_path, const gchar* type, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  add_session (self, member, session_handler_path, type);
}

static void channel_closed (DBusGProxy *proxy, gpointer user_data);

static void
shutdown_channel (TpStreamEngineChannel *self, gboolean destroyed)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  g_signal_handler_disconnect (
    priv->channel_proxy, priv->channel_destroy_handler);

  if (!destroyed)
    {
      if (priv->streamed_media_proxy)
        {
          g_debug ("%s: disconnecting signals from streamed_media_proxy",
              G_STRFUNC);

          dbus_g_proxy_disconnect_signal (
              DBUS_G_PROXY (priv->streamed_media_proxy),
              "NewMediaSessionHandler", G_CALLBACK (new_media_session_handler),
              self);
        }

      if (priv->channel_proxy)
        {
          g_debug ("%s: disconnecting signals from channel_proxy", G_STRFUNC);

          dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->channel_proxy),
              "Closed", G_CALLBACK (channel_closed), self);
        }
    }

  g_signal_emit (self, signals[CLOSED], 0);
}

static void
channel_closed (DBusGProxy *proxy, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);

  g_debug ("connection manager channel closed");

  shutdown_channel (self, FALSE);
}

static void
channel_destroyed (DBusGProxy *proxy, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);

  g_debug ("connection manager channel destroyed");

  shutdown_channel (self, TRUE);
}

void
get_session_handlers_reply (DBusGProxy *proxy, GPtrArray *session_handlers, GError *error, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  GValueArray *session;
  int i;
  if (error)
    g_critical ("Error calling GetSessionHandlers: %s", error->message);

  g_debug ("GetSessionHandlers replied: ");
  for (i = 0; i < session_handlers->len; i++)
    {
      session = g_ptr_array_index (session_handlers, i);
      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (session, 0)));
      g_assert(G_VALUE_TYPE (g_value_array_get_nth (session, 1)) == DBUS_TYPE_G_OBJECT_PATH);
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (session, 2)));

      add_session (self,
          g_value_get_uint (g_value_array_get_nth (session, 0)),
          g_value_get_boxed (g_value_array_get_nth (session, 1)),
          g_value_get_string (g_value_array_get_nth (session, 2)));
    }
}

gboolean
tp_stream_engine_channel_go (
  TpStreamEngineChannel *self,
  const gchar *bus_name,
  const gchar *connection_path,
  const gchar *channel_path,
  guint handle_type,
  guint handle,
  GError **error)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  g_assert (NULL == priv->channel_proxy);

  self->channel_path = g_strdup (channel_path);

  priv->connection_path = g_strdup (connection_path);

  priv->channel_proxy = tp_chan_new (
    tp_get_bus(),                              /* connection  */
    bus_name,                                  /* bus_name    */
    channel_path,                              /* object_name */
    TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,      /* type        */
    handle_type,                               /* handle_type */
    handle);                                   /* handle      */

  if (!priv->channel_proxy)
    {
      *error = g_error_new (TELEPATHY_ERRORS, NotAvailable,
                            "Unable to bind to channel");
      return FALSE;
    }

  /* TODO: check for group interface
  chan_interfaces = (GSList *) tp_chan_local_get_interface_objs(priv->chan);

  if (chan_interfaces == NULL)
  {
    g_error("Channel does not have interfaces.");
    exit(1);
  }
  */

  priv->channel_destroy_handler = g_signal_connect (
    priv->channel_proxy, "destroy", G_CALLBACK (channel_destroyed), self);

  dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->channel_proxy), "Closed",
                               G_CALLBACK (channel_closed),
                               self, NULL);

  priv->streamed_media_proxy = tp_chan_get_interface (priv->channel_proxy,
      TELEPATHY_CHAN_IFACE_STREAMED_QUARK);

  if (!priv->streamed_media_proxy)
    {
      *error = g_error_new (TELEPATHY_ERRORS, NotAvailable,
                            "Channel doesn't have StreamedMedia interface");
      return FALSE;
    }

  dbus_g_proxy_add_signal (DBUS_G_PROXY (priv->streamed_media_proxy),
      "NewMediaSessionHandler", G_TYPE_UINT, DBUS_TYPE_G_OBJECT_PATH,
      G_TYPE_STRING, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->streamed_media_proxy),
      "NewMediaSessionHandler", G_CALLBACK (new_media_session_handler),
      self, NULL);

  tp_chan_type_streamed_media_get_session_handlers_async (
        DBUS_G_PROXY (priv->streamed_media_proxy),
        get_session_handlers_reply, self);

  return TRUE;
}

TpStreamEngineChannel*
tp_stream_engine_channel_new (void)
{
  return g_object_new (TP_STREAM_ENGINE_TYPE_CHANNEL, NULL);
}

void tp_stream_engine_channel_mute_input (
  TpStreamEngineChannel *self,
  gboolean mute_state)
{
  /*
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  GstElement *source;
  priv->input_mute = mute_state;

  g_message ("%s: input mute set to %s", G_STRFUNC, mute_state?"on":"off");

  if (priv->fs_stream &&
      farsight_stream_get_state (priv->fs_stream) ==
        FARSIGHT_STREAM_STATE_PLAYING)
    {
      source = farsight_stream_get_source (priv->fs_stream);
      if (source)
        g_object_set (G_OBJECT (source), "mute", mute_state, NULL);
    }
  */
}

void tp_stream_engine_channel_mute_output (
  TpStreamEngineChannel *self,
  gboolean mute_state)
{
  /*
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  GstElement *sink;
  priv->output_mute = mute_state;

  g_message ("%s: output mute set to %s", G_STRFUNC, mute_state?"on":"off");

  if (priv->fs_stream &&
      farsight_stream_get_state (priv->fs_stream) ==
        FARSIGHT_STREAM_STATE_PLAYING)
    {
      sink = farsight_stream_get_sink (priv->fs_stream);
      if (sink)
        g_object_set (G_OBJECT (sink), "mute", mute_state, NULL);
    }
  */
}

void tp_stream_engine_channel_set_output_volume (
  TpStreamEngineChannel *self,
  guint volume)
{
  /*
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  GstElement *sink;

  if (volume > 100) volume=100;

  priv->output_volume = (volume * 65535)/100;
  g_debug ("%s: Setting output volume to %d", G_STRFUNC, priv->output_volume);
  if (priv->fs_stream &&
      farsight_stream_get_state (priv->fs_stream) ==
        FARSIGHT_STREAM_STATE_PLAYING)
    {
      sink = farsight_stream_get_sink (priv->fs_stream);
      if (sink)
        {
          g_debug ("Setting volume to %d", priv->output_volume);
          g_object_set (G_OBJECT (sink), "volume", priv->output_volume, NULL);
          g_message ("Finished setting volume to %d", priv->output_volume);
        }
    }
  */
}

void tp_stream_engine_channel_error (
  TpStreamEngineChannel *self,
  guint error,
  const gchar *message)
{
  /*
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  org_freedesktop_Telepathy_Media_StreamHandler_error_async (
    priv->stream_proxy, error, message, dummy_callback,
    "Media.StreamHandler::Error");
  shutdown_channel (self, FALSE);
  */
}

