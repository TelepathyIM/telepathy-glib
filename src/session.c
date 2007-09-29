/*
 * session.c - Source for TpStreamEngineSession
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

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-chan-type-streamed-media-gen.h>
#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-interfaces.h>
#include <libtelepathy/tp-media-session-handler-gen.h>

#include <farsight/farsight-session.h>
#include <farsight/farsight-codec.h>

#include "stream.h"
#include "types.h"

#include "session.h"

G_DEFINE_TYPE (TpStreamEngineSession, tp_stream_engine_session, G_TYPE_OBJECT);

#define SESSION_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TP_STREAM_ENGINE_TYPE_SESSION, TpStreamEngineSessionPrivate))

typedef struct _TpStreamEngineSessionPrivate TpStreamEngineSessionPrivate;

struct _TpStreamEngineSessionPrivate
{
  DBusGProxy *session_handler_proxy;

  FarsightSession *fs_session;

  const TpStreamEngineNatProperties *nat_props;

  gchar *connection_path;
  gchar *channel_path;
};

/* dummy callback handler for async calling calls with no return values */
static void
dummy_callback (DBusGProxy *proxy, GError *error, gpointer user_data)
{
  if (error)
    {
      g_warning ("Error calling %s: %s", (gchar *) user_data, error->message);
      g_error_free (error);
    }
}

static void
cb_fs_session_error (
  FarsightSession *stream,
  FarsightSessionError error,
  const gchar *debug,
  gpointer user_data)
{
  DBusGProxy *session_handler_proxy = (DBusGProxy *) user_data;

  g_message (
    "%s: session error: session=%p error=%s\n", G_STRFUNC, stream, debug);
  tp_media_session_handler_error_async (
    session_handler_proxy, error, debug, dummy_callback,
    "Media.SessionHandler::Error");
}

static void
destroy_cb (DBusGProxy *proxy, gpointer user_data)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (user_data);
  TpStreamEngineSessionPrivate *priv = SESSION_PRIVATE (self);

  if (priv->session_handler_proxy)
    {
      DBusGProxy *tmp;

      tmp = priv->session_handler_proxy;
      priv->session_handler_proxy = NULL;
      g_object_unref (tmp);
    }
}

static void
new_media_stream_handler (DBusGProxy *proxy, gchar *stream_handler_path,
                          guint id, guint media_type, guint direction,
                          gpointer user_data);

static void
tp_stream_engine_session_dispose (GObject *object)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (object);
  TpStreamEngineSessionPrivate *priv = SESSION_PRIVATE (self);

  g_debug (G_STRFUNC);

  if (self->streams)
    {
      guint i;

      for (i = 0; i < self->streams->len; i++)
        g_object_unref (g_ptr_array_index (self->streams, i));

      g_ptr_array_free (self->streams, TRUE);
      self->streams = NULL;
    }

  if (priv->channel_path)
    {
      g_free (priv->channel_path);
      priv->channel_path = NULL;
    }

  if (priv->connection_path)
    {
      g_free (priv->connection_path);
      priv->connection_path = NULL;
    }

  if (priv->session_handler_proxy)
    {
      DBusGProxy *tmp;

      g_debug ("%s: disconnecting signals from session handler proxy",
        G_STRFUNC);

      dbus_g_proxy_disconnect_signal (
          priv->session_handler_proxy, "NewStreamHandler",
          G_CALLBACK (new_media_stream_handler), self);

      g_signal_handlers_disconnect_by_func (
          priv->session_handler_proxy, destroy_cb, self);

      tmp = priv->session_handler_proxy;
      priv->session_handler_proxy = NULL;
      g_object_unref (tmp);
    }

  if (priv->fs_session)
    {
      g_object_unref (priv->fs_session);
      priv->fs_session = NULL;
    }

  if (G_OBJECT_CLASS (tp_stream_engine_session_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_session_parent_class)->dispose (object);
}

static void
tp_stream_engine_session_class_init (TpStreamEngineSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpStreamEngineSessionPrivate));

  object_class->dispose = tp_stream_engine_session_dispose;
}

static void
tp_stream_engine_session_init (TpStreamEngineSession *self)
{
  self->streams = g_ptr_array_new ();
}

static void
cb_stream_error (TpStreamEngineStream *stream, gpointer user_data)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (user_data);

  g_object_unref (stream);
  g_ptr_array_remove_fast (self->streams, stream);
}

static void
cb_stream_closed (TpStreamEngineStream *stream, gpointer user_data)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (user_data);

  g_object_unref (stream);
  g_ptr_array_remove_fast (self->streams, stream);
}

static void
new_media_stream_handler (DBusGProxy *proxy, gchar *stream_handler_path,
                          guint id, guint media_type, guint direction,
                          gpointer user_data)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (user_data);
  TpStreamEngineSessionPrivate *priv = SESSION_PRIVATE (self);
  TpStreamEngineStream *stream;
  gchar *bus_name;

  g_debug ("Adding stream, media_type=%d, direction=%d",
      media_type, direction);

  g_object_get (priv->session_handler_proxy, "name", &bus_name, NULL);

  stream = g_object_new (TP_STREAM_ENGINE_TYPE_STREAM, NULL);

  g_signal_connect (stream, "stream-error", G_CALLBACK (cb_stream_error), self);
  g_signal_connect (stream, "stream-closed", G_CALLBACK (cb_stream_closed), self);

  if (tp_stream_engine_stream_go (
        stream,
        bus_name,
        stream_handler_path,
        priv->channel_path,
        priv->fs_session,
        id,
        media_type,
        direction,
        priv->nat_props))
    {
      g_ptr_array_add (self->streams, stream);
    }
  else
    {
      g_object_unref (stream);
    }

  g_free (bus_name);
}

gboolean
tp_stream_engine_session_go (
  TpStreamEngineSession *self,
  const gchar *bus_name,
  const gchar *connection_path,
  const gchar *session_handler_path,
  const gchar *channel_path,
  const gchar *type,
  const TpStreamEngineNatProperties *nat_props)
{
  TpStreamEngineSessionPrivate *priv = SESSION_PRIVATE (self);

  priv->connection_path = g_strdup (connection_path);

  priv->channel_path = g_strdup (channel_path);

  priv->nat_props = nat_props;

  priv->session_handler_proxy = dbus_g_proxy_new_for_name (tp_get_bus(),
    bus_name,
    session_handler_path,
    TP_IFACE_MEDIA_SESSION_HANDLER);

  g_signal_connect (priv->session_handler_proxy, "destroy",
      G_CALLBACK (destroy_cb), self);

  if (!priv->session_handler_proxy)
    {
      g_critical ("couldn't get proxy for session");
      return FALSE;
    }

  /* tell the proxy about the NewStreamHandler signal*/
  dbus_g_proxy_add_signal (priv->session_handler_proxy,
      "NewStreamHandler", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_UINT,
      G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (priv->session_handler_proxy,
      "NewStreamHandler", G_CALLBACK (new_media_stream_handler), self,
      NULL);

  priv->fs_session = farsight_session_factory_make (type);

  if (!priv->fs_session)
    {
      g_error ("RTP plugin not found");
      return FALSE;
    }

  g_debug ("plugin details:\n name: %s\n description: %s\n author: %s\n",
           farsight_plugin_get_name (priv->fs_session->plugin),
           farsight_plugin_get_description (priv->fs_session->plugin),
           farsight_plugin_get_author (priv->fs_session->plugin));

  g_signal_connect (G_OBJECT (priv->fs_session), "error",
                    G_CALLBACK (cb_fs_session_error),
                    priv->session_handler_proxy);

  g_debug ("Calling MediaSessionHandler::Ready -->");
  tp_media_session_handler_ready_async (
    priv->session_handler_proxy, dummy_callback,
    "Media.SessionHandler::Ready");
  g_debug ("<-- Returned from MediaSessionHandler::Ready");

  return TRUE;
}

TpStreamEngineSession*
tp_stream_engine_session_new (void)
{
  return g_object_new (TP_STREAM_ENGINE_TYPE_SESSION, NULL);
}

