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

#include <gst/interfaces/xoverlay.h>

#include "tp-stream-engine.h"
#include "tp-stream-engine-signals-marshal.h"
#include "misc-signals-marshal.h"

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
#include "xerrorhandler.h"

#define BUS_NAME        "org.freedesktop.Telepathy.StreamEngine"
#define OBJECT_PATH     "/org/freedesktop/Telepathy/StreamEngine"

#define STATUS_BAR_SERVICE_NAME "com.nokia.statusbar"
#define STATUS_BAR_INTERFACE_NAME "com.nokia.statusbar"
#define STATUS_BAR_OBJECT_PATH "/com/nokia/statusbar"

static void
register_dbus_signal_marshallers()
{
  /*register a marshaller for the NewMediaStreamHandler signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__BOXED_UINT_UINT_UINT, G_TYPE_NONE,
     DBUS_TYPE_G_OBJECT_PATH, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT,
     G_TYPE_INVALID);

  /*register a marshaller for the NewMediaSessionHandler signal*/
  dbus_g_object_register_marshaller
    (misc_marshal_VOID__BOXED_STRING, G_TYPE_NONE,
     DBUS_TYPE_G_OBJECT_PATH, G_TYPE_STRING, G_TYPE_INVALID);

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
  GHashTable *preview_windows;
  GHashTable *output_windows;
  GstElement *pipeline;
  guint bad_drawable_handler_id;
  guint bad_window_handler_id;

  gint tee_counter;

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

static gboolean
bad_drawable_cb (TpStreamEngineXErrorHandler *handler,
                 guint window_id,
                 gpointer data);

static gboolean
bad_window_cb (TpStreamEngineXErrorHandler *handler,
               guint window_id,
               gpointer data);

static void
tp_stream_engine_init (TpStreamEngine *obj)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  TpStreamEngineXErrorHandler *handler =
    tp_stream_engine_x_error_handler_get ();

  priv->channels = g_ptr_array_new ();
  priv->preview_windows = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->output_windows = g_hash_table_new (g_direct_hash, g_direct_equal);

  priv->bad_drawable_handler_id =
    g_signal_connect (handler, "bad-drawable", (GCallback) bad_drawable_cb,
      obj);
  priv->bad_window_handler_id =
    g_signal_connect (handler, "bad-window", (GCallback) bad_window_cb,
      obj);

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

  priv->tee_counter = 0;

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

  if (priv->pipeline)
    {
      gst_element_set_state (priv->pipeline, GST_STATE_NULL);
      g_object_unref (priv->pipeline);
      priv->pipeline = NULL;
    }

  if (priv->preview_windows)
    {
      g_hash_table_destroy (priv->preview_windows);
      priv->preview_windows = NULL;
    }

  if (priv->output_windows)
    {
      g_hash_table_destroy (priv->output_windows);
      priv->output_windows = NULL;
    }

  if (priv->bad_drawable_handler_id)
    {
      TpStreamEngineXErrorHandler *handler =
        tp_stream_engine_x_error_handler_get ();

      g_signal_handler_disconnect (handler, priv->bad_drawable_handler_id);
      priv->bad_drawable_handler_id = 0;
    }

  if (priv->bad_window_handler_id)
    {
      TpStreamEngineXErrorHandler *handler =
        tp_stream_engine_x_error_handler_get ();

      g_signal_handler_disconnect (handler, priv->bad_window_handler_id);
      priv->bad_window_handler_id = 0;
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
check_if_busy (TpStreamEngine *self)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);
  guint num_previews = g_hash_table_size (priv->preview_windows);

  if (priv->channels->len == 0 && num_previews == 0)
    {
      g_debug ("no channels or previews remaining; emitting no-more-channels");
      g_signal_emit (self, signals[NO_MORE_CHANNELS], 0);
    }
  else
    {
      g_debug ("channels remaining: %d", priv->channels->len);
      g_debug ("preview windows remaining: %d", num_previews);
    }
}


static void
channel_closed (TpStreamEngineChannel *chan, gpointer user_data)
{
  TpStreamEngine *self = TP_STREAM_ENGINE (user_data);
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (self);

  g_debug ("channel closed: %p", chan);
  g_ptr_array_remove_fast (priv->channels, chan);
  g_object_unref (chan);
  check_if_busy (self);
}

static GstBusSyncReply
bus_sync_handler (GstBus *bus, GstMessage *message, gpointer data)
{
  TpStreamEngine *engine = TP_STREAM_ENGINE (data);
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (engine);
  guint window_id;

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR)
    {
      gchar *error = gst_structure_to_string (message->structure);
      /* FIXME: raise the error signal here? */
      g_debug ("got error:\n%s", error);
      g_free (error);
      //g_assert_not_reached ();
    }

  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return GST_BUS_PASS;

  if (!gst_structure_has_name (message->structure, "prepare-xwindow-id"))
    return GST_BUS_PASS;

  g_debug ("got prepare-xwindow-id message");

  window_id = GPOINTER_TO_UINT (
    g_hash_table_lookup (priv->output_windows, GST_MESSAGE_SRC (message)));

  if (!window_id)
    {
      window_id = GPOINTER_TO_UINT (
        g_hash_table_lookup (priv->preview_windows, GST_MESSAGE_SRC (message)));
    }

  if (!window_id)
    {
      return GST_BUS_PASS;
    }

  gst_x_overlay_set_xwindow_id (
      GST_X_OVERLAY (GST_MESSAGE_SRC (message)), window_id);

  return GST_BUS_DROP;
}

static
void tee_linked (GstPad  *pad,
    GstPad  *peer,
    gpointer user_data)
{
  TpStreamEnginePrivate *priv = (TpStreamEnginePrivate *)user_data;

  if (gst_pad_get_direction (pad) == GST_PAD_SRC)
  {
    g_debug ("Element linked to tee src pad, incrementing counter");
    priv->tee_counter++;
  }
}

static
void tee_unlinked (GstPad  *pad,
    GstPad  *peer,
    gpointer user_data)
{
  TpStreamEnginePrivate *priv = (TpStreamEnginePrivate *)user_data;

  if (gst_pad_get_direction (pad) == GST_PAD_SRC)
  {
    g_debug ("Element unlinked to tee src pad, decrementing counter");
    priv->tee_counter--;
  }
}

/*
 * tp_stream_engine_get_pipeline
 *
 * Return the GStreamer pipeline belonging to the stream engine. Caller does
 * not own a reference to the pipeline.
 */

GstElement *
tp_stream_engine_get_pipeline (TpStreamEngine *obj)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  GstElement *videosrc;
  GstElement *tee;
  GstBus *bus;
  GstCaps *filter;

  if (NULL == priv->pipeline)
    {
      const gchar *elem;

      priv->pipeline = gst_pipeline_new (NULL);
      tee = gst_element_factory_make ("tee", "tee");

      g_signal_connect (tee, "linked", G_CALLBACK (tee_linked), priv);
      g_signal_connect (tee, "unlinked", G_CALLBACK (tee_unlinked), priv);

      if ((elem = getenv ("FS_VIDEO_SRC")) || (elem = getenv ("FS_VIDEOSRC")))
        {
          g_debug ("making video src with pipeline \"%s\"", elem);
          videosrc = gst_parse_bin_from_description (elem, TRUE, NULL);
          g_assert (videosrc);
        }
      else
        {
          videosrc = gst_element_factory_make ("v4l2src", NULL);
        }
      filter = gst_caps_new_simple(
        "video/x-raw-yuv",
        "width", G_TYPE_INT, 352,
        "height", G_TYPE_INT, 288,
        "framerate", GST_TYPE_FRACTION, 15, 1,
        NULL);

      gst_bin_add_many (GST_BIN (priv->pipeline), videosrc, tee, NULL);
      gst_element_link_filtered (videosrc, tee, filter);
      gst_caps_unref (filter);

      /* connect a callback to the stream bus so that we can set X window IDs
       * at the right time */
      bus = gst_element_get_bus (priv->pipeline);
      gst_bus_set_sync_handler (bus, bus_sync_handler, obj);
      gst_object_unref (bus);
    }

  return priv->pipeline;
}

/**
 * tp_stream_engine_add_preview_window
 *
 * Implements DBus method AddPreviewWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_add_preview_window (TpStreamEngine *obj, guint window, GError **error)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  GstElement *tee, *sink, *filter, *pipeline;

  sink = g_hash_table_lookup (
    priv->preview_windows, GUINT_TO_POINTER (window));

  if (NULL != sink)
    {
      *error = g_error_new (TELEPATHY_ERRORS, InvalidArgument,
        "window %d already has a preview", window);
      return FALSE;
    }

  pipeline = tp_stream_engine_get_pipeline (obj);

  g_debug ("adding preview in window %d", window);

  tee = gst_bin_get_by_name (GST_BIN (priv->pipeline), "tee");
  sink = gst_element_factory_make ("xvimagesink", NULL);
  g_object_set (G_OBJECT (sink), "sync", FALSE, NULL);
  filter = gst_element_factory_make ("ffmpegcolorspace", NULL);
  gst_bin_add (GST_BIN (priv->pipeline), filter);
  gst_bin_add (GST_BIN (priv->pipeline), sink);
  gst_element_link_many (tee, filter, sink, NULL);
  //gst_element_sync_state_with_parent (sink);
  gst_object_unref (tee);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_hash_table_insert (priv->preview_windows, sink, GUINT_TO_POINTER (window));

  g_signal_emit (obj, signals[HANDLING_CHANNEL], 0);

  return TRUE;
}

static gboolean
bad_window_cb (TpStreamEngineXErrorHandler *handler,
               guint window_id,
               gpointer data)
{
  return tp_stream_engine_remove_preview_window (TP_STREAM_ENGINE (data),
      window_id, NULL);
}


typedef struct {
  guint window_id;
  GstElement *sink;
} ReverseLookup;

static gboolean
_find_preview_sink_by_window_id (GstElement *sink,
                                 guint window_id,
                                 ReverseLookup *reverse)
{
  if (window_id == reverse->window_id)
    reverse->sink = sink;

  return TRUE;
}

/**
 * tp_stream_engine_remove_preview_window
 *
 * Implements DBus method RemovePreviewWindow
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_remove_preview_window (TpStreamEngine *obj, guint window, GError **error)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);
  ReverseLookup reverse = { 0, };
  GstElement *tee;
  GstElement *sink;

  /* use the window ID to find the sink, even though
   * the hash table is of sinks to window IDs */
  reverse.window_id = window;
  g_hash_table_find (priv->preview_windows,
      (GHRFunc) _find_preview_sink_by_window_id, &reverse);
  sink = reverse.sink;

  if (NULL == sink)
    {
      /* set *error here if error is set (bad_window does not care) */
      return FALSE;
    }

  g_debug ("removing preview in window %d, tee counter is %d", window,
      priv->tee_counter);
  tee = gst_bin_get_by_name (GST_BIN (priv->pipeline), "tee");

  /* let's check if this is the last sink connected to tee, if so PAUSE */
  if (priv->tee_counter == 1)
    {
      g_debug ("This preview window is the last one, "
          "pausing pipeline before disconnecting");
      gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
    }

  gst_element_set_state (sink, GST_STATE_NULL);
  gst_bin_remove (GST_BIN (priv->pipeline), sink);
  gst_object_unref (GST_OBJECT (tee));

  g_hash_table_remove (priv->preview_windows, sink);
  check_if_busy (obj);

  return TRUE;
}

/* FIXME: add corresponding _remove_output_window */
gboolean
tp_stream_engine_add_output_window (TpStreamEngine *obj,
                                    GstElement *sink,
                                    guint window)
{
  TpStreamEnginePrivate *priv = TP_STREAM_ENGINE_GET_PRIVATE (obj);

  g_hash_table_insert (priv->output_windows, sink, GUINT_TO_POINTER (window));
  return TRUE;
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
 * tp_stream_engine_mute_input
 *
 * Implements DBus method MuteInput
 * on interface org.freedesktop.Telepathy.StreamEngine
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occured, DBus will throw the error only if this
 *         function returns false.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean tp_stream_engine_mute_input (TpStreamEngine *obj, const gchar * channel_path, guint stream_id, gboolean mute_state, GError **error)
{
  TpStreamEngineStream *stream;

  stream = _lookup_stream (obj, channel_path, stream_id, error);

  if (!stream)
    return FALSE;

  return tp_stream_engine_stream_mute_input (stream, mute_state, error);
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

/*
 * tp_stream_engine_get
 *
 * Return the stream engine singleton. Caller does not own a reference to the
 * stream engine.
 */

TpStreamEngine *
tp_stream_engine_get ()
{
  static TpStreamEngine *engine = NULL;

  if (NULL == engine)
    {
      engine = g_object_new (TP_TYPE_STREAM_ENGINE, NULL);
      g_object_add_weak_pointer (G_OBJECT (engine), (gpointer) &engine);
    }

  return engine;
}

