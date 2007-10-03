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

#include "common/telepathy-errors.h"

#include "session.h"
#include "tp-stream-engine-signals-marshal.h"

G_DEFINE_TYPE (TpStreamEngineSession, tp_stream_engine_session, G_TYPE_OBJECT);

#define SESSION_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TP_STREAM_ENGINE_TYPE_SESSION, TpStreamEngineSessionPrivate))

typedef struct _TpStreamEngineSessionPrivate TpStreamEngineSessionPrivate;

struct _TpStreamEngineSessionPrivate
{
  gchar *bus_name;
  gchar *object_path;
  gchar *session_type;
  FarsightSession *fs_session;

  DBusGProxy *session_handler_proxy;
};

enum
{
  PROP_BUS_NAME = 1,
  PROP_OBJECT_PATH,
  PROP_SESSION_TYPE,
  PROP_FARSIGHT_SESSION
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

static void
tp_stream_engine_session_get_property (GObject    *object,
                                       guint       property_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (object);
  TpStreamEngineSessionPrivate *priv = SESSION_PRIVATE (self);

  switch (property_id)
    {
    case PROP_BUS_NAME:
      g_value_set_string (value, priv->bus_name);
      break;
    case PROP_OBJECT_PATH:
      g_value_set_string (value, priv->object_path);
      break;
    case PROP_SESSION_TYPE:
      g_value_set_string (value, priv->session_type);
      break;
    case PROP_FARSIGHT_SESSION:
      g_value_set_object (value, priv->fs_session);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_stream_engine_session_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (object);
  TpStreamEngineSessionPrivate *priv = SESSION_PRIVATE (self);

  switch (property_id)
    {
    case PROP_BUS_NAME:
      priv->bus_name = g_value_dup_string (value);
      break;
    case PROP_OBJECT_PATH:
      priv->object_path = g_value_dup_string (value);
      break;
    case PROP_SESSION_TYPE:
      priv->session_type = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void new_media_stream_handler (DBusGProxy *proxy,
    gchar *stream_handler_path, guint id, guint media_type, guint direction,
    gpointer user_data);

static void cb_fs_session_error (FarsightSession *stream,
    FarsightSessionError error, const gchar *debug, gpointer user_data);

static void dummy_callback (DBusGProxy *proxy, GError *error,
    gpointer user_data);

static void destroy_cb (DBusGProxy *proxy, gpointer user_data);

static GObject *
tp_stream_engine_session_constructor (GType type,
                                      guint n_props,
                                      GObjectConstructParam *props)
{
  GObject *obj;
  TpStreamEngineSessionPrivate *priv;

  obj = G_OBJECT_CLASS (tp_stream_engine_session_parent_class)->
           constructor (type, n_props, props);
  priv = SESSION_PRIVATE (obj);

  priv->fs_session = farsight_session_factory_make (priv->session_type);

  /* TODO: signal an error back to the connection manager */
  if (priv->fs_session == NULL)
    {
      g_warning ("requested session type was not found, session is unusable!");
      return obj;
    }

  g_debug ("plugin details:\n name: %s\n description: %s\n author: %s",
      farsight_plugin_get_name (priv->fs_session->plugin),
      farsight_plugin_get_description (priv->fs_session->plugin),
      farsight_plugin_get_author (priv->fs_session->plugin));

  priv->session_handler_proxy = dbus_g_proxy_new_for_name (tp_get_bus (),
      priv->bus_name,
      priv->object_path,
      TP_IFACE_MEDIA_SESSION_HANDLER);

  g_signal_connect (priv->session_handler_proxy, "destroy",
      G_CALLBACK (destroy_cb), obj);

  g_signal_connect (G_OBJECT (priv->fs_session), "error",
      G_CALLBACK (cb_fs_session_error), priv->session_handler_proxy);

  /* tell the proxy about the NewStreamHandler signal*/
  dbus_g_proxy_add_signal (priv->session_handler_proxy, "NewStreamHandler",
      DBUS_TYPE_G_OBJECT_PATH, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT,
      G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (priv->session_handler_proxy, "NewStreamHandler",
      G_CALLBACK (new_media_stream_handler), obj, NULL);

  g_debug ("calling MediaSessionHandler::Ready");

  tp_media_session_handler_ready_async (priv->session_handler_proxy,
      dummy_callback, "Media.SessionHandler::Ready");

  return obj;
}

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

  g_free (priv->bus_name);
  priv->bus_name = NULL;

  g_free (priv->object_path);
  priv->object_path = NULL;

  g_free (priv->session_type);
  priv->session_type = NULL;

  if (G_OBJECT_CLASS (tp_stream_engine_session_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_session_parent_class)->dispose (object);
}

static void
tp_stream_engine_session_class_init (TpStreamEngineSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (TpStreamEngineSessionPrivate));

  object_class->set_property = tp_stream_engine_session_set_property;
  object_class->get_property = tp_stream_engine_session_get_property;

  object_class->constructor = tp_stream_engine_session_constructor;

  object_class->dispose = tp_stream_engine_session_dispose;

  param_spec = g_param_spec_string ("bus-name",
                                    "session handler bus name",
                                    "D-Bus bus name for the session handler "
                                    "which this session interacts with.",
                                    NULL,
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_BUS_NAME, param_spec);

  param_spec = g_param_spec_string ("object-path",
                                    "session handler object path",
                                    "D-Bus object path of the session handler "
                                    "which this session interacts with.",
                                    NULL,
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH, param_spec);

  param_spec = g_param_spec_string ("session-type",
                                    "Farsight session type",
                                    "Name of the Farsight session type this "
                                    "session will create.",
                                    NULL,
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_SESSION_TYPE,
      param_spec);

  param_spec = g_param_spec_object ("farsight-session",
                                    "Farsight session",
                                    "The Farsight session which streams "
                                    "should be created within.",
                                    FARSIGHT_TYPE_SESSION,
                                    G_PARAM_READABLE |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_FARSIGHT_SESSION,
      param_spec);

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

TpStreamEngineSession *
tp_stream_engine_session_new (const gchar *bus_name,
                              const gchar *object_path,
                              const gchar *session_type,
                              GError **error)
{
  TpStreamEngineSession *ret;
  TpStreamEngineSessionPrivate *priv;

  g_return_val_if_fail (bus_name != NULL, NULL);
  g_return_val_if_fail (object_path != NULL, NULL);
  g_return_val_if_fail (session_type != NULL, NULL);

  ret = g_object_new (TP_STREAM_ENGINE_TYPE_SESSION,
      "bus-name", bus_name,
      "object-path", object_path,
      "session-type", session_type,
      NULL);

  priv = SESSION_PRIVATE (ret);

  if (priv->fs_session == NULL)
    {
      g_set_error (error, TELEPATHY_ERRORS, NotAvailable,
          "requested session type not found");
      g_object_unref (ret);
      return NULL;
    }

  return ret;
}

