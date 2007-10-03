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

#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-interfaces.h>
#include <libtelepathy/tp-media-session-handler-gen.h>

#include <farsight/farsight-session.h>
#include <farsight/farsight-codec.h>

#include "session.h"
#include "tp-stream-engine-signals-marshal.h"

G_DEFINE_TYPE (TpStreamEngineSession, tp_stream_engine_session, G_TYPE_OBJECT);

#define SESSION_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TP_STREAM_ENGINE_TYPE_SESSION, TpStreamEngineSessionPrivate))

typedef struct _TpStreamEngineSessionPrivate TpStreamEngineSessionPrivate;

struct _TpStreamEngineSessionPrivate
{
  DBusGProxy *session_handler_proxy;

  FarsightSession *fs_session;
};

enum
{
  NEW_STREAM,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = { 0 };

static void
tp_stream_engine_session_init (TpStreamEngineSession *self)
{
}

static void new_media_stream_handler (DBusGProxy *proxy,
    gchar *stream_handler_path, guint id, guint media_type, guint direction,
    gpointer user_data);

static void destroy_cb (DBusGProxy *proxy, gpointer user_data);

static void
tp_stream_engine_session_dispose (GObject *object)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (object);
  TpStreamEngineSessionPrivate *priv = SESSION_PRIVATE (self);

  g_debug (G_STRFUNC);

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

  signals[NEW_STREAM] =
    g_signal_new ("new-stream",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  tp_stream_engine_marshal_VOID__BOXED_UINT_UINT_UINT,
                  G_TYPE_NONE, 4,
                  DBUS_TYPE_G_OBJECT_PATH, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
}

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
cb_fs_session_error (FarsightSession *session,
                     FarsightSessionError error,
                     const gchar *debug,
                     gpointer user_data)
{
  DBusGProxy *session_handler_proxy = (DBusGProxy *) user_data;

  g_message (
    "%s: session error: session=%p error=%s\n", G_STRFUNC, session, debug);
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
new_media_stream_handler (DBusGProxy *proxy,
                          gchar *object_path,
                          guint stream_id,
                          guint media_type,
                          guint direction,
                          gpointer user_data)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (user_data);

  g_debug ("New stream, stream_id=%d, media_type=%d, direction=%d",
      stream_id, media_type, direction);

  g_signal_emit (self, signals[NEW_STREAM], 0, object_path, stream_id,
      media_type, direction);
}

gboolean
tp_stream_engine_session_go (
  TpStreamEngineSession *self,
  const gchar *bus_name,
  const gchar *session_handler_path,
  const gchar *type)
{
  TpStreamEngineSessionPrivate *priv = SESSION_PRIVATE (self);

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

