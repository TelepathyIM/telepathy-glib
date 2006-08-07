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

#include <farsight/farsight-session.h>
#include <farsight/farsight-codec.h>

#include "tp-media-session-handler-gen.h"

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
  TpStreamEngineStream *stream;

  FarsightSession *fs_session;

  gchar *connection_path;
};

/* dummy callback handler for async calling calls with no return values */
static void
dummy_callback (DBusGProxy *proxy, GError *error, gpointer user_data)
{
  if (error)
    g_critical ("%s calling %s", error->message, (char*)user_data);
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
  org_freedesktop_Telepathy_Media_SessionHandler_error_async (
    session_handler_proxy, error, debug, dummy_callback,
    "Media.SessionHandler::Error");
}

static void
new_media_stream_handler (DBusGProxy *proxy, gchar *stream_handler_path,
                          guint media_type, guint direction,
                          gpointer user_data);

static void
tp_stream_engine_session_dispose (GObject *object)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (object);
  TpStreamEngineSessionPrivate *priv = SESSION_PRIVATE (self);

  g_debug (G_STRFUNC);

  if (priv->stream)
    {
      g_object_unref (priv->stream);
      priv->stream = NULL;
    }

  if (priv->connection_path)
    {
      g_free (priv->connection_path);
      priv->connection_path = NULL;
    }

  if (priv->session_handler_proxy)
    {
      g_debug ("%s: disconnecting signals from session handler proxy",
        G_STRFUNC);

      dbus_g_proxy_disconnect_signal (
          priv->session_handler_proxy, "NewMediaStreamHandler",
          G_CALLBACK (new_media_stream_handler), self);

      g_object_unref (priv->session_handler_proxy);
      priv->session_handler_proxy = NULL;
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
}

static void
new_media_stream_handler (DBusGProxy *proxy, gchar *stream_handler_path,
                          guint media_type, guint direction,
                          gpointer user_data)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (user_data);
  TpStreamEngineSessionPrivate *priv = SESSION_PRIVATE (self);
  gchar *bus_name;

  g_debug ("Adding stream, media_type=%d, direction=%d",
      media_type, direction);

  if (priv->stream)
    {
      g_warning("already allocated the one supported stream.");
      return;
    }

  g_object_get (priv->session_handler_proxy, "name", &bus_name, NULL);

  priv->stream = g_object_new (TP_STREAM_ENGINE_TYPE_STREAM, NULL);

  /* FIXME: connect to stream-error signal here */

  tp_stream_engine_stream_go (
    priv->stream,
    bus_name,
    priv->connection_path,
    stream_handler_path,
    priv->fs_session,
    media_type,
    direction);

  g_free (bus_name);
}

gboolean
tp_stream_engine_session_go (
  TpStreamEngineSession *self,
  const gchar *bus_name,
  const gchar *connection_path,
  const gchar *session_handler_path,
  const gchar *type)
{
  TpStreamEngineSessionPrivate *priv = SESSION_PRIVATE (self);

  priv->connection_path = g_strdup (connection_path);

  priv->session_handler_proxy = dbus_g_proxy_new_for_name (tp_get_bus(),
    bus_name,
    session_handler_path,
    TP_IFACE_MEDIA_SESSION_HANDLER);

  if (!priv->session_handler_proxy)
    {
      g_critical ("couldn't get proxy for session");
      return FALSE;
    }

  /* tell the proxy about the NewMediaStreamHandler signal*/
  dbus_g_proxy_add_signal (priv->session_handler_proxy,
      "NewMediaStreamHandler", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_UINT,
      G_TYPE_UINT, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (priv->session_handler_proxy,
      "NewMediaStreamHandler", G_CALLBACK (new_media_stream_handler), self,
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
  org_freedesktop_Telepathy_Media_SessionHandler_ready_async (
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

