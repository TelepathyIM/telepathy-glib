/*
 * session.c - Source for TpStreamEngineSession
 * Copyright (C) 2006-2007 Collabora Ltd.
 * Copyright (C) 2006-2007 Nokia Corporation
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

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/interfaces.h>

#include <farsight/farsight-session.h>
#include <farsight/farsight-codec.h>

#include "session.h"
#include "tp-stream-engine-signals-marshal.h"

G_DEFINE_TYPE (TpStreamEngineSession, tp_stream_engine_session, G_TYPE_OBJECT);

struct _TpStreamEngineSessionPrivate
{
  gchar *session_type;
  FarsightSession *fs_session;

  TpMediaSessionHandler *session_handler_proxy;
};

enum
{
  PROP_PROXY = 1,
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
  TpStreamEngineSessionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_STREAM_ENGINE_TYPE_SESSION, TpStreamEngineSessionPrivate);

  self->priv = priv;
}

static void
tp_stream_engine_session_get_property (GObject    *object,
                                       guint       property_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (object);

  switch (property_id)
    {
    case PROP_SESSION_TYPE:
      g_value_set_string (value, self->priv->session_type);
      break;
    case PROP_FARSIGHT_SESSION:
      g_value_set_object (value, self->priv->fs_session);
      break;
    case PROP_PROXY:
      g_value_set_object (value, self->priv->session_handler_proxy);
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

  switch (property_id)
    {
    case PROP_SESSION_TYPE:
      self->priv->session_type = g_value_dup_string (value);
      break;
    case PROP_PROXY:
      self->priv->session_handler_proxy =
          TP_MEDIA_SESSION_HANDLER (g_value_dup_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void new_media_stream_handler (TpMediaSessionHandler *proxy,
    const gchar *stream_handler_path, guint id, guint media_type,
    guint direction, gpointer user_data, GObject *object);

static void cb_fs_session_error (FarsightSession *stream,
    FarsightSessionError error, const gchar *debug, gpointer user_data);

static void dummy_callback (TpMediaSessionHandler *proxy, const GError *error,
    gpointer user_data, GObject *object);

static void invalidated_cb (TpMediaSessionHandler *proxy, guint domain,
    gint code, gchar *message, gpointer user_data);

static GObject *
tp_stream_engine_session_constructor (GType type,
                                      guint n_props,
                                      GObjectConstructParam *props)
{
  GObject *obj;
  TpStreamEngineSession *self;

  obj = G_OBJECT_CLASS (tp_stream_engine_session_parent_class)->
           constructor (type, n_props, props);
  self = (TpStreamEngineSession *) obj;

  self->priv->fs_session = farsight_session_factory_make (self->priv->session_type);

  /* TODO: signal an error back to the connection manager */
  if (self->priv->fs_session == NULL)
    {
      g_warning ("requested session type was not found, session is unusable!");
      return obj;
    }

  g_debug ("plugin details:\n name: %s\n description: %s\n author: %s",
      farsight_plugin_get_name (self->priv->fs_session->plugin),
      farsight_plugin_get_description (self->priv->fs_session->plugin),
      farsight_plugin_get_author (self->priv->fs_session->plugin));

  g_signal_connect (self->priv->session_handler_proxy, "invalidated",
      G_CALLBACK (invalidated_cb), obj);

  g_signal_connect (G_OBJECT (self->priv->fs_session), "error",
      G_CALLBACK (cb_fs_session_error), self->priv->session_handler_proxy);

  tp_cli_media_session_handler_connect_to_new_stream_handler
      (self->priv->session_handler_proxy, new_media_stream_handler, NULL, NULL,
       obj, NULL);

  g_debug ("calling MediaSessionHandler::Ready");

  tp_cli_media_session_handler_call_ready (self->priv->session_handler_proxy,
      -1, dummy_callback, "Media.SessionHandler::Ready", NULL, NULL);

  return obj;
}

static void
tp_stream_engine_session_dispose (GObject *object)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (object);

  g_debug (G_STRFUNC);

  if (self->priv->session_handler_proxy)
    {
      TpMediaSessionHandler *tmp;

      g_signal_handlers_disconnect_by_func (
          self->priv->session_handler_proxy, invalidated_cb, self);

      tmp = self->priv->session_handler_proxy;
      self->priv->session_handler_proxy = NULL;
      g_object_unref (tmp);
    }

  if (self->priv->fs_session)
    {
      g_object_unref (self->priv->fs_session);
      self->priv->fs_session = NULL;
    }

  g_free (self->priv->session_type);
  self->priv->session_type = NULL;

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

  param_spec = g_param_spec_object ("proxy", "TpMediaSessionHandler proxy",
      "The session handler proxy which this session interacts with.",
      TP_TYPE_MEDIA_SESSION_HANDLER,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_PROXY, param_spec);

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
dummy_callback (TpMediaSessionHandler *proxy,
                const GError *error,
                gpointer user_data,
                GObject *weak_object)
{
  if (error != NULL)
    {
      g_warning ("Error calling %s: %s", (gchar *) user_data, error->message);
    }
}

static void
cb_fs_session_error (FarsightSession *session,
                     FarsightSessionError error,
                     const gchar *debug,
                     gpointer user_data)
{
  TpMediaSessionHandler *session_handler_proxy =
      TP_MEDIA_SESSION_HANDLER (user_data);

  g_message (
    "%s: session error: session=%p error=%s\n", G_STRFUNC, session, debug);
  tp_cli_media_session_handler_call_error (session_handler_proxy, -1, error,
      debug, dummy_callback, "Media.SessionHandler::Error", NULL, NULL);
}

static void
invalidated_cb (TpMediaSessionHandler *proxy,
                guint domain,
                gint code,
                gchar *message,
                gpointer user_data)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (user_data);

  if (self->priv->session_handler_proxy)
    {
      TpMediaSessionHandler *tmp;

      tmp = self->priv->session_handler_proxy;
      self->priv->session_handler_proxy = NULL;
      g_object_unref (tmp);
    }
}

static void
new_media_stream_handler (TpMediaSessionHandler *proxy,
                          const gchar *object_path,
                          guint stream_id,
                          guint media_type,
                          guint direction,
                          gpointer user_data,
                          GObject *object)
{
  TpStreamEngineSession *self = TP_STREAM_ENGINE_SESSION (object);

  g_debug ("New stream, stream_id=%d, media_type=%d, direction=%d",
      stream_id, media_type, direction);

  g_signal_emit (self, signals[NEW_STREAM], 0, object_path, stream_id,
      media_type, direction);
}

TpStreamEngineSession *
tp_stream_engine_session_new (TpMediaSessionHandler *proxy,
                              const gchar *session_type,
                              GError **error)
{
  TpStreamEngineSession *self;

  g_return_val_if_fail (proxy != NULL, NULL);
  g_return_val_if_fail (session_type != NULL, NULL);

  self = g_object_new (TP_STREAM_ENGINE_TYPE_SESSION,
      "proxy", proxy,
      "session-type", session_type,
      NULL);

  if (self->priv->fs_session == NULL)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "requested session type not found");
      g_object_unref (self);
      return NULL;
    }

  return self;
}

