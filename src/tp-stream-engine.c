/*
 * tp-stream-engine.c - Source for TpStreamEngine
 * Copyright (C) 2005 Collabora Ltd.
 * Copyright (C) 2005 Nokia Corporation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dbus/dbus-glib.h>
#include <stdio.h>
#include <string.h>

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-interfaces.h>
#include <libtelepathy/tp-constants.h>

#include <farsight/farsight-session.h>
#include <farsight/farsight-stream.h>
#include <farsight/farsight-codec.h>
#include <farsight/farsight-transport.h>

#include "tp-stream-engine.h"
#include "tp-stream-engine-signals-marshal.h"
#include "misc-signals-marshal.h"
#include "stream-engine-gen.h"

#ifdef USE_INFOPRINT
#include "statusbar-gen.h"
#endif

#include "tp-stream-engine-glue.h"

#include "common/telepathy-errors.h"
#include "common/telepathy-errors-enumtypes.h"

#include "channel.h"
#include "session.h"
#include "stream.h"
#include "types.h"

#define BUS_NAME        "org.freedesktop.Telepathy.VoipEngine"
#define OBJECT_PATH     "/org/freedesktop/Telepathy/VoipEngine"

#define STATUS_BAR_SERVICE_NAME "com.nokia.statusbar"
#define STATUS_BAR_INTERFACE_NAME "com.nokia.statusbar"
#define STATUS_BAR_OBJECT_PATH "/com/nokia/statusbar"

static void
register_dbus_signal_marshallers()
{
  /*register a marshaller for the NewMediaStreamHandler signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__BOXED_UINT_UINT, G_TYPE_NONE,
     DBUS_TYPE_G_OBJECT_PATH, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INVALID);

  /*register a marshaller for the NewMediaSessionHandler signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__UINT_BOXED_STRING, G_TYPE_NONE,
     G_TYPE_UINT, DBUS_TYPE_G_OBJECT_PATH, G_TYPE_STRING, G_TYPE_INVALID);

  /*register a marshaller for the AddRemoteCandidate signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__STRING_BOXED, G_TYPE_NONE,
     G_TYPE_STRING, TP_TYPE_TRANSPORT_LIST, G_TYPE_INVALID);

  /*register a marshaller for the SetActiveCandidatePair signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__STRING_STRING, G_TYPE_NONE,
     G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

  /*register a marshaller for the SetRemoteCandidateList signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__BOXED, G_TYPE_NONE,
     TP_TYPE_CANDIDATE_LIST, G_TYPE_INVALID);

  /*register a marshaller for the SetRemoteCodecs signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__BOXED, G_TYPE_NONE,
     TP_TYPE_CODEC_LIST, G_TYPE_INVALID);
}

G_DEFINE_TYPE(TpStreamEngine, tp_stream_engine, G_TYPE_OBJECT)

/* signal enum */
enum
{
  HANDLING_CHANNEL,
  NO_MORE_CHANNELS,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
typedef struct _TpStreamEnginePrivate TpStreamEnginePrivate;
struct _TpStreamEnginePrivate
{
  gboolean dispose_has_run;

  GPtrArray *channels;

#ifdef MAEMO_OSSO_SUPPORT
  DBusGProxy *infoprint_proxy;
#endif

};

#define TP_STREAM_ENGINE_GET_PRIVATE(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), TP_TYPE_STREAM_ENGINE, TpStreamEnginePrivate))

#ifdef USE_INFOPRINT
static void
tp_stream_engine_infoprint (const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *message,
    gpointer user_data)
{
  TpStreamEnginePrivate *priv = (TpStreamEnginePrivate *)user_data;
  com_nokia_statusbar_system_note_infoprint (
          DBUS_G_PROXY (priv->infoprint_proxy),
          message, NULL);
  g_log_default_handler (log_domain, log_level, message, user_data);
}
#endif

static void
tp_stream_engine_init (TpStreamEngine *obj)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);

  priv->channels = g_ptr_array_new ();

#ifdef USE_INFOPRINT
  priv->infoprint_proxy =
    dbus_g_proxy_new_for_name (tp_get_bus(),
        STATUS_BAR_SERVICE_NAME,
        STATUS_BAR_OBJECT_PATH,
        STATUS_BAR_INTERFACE_NAME);

  g_debug ("Using infoprint %p", priv->infoprint_proxy);
  /* handler for stream-engine messages */
  g_log_set_handler (NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL |
      G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, tp_stream_engine_infoprint, priv);

  /* handler for farsight messages */
  /*
  g_log_set_handler ("Farsight", G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_CRITICAL |
      G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, tp_stream_engine_infoprint, NULL);
      */

#endif
}

static void tp_stream_engine_dispose (GObject *object);
static void tp_stream_engine_finalize (GObject *object);

static void
tp_stream_engine_class_init (TpStreamEngineClass *tp_stream_engine_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (tp_stream_engine_class);

  g_type_class_add_private (tp_stream_engine_class, sizeof (TpStreamEnginePrivate));

  object_class->dispose = tp_stream_engine_dispose;
  object_class->finalize = tp_stream_engine_finalize;

  /**
   * TpStreamEngine::handling-channel:
   *
   * Emitted whenever this object starts handling a channel
   */
  signals[HANDLING_CHANNEL] =
  g_signal_new ("handling-channel",
                G_OBJECT_CLASS_TYPE (tp_stream_engine_class),
                G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

  /**
   * TpStreamEngine::no-more-channels:
   *
   * Emitted whenever this object is handling no channels
   */
  signals[NO_MORE_CHANNELS] =
  g_signal_new ("no-more-channels",
                G_OBJECT_CLASS_TYPE (tp_stream_engine_class),
                G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);


  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (tp_stream_engine_class), &dbus_glib_tp_stream_engine_object_info);
}

void
tp_stream_engine_dispose (GObject *object)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (object);
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  if (priv->channels)
    {
      guint i;

      for (i = 0; i < priv->channels->len; i++)
        g_object_unref (g_ptr_array_index (priv->channels, i));

      g_ptr_array_free (priv->channels, TRUE);
      priv->channels = NULL;
    }

  priv->dispose_has_run = TRUE;

  if (G_OBJECT_CLASS (tp_stream_engine_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_parent_class)->dispose (object);
}

void
tp_stream_engine_finalize (GObject *object)
{
#ifdef MAEMO_OSSO_SUPPORT
  TpStreamEngine *self = TP_STREAM_ENGINE (object);

  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);

  if (priv->infoprint_proxy)
    {
      g_debug ("priv->infoprint_proxy->ref_count before unref == %d", G_OBJECT (priv->infoprint_proxy)->ref_count);
      g_object_unref (priv->infoprint_proxy);
      priv->infoprint_proxy = NULL;
    }
#endif

  G_OBJECT_CLASS (tp_stream_engine_parent_class)->finalize (object);
}

/**
 * tp_stream_engine_error
 *
 * Used to inform the stream engine than an exceptional situation has ocurred.
 *
 * @error:   The error ID, as per the
 *           org.freedesktop.Telepathy.Media.StreamHandler::Error signal.
 * @message: The human-readable error message.
 */
void
tp_stream_engine_error (TpStreamEngine *self, int error, const char *message)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);
  guint i;

  for (i = 0; i < priv->channels->len; i++)
    tp_stream_engine_channel_error (
      g_ptr_array_index (priv->channels, i), error, message);
}

static void
channel_closed (TpStreamEngineChannel *chan, gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);

  g_debug ("channel closed: %p", chan);

  g_ptr_array_remove_fast (priv->channels, chan);

  if (priv->channels->len == 0)
    {
      g_debug ("no channels remaining; emitting no-more-channels");
      g_signal_emit (self, signals[NO_MORE_CHANNELS], 0);
    }
  else
    {
      g_debug ("channels remaining: %d", priv->channels->len);
    }

  g_object_unref (chan);
}

/**
 * tp_stream_engine_handle_channel
 *
 * Implements DBus method HandleChannel
 * on interface org.freedesktop.Telepathy.ChannelHandler
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_handle_channel (TpStreamEngine *obj, const gchar * bus_name, const gchar * connection, const gchar * channel_type, const gchar * channel, guint handle_type, guint handle, GError **error)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  TpStreamEngineChannel *chan = NULL;

  g_debug("HandleChannel called");

  if (strcmp (channel_type, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA) != 0)
    {
      const gchar *message =
        "Stream Engine was passed a channel that was not a "
        TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA;
      *error = g_error_new (TELEPATHY_ERRORS, InvalidArgument, message);
      g_message (message);
      goto ERROR;
     }

  chan = tp_stream_engine_channel_new ();

  if (!tp_stream_engine_channel_go (chan, bus_name, connection, channel,
      handle_type, handle, error))
    goto ERROR;

  g_ptr_array_add (priv->channels, chan);

  g_signal_connect (chan, "closed", G_CALLBACK (channel_closed), obj);

  g_signal_emit (obj, signals[HANDLING_CHANNEL], 0);

  return TRUE;

ERROR:
  if (chan)
    g_object_unref (chan);

  return FALSE;
}

void
tp_stream_engine_register (TpStreamEngine *self)
{
  DBusGConnection *bus;
  DBusGProxy *bus_proxy;
  GError *error = NULL;
  guint request_name_result;

  g_assert (TP_IS_STREAM_ENGINE (self));

  bus = tp_get_bus ();
  bus_proxy = tp_get_bus_proxy ();

  g_debug("Requesting " BUS_NAME);

  if (!dbus_g_proxy_call (bus_proxy, "RequestName", &error,
                          G_TYPE_STRING, BUS_NAME,
                          G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
                          G_TYPE_INVALID,
                          G_TYPE_UINT, &request_name_result,
                          G_TYPE_INVALID))
    g_error ("Failed to request bus name: %s", error->message);

  if (request_name_result == DBUS_REQUEST_NAME_REPLY_EXISTS)
    g_error ("Failed to acquire bus name, stream engine already running?");

  g_debug("registering StreamEngine at " OBJECT_PATH);
  dbus_g_connection_register_g_object (bus, OBJECT_PATH, G_OBJECT (self));

  register_dbus_signal_marshallers();
}

static TpStreamEngineStream *
_lookup_stream (TpStreamEngine *obj, const gchar *path, guint stream_id,
  GError **error)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  guint i, j, k;

  for (i = 0; i < priv->channels->len; i++)
    {
      TpStreamEngineChannel *channel = TP_STREAM_ENGINE_CHANNEL (
        priv->channels->pdata[i]);

      if (0 == strcmp (path, channel->channel_path))
        {
          for (j = 0; j < channel->sessions->len; j++)
            {
              TpStreamEngineSession *session = TP_STREAM_ENGINE_SESSION (
                channel->sessions->pdata[j]);

              for (k = 0; k < session->streams->len; k++)
                {
                  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (
                    session->streams->pdata[k]);

                  if (stream_id == stream->stream_id)
                    return stream;
                }
            }

          *error = g_error_new (TELEPATHY_ERRORS, NotAvailable,
            "the channel %s has no stream with id %d", path, stream_id);
          return NULL;
        }
    }

  *error = g_error_new (TELEPATHY_ERRORS, NotAvailable,
    "stream-engine is not handling the channel %s", path);
  return NULL;
}


/**
 * tp_stream_engine_hold_stream
 *
 * Implements DBus method HoldStream
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_hold_stream (TpStreamEngine *obj, const gchar * channel_path, guint stream_id, gboolean hold_state, GError **error)
{
  TpStreamEngineStream *stream;

  stream = _lookup_stream (obj, channel_path, stream_id, error);

  if (!stream)
    return FALSE;

  return tp_stream_engine_stream_hold_stream (stream, hold_state, error);
}

/**
 * tp_stream_engine_mute_output
 *
 * Implements DBus method MuteOutput
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_mute_output (TpStreamEngine *obj, const gchar * channel_path, guint stream_id, gboolean mute_state, GError **error)
{
  TpStreamEngineStream *stream;

  stream = _lookup_stream (obj, channel_path, stream_id, error);

  if (!stream)
    return FALSE;

  return tp_stream_engine_stream_mute_output (stream, mute_state, error);
}

/**
 * tp_stream_engine_set_output_volume
 *
 * Implements DBus method SetOutputVolume
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_set_output_volume (TpStreamEngine *obj, const gchar * channel_path, guint stream_id, guint volume, GError **error)
{
  TpStreamEngineStream *stream;

  stream = _lookup_stream (obj, channel_path, stream_id, error);

  if (!stream)
    return FALSE;

  return tp_stream_engine_stream_set_output_volume (stream, volume, error);
}

/**
 * tp_stream_engine_set_output_window
 *
 * Implements DBus method SetOutputWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_set_output_window (TpStreamEngine *obj, const gchar * channel_path, guint stream_id, guint window, GError **error)
{
  TpStreamEngineStream *stream;

  stream = _lookup_stream (obj, channel_path, stream_id, error);

  if (!stream)
    return FALSE;

  return tp_stream_engine_stream_set_output_window (stream, window, error);
}


/**
 * tp_stream_engine_set_preview_window
 *
 * Implements DBus method SetPreviewWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_set_preview_window (TpStreamEngine *obj, const gchar * channel_path, guint stream_id, guint window, GError **error)
{
  TpStreamEngineStream *stream;

  stream = _lookup_stream (obj, channel_path, stream_id, error);

  if (!stream)
    return FALSE;

  return tp_stream_engine_stream_set_preview_window (stream, window, error);
}

