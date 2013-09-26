/*
 * call-handler.c
 * Copyright (C) 2011 Collabora Ltd.
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

#include "config.h"

#include <gst/gst.h>
#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>
#include <farstream/fs-element-added-notifier.h>
#include <farstream/fs-utils.h>
#include <telepathy-farstream/telepathy-farstream.h>

typedef struct {
  GstElement *pipeline;
  guint buswatch;
  TpChannel *proxy;
  TfChannel *channel;
  GList *notifiers;

  guint input_volume;
  guint output_volume;

  gboolean has_audio_src;
  gboolean has_video_src;

  GstElement *video_input;
  GstElement *video_capsfilter;

  guint width;
  guint height;
  guint framerate;
} ChannelContext;

GMainLoop *loop;

static gboolean
bus_watch_cb (GstBus *bus,
    GstMessage *message,
    gpointer user_data)
{
  ChannelContext *context = user_data;

  if (context->channel != NULL)
    tf_channel_bus_message (context->channel, message);

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR)
    {
      GError *error = NULL;
      gchar *debug = NULL;
      gst_message_parse_error (message, &error, &debug);
      g_printerr ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (message->src), error->message);
      g_printerr ("Debugging info: %s\n", (debug) ? debug : "none");
      g_error_free (error);
      g_free (debug);
    }

  return TRUE;
}

static void
on_audio_output_volume_changed (TfContent *content,
  GParamSpec *spec,
  GstElement *volume)
{
  guint output_volume = 0;

  g_object_get (content, "requested-output-volume", &output_volume, NULL);

  if (output_volume == 0)
    return;

  g_object_set (volume, "volume", (double)output_volume / 255.0, NULL);
}

static void
src_pad_added_cb (TfContent *content,
    TpHandle handle,
    FsStream *stream,
    GstPad *pad,
    FsCodec *codec,
    gpointer user_data)
{
  ChannelContext *context = user_data;
  gchar *cstr = fs_codec_to_string (codec);
  FsMediaType mtype;
  GstPad *sinkpad;
  GstElement *element;
  GstStateChangeReturn ret;

  g_debug ("New src pad: %s", cstr);
  g_object_get (content, "media-type", &mtype, NULL);

  switch (mtype)
    {
      case FS_MEDIA_TYPE_AUDIO:
        {
          GstElement *volume = NULL;
          gchar *tmp_str = g_strdup_printf ("audioconvert ! audioresample "
              "! volume name=\"output_volume%s\" "
              "! audioconvert ! autoaudiosink", cstr);
          element = gst_parse_bin_from_description (tmp_str,
              TRUE, NULL);
          g_free (tmp_str);

          tmp_str = g_strdup_printf ("output_volume%s", cstr);
          volume = gst_bin_get_by_name (GST_BIN (element), tmp_str);
          g_free (tmp_str);

          tp_g_signal_connect_object (content, "notify::output-volume",
              G_CALLBACK (on_audio_output_volume_changed),
              volume, 0);

          gst_object_unref (volume);

          break;
        }
      case FS_MEDIA_TYPE_VIDEO:
        element = gst_parse_bin_from_description (
          "videoconvert ! videoscale ! autovideosink",
          TRUE, NULL);
        break;
      default:
        g_warning ("Unknown media type");
        return;
    }

  gst_bin_add (GST_BIN (context->pipeline), element);
  sinkpad = gst_element_get_static_pad (element, "sink");
  ret = gst_element_set_state (element, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE)
    {
      tp_channel_close_async (TP_CHANNEL (context->proxy), NULL, NULL);
      g_warning ("Failed to start sink pipeline !?");
      return;
    }

  if (GST_PAD_LINK_FAILED (gst_pad_link (pad, sinkpad)))
    {
      tp_channel_close_async (TP_CHANNEL (context->proxy), NULL, NULL);
      g_warning ("Couldn't link sink pipeline !?");
      return;
    }

  g_object_unref (sinkpad);
}

static void
update_video_parameters (ChannelContext *context, gboolean restart)
{
  GstCaps *caps;
  GstClock *clock;

  if (restart)
    {
      /* Assuming the pipeline is in playing state */
      gst_element_set_locked_state (context->video_input, TRUE);
      gst_element_set_state (context->video_input, GST_STATE_NULL);
    }

  g_object_get (context->video_capsfilter, "caps", &caps, NULL);
  caps = gst_caps_make_writable (caps);

  gst_caps_set_simple (caps,
      "framerate", GST_TYPE_FRACTION, context->framerate, 1,
      "width", G_TYPE_INT, context->width,
      "height", G_TYPE_INT, context->height,
      NULL);

  g_object_set (context->video_capsfilter, "caps", caps, NULL);

  if (restart)
    {
      clock = gst_pipeline_get_clock (GST_PIPELINE (context->pipeline));
      /* Need to reset the clock if we set the pipeline back to ready by hand */
      if (clock != NULL)
        {
          gst_element_set_clock (context->video_input, clock);
          g_object_unref (clock);
        }

      gst_element_set_locked_state (context->video_input, FALSE);
      gst_element_sync_state_with_parent (context->video_input);
    }
}

static void
on_video_framerate_changed (TfContent *content,
  GParamSpec *spec,
  ChannelContext *context)
{
  guint framerate;

  g_object_get (content, "framerate", &framerate, NULL);

  if (framerate != 0)
    context->framerate = framerate;

  update_video_parameters (context, FALSE);
}

static void
on_video_resolution_changed (TfContent *content,
   guint width,
   guint height,
   ChannelContext *context)
{
  g_assert (width > 0 && height > 0);

  context->width = width;
  context->height = height;

  update_video_parameters (context, TRUE);
}

static void
on_audio_input_volume_changed (TfContent *content,
  GParamSpec *spec,
  ChannelContext *context)
{
  GstElement *volume;
  guint input_volume = 0;

  g_object_get (content, "requested-input-volume", &input_volume, NULL);

  if (input_volume == 0)
    return;

  volume = gst_bin_get_by_name (GST_BIN (context->pipeline), "input_volume");
  g_object_set (volume, "volume", (double)input_volume / 255.0, NULL);
  gst_object_unref (volume);
}

static GstElement *
setup_audio_source (ChannelContext *context, TfContent *content)
{
  GstElement *result;
  GstElement *volume;
  gint input_volume = 0;

  result = gst_parse_bin_from_description (
      "pulsesrc ! audio/x-raw, rate=8000 ! queue"
      " ! audioconvert ! audioresample"
      " ! volume name=input_volume ! audioconvert ",
      TRUE, NULL);

  /* FIXME Need to handle both requested/reported */
  /* TODO Volume control should be handled in FsIo */
  g_object_get (content,
      "requested-input-volume", &input_volume,
      NULL);

  if (input_volume >= 0)
    {
      volume = gst_bin_get_by_name (GST_BIN (result), "input_volume");
      g_debug ("Requested volume is: %i", input_volume);
      g_object_set (volume, "volume", (double)input_volume / 255.0, NULL);
      gst_object_unref (volume);
    }

  g_signal_connect (content, "notify::requested-input-volume",
      G_CALLBACK (on_audio_input_volume_changed),
      context);

  return result;
}

static GstElement *
setup_video_source (ChannelContext *context, TfContent *content)
{
  GstElement *result, *capsfilter;
  GstCaps *caps;
  guint framerate = 0, width = 0, height = 0;

  result = gst_parse_bin_from_description_full (
      "autovideosrc ! videorate drop-only=1 average-period=20000000000 ! videoscale ! videoconvert ! capsfilter name=c",
      TRUE, NULL, GST_PARSE_FLAG_FATAL_ERRORS, NULL);

  g_assert (result);
  capsfilter = gst_bin_get_by_name (GST_BIN (result), "c");

  g_object_get (content,
      "framerate", &framerate,
      "width", &width,
      "height", &height,
      NULL);

  if (framerate == 0)
    framerate = 15;

  if (width == 0 || height == 0)
    {
      width = 320;
      height = 240;
    }

  context->framerate = framerate;
  context->width = width;
  context->height = height;

  caps = gst_caps_new_simple ("video/x-raw",
      "width", G_TYPE_INT, width,
      "height", G_TYPE_INT, height,
      "framerate", GST_TYPE_FRACTION, framerate, 1,
      NULL);

  g_object_set (G_OBJECT (capsfilter), "caps", caps, NULL);

  gst_caps_unref (caps);

  context->video_input = result;
  context->video_capsfilter = capsfilter;

  g_signal_connect (content, "notify::framerate",
    G_CALLBACK (on_video_framerate_changed),
    context);

  g_signal_connect (content, "resolution-changed",
    G_CALLBACK (on_video_resolution_changed),
    context);

  return result;
}

static gboolean
start_sending_cb (TfContent *content, gpointer user_data)
{
  ChannelContext *context = user_data;
  GstPad *srcpad, *sinkpad;
  FsMediaType mtype;
  GstElement *element;
  GstStateChangeReturn ret;
  gboolean res = FALSE;

  g_debug ("Start sending");

  g_object_get (content,
    "sink-pad", &sinkpad,
    "media-type", &mtype,
    NULL);

  switch (mtype)
    {
      case FS_MEDIA_TYPE_AUDIO:
        if (context->has_audio_src)
          goto out;

        element = setup_audio_source (context, content);
        context->has_audio_src = TRUE;
        break;
      case FS_MEDIA_TYPE_VIDEO:
        if (context->has_video_src)
          goto out;

        element = setup_video_source (context, content);
        context->has_video_src = TRUE;
        break;
      default:
        g_warning ("Unknown media type");
        goto out;
    }


  gst_bin_add (GST_BIN (context->pipeline), element);
  srcpad = gst_element_get_static_pad (element, "src");

  if (GST_PAD_LINK_FAILED (gst_pad_link (srcpad, sinkpad)))
    {
      tp_channel_close_async (TP_CHANNEL (context->proxy), NULL, NULL);
      g_warning ("Couldn't link source pipeline !?");
      goto out2;
    }

  ret = gst_element_set_state (element, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE)
    {
      tp_channel_close_async (TP_CHANNEL (context->proxy), NULL, NULL);
      g_warning ("source pipeline failed to start!?");
      goto out2;
    }

  res = TRUE;

out2:
  g_object_unref (srcpad);
out:
  g_object_unref (sinkpad);

  return res;
}

static void
content_added_cb (TfChannel *channel,
    TfContent *content,
    gpointer user_data)
{
  ChannelContext *context = user_data;

  g_debug ("Content added");

  g_signal_connect (content, "src-pad-added",
    G_CALLBACK (src_pad_added_cb), context);
  g_signal_connect (content, "start-sending",
      G_CALLBACK (start_sending_cb), context);
}

static void
conference_added_cb (TfChannel *channel,
  GstElement *conference,
  gpointer user_data)
{
  ChannelContext *context = user_data;
  GKeyFile *keyfile;

  g_debug ("Conference added");

  /* Add notifier to set the various element properties as needed */
  keyfile = fs_utils_get_default_element_properties (conference);
  if (keyfile != NULL)
    {
      FsElementAddedNotifier *notifier;
      g_debug ("Loaded default codecs for %s", GST_ELEMENT_NAME (conference));

      notifier = fs_element_added_notifier_new ();
      fs_element_added_notifier_set_properties_from_keyfile (notifier, keyfile);
      fs_element_added_notifier_add (notifier, GST_BIN (context->pipeline));

      context->notifiers = g_list_prepend (context->notifiers, notifier);
    }


  gst_bin_add (GST_BIN (context->pipeline), conference);
  gst_element_set_state (conference, GST_STATE_PLAYING);
}


static void
conference_removed_cb (TfChannel *channel,
  GstElement *conference,
  gpointer user_data)
{
  ChannelContext *context = user_data;

  gst_element_set_locked_state (conference, TRUE);
  gst_element_set_state (conference, GST_STATE_NULL);
  gst_bin_remove (GST_BIN (context->pipeline), conference);
}

static gboolean
dump_pipeline_cb (gpointer data)
{
  ChannelContext *context = data;

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (context->pipeline),
    GST_DEBUG_GRAPH_SHOW_ALL,
    "call-handler");

  return TRUE;
}

static void
new_tf_channel_cb (GObject *source,
  GAsyncResult *result,
  gpointer user_data)
{
  ChannelContext *context = user_data;
  GError *error = NULL;

  g_debug ("New TfChannel");

  context->channel = tf_channel_new_finish (source, result, &error);

  if (context->channel == NULL)
    {
      g_error ("Failed to create channel: %s", error->message);
      g_clear_error (&error);
    }

  g_debug ("Adding timeout");
  g_timeout_add_seconds (5, dump_pipeline_cb, context);

  g_signal_connect (context->channel, "fs-conference-added",
    G_CALLBACK (conference_added_cb), context);


  g_signal_connect (context->channel, "fs-conference-removed",
    G_CALLBACK (conference_removed_cb), context);

  g_signal_connect (context->channel, "content-added",
    G_CALLBACK (content_added_cb), context);
}

static void
proxy_invalidated_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  ChannelContext *context = user_data;

  g_debug ("Channel closed");
  if (context->pipeline != NULL)
    {
      gst_element_set_state (context->pipeline, GST_STATE_NULL);
      g_object_unref (context->pipeline);
    }

  if (context->channel != NULL)
    g_object_unref (context->channel);

  g_list_foreach (context->notifiers, (GFunc) g_object_unref, NULL);
  g_list_free (context->notifiers);

  g_object_unref (context->proxy);

  g_slice_free (ChannelContext, context);

  g_main_loop_quit (loop);
}

static void
new_call_channel_cb (TpSimpleHandler *handler,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    GList *requests_satisfied,
    gint64 user_action_time,
    TpHandleChannelsContext *handler_context,
    gpointer user_data)
{
  ChannelContext *context;
  TpChannel *proxy;
  GstBus *bus;
  GstElement *pipeline;
  GstStateChangeReturn ret;

  g_debug ("New channel");

  proxy = channels->data;

  pipeline = gst_pipeline_new (NULL);

  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);

  if (ret == GST_STATE_CHANGE_FAILURE)
    {
      tp_channel_close_async (TP_CHANNEL (proxy), NULL, NULL);
      g_object_unref (pipeline);
      g_warning ("Failed to start an empty pipeline !?");
      return;
    }

  context = g_slice_new0 (ChannelContext);
  context->pipeline = pipeline;

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  context->buswatch = gst_bus_add_watch (bus, bus_watch_cb, context);
  g_object_unref (bus);

  tf_channel_new_async (proxy, new_tf_channel_cb, context);

  tp_handle_channels_context_accept (handler_context);

  tp_cli_channel_type_call_call_accept (proxy, -1,
      NULL, NULL, NULL, NULL);

  context->proxy = g_object_ref (proxy);
  g_signal_connect (proxy, "invalidated",
    G_CALLBACK (proxy_invalidated_cb),
    context);
}

int
main (int argc, char **argv)
{
  TpBaseClient *client;
  TpAccountManager *am;

  g_type_init ();
  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  am = tp_account_manager_dup ();

  client = tp_simple_handler_new_with_am (am,
    FALSE,
    FALSE,
    "TpFsCallHandlerDemo",
    TRUE,
    new_call_channel_cb,
    NULL,
    NULL);

  tp_base_client_take_handler_filter (client,
    tp_asv_new (
       TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_CALL,
       TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        TP_PROP_CHANNEL_TYPE_CALL_INITIAL_AUDIO, G_TYPE_BOOLEAN,
          TRUE,
       NULL));

  tp_base_client_take_handler_filter (client,
    tp_asv_new (
       TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_CALL,
       TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        TP_PROP_CHANNEL_TYPE_CALL_INITIAL_VIDEO, G_TYPE_BOOLEAN,
          TRUE,
       NULL));

  tp_base_client_add_handler_capabilities_varargs (client,
    TP_IFACE_CHANNEL_TYPE_CALL "/video/h264",
    TP_IFACE_CHANNEL_TYPE_CALL "/shm",
    TP_IFACE_CHANNEL_TYPE_CALL "/ice",
    TP_IFACE_CHANNEL_TYPE_CALL "/gtalk-p2p",
    NULL);

  tp_base_client_register (client, NULL);

  g_main_loop_run (loop);

  g_object_unref (am);
  g_object_unref (client);
  g_main_loop_unref (loop);

  return 0;
}
