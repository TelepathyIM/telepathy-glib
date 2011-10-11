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

#include <gst/gst.h>
#include <telepathy-glib/telepathy-glib.h>
#include <extensions/extensions.h>
#include <farstream/fs-element-added-notifier.h>
#include <farstream/fs-utils.h>
#include <telepathy-farstream/telepathy-farstream.h>

typedef struct {
  GstElement *pipeline;
  guint buswatch;
  TpChannel *proxy;
  TfChannel *channel;
  GList *notifiers;

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
        element = gst_parse_bin_from_description (
          "audioconvert ! audioresample ! audioconvert ! autoaudiosink",
            TRUE, NULL);
        break;
      case FS_MEDIA_TYPE_VIDEO:
        element = gst_parse_bin_from_description (
          "ffmpegcolorspace ! videoscale ! autovideosink",
          TRUE, NULL);
        break;
      default:
        g_warning ("Unknown media type");
        return;
    }

  gst_bin_add (GST_BIN (context->pipeline), element);
  sinkpad = gst_element_get_pad (element, "sink");
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

static GstElement *
setup_video_source (ChannelContext *context, TfContent *content)
{
  GstElement *result, *input, *rate, *scaler, *colorspace, *capsfilter;
  GstCaps *caps;
  guint framerate = 0, width = 0, height = 0;
  GstPad *pad, *ghost;

  result = gst_bin_new ("video_input");
  input = gst_element_factory_make ("autovideosrc", NULL);
  rate = gst_element_factory_make ("videomaxrate", NULL);
  scaler = gst_element_factory_make ("videoscale", NULL);
  colorspace = gst_element_factory_make ("colorspace", NULL);
  capsfilter = gst_element_factory_make ("capsfilter", NULL);

  g_assert (input && rate && scaler && colorspace && capsfilter);
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

  caps = gst_caps_new_simple ("video/x-raw-yuv",
      "width", G_TYPE_INT, width,
      "height", G_TYPE_INT, height,
      "framerate", GST_TYPE_FRACTION, framerate, 1,
      NULL);

  g_object_set (G_OBJECT (capsfilter), "caps", caps, NULL);

  gst_bin_add_many (GST_BIN (result), input, rate, scaler,
      colorspace, capsfilter, NULL);
  g_assert (gst_element_link_many (input, rate, scaler,
      colorspace, capsfilter, NULL));

  pad = gst_element_get_static_pad (capsfilter, "src");
  g_assert (pad != NULL);

  ghost = gst_ghost_pad_new ("src", pad);
  gst_element_add_pad (result, ghost);

  g_object_unref (pad);

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


static void
content_added_cb (TfChannel *channel,
    TfContent *content,
    gpointer user_data)
{
  GstPad *srcpad, *sinkpad;
  FsMediaType mtype;
  GstElement *element;
  GstStateChangeReturn ret;
  ChannelContext *context = user_data;

  g_debug ("Content added");

  g_object_get (content,
    "sink-pad", &sinkpad,
    "media-type", &mtype,
    NULL);

  switch (mtype)
    {
      case FS_MEDIA_TYPE_AUDIO:
        element = gst_parse_bin_from_description (
          "audiotestsrc is-live=1 ! audio/x-raw-int,rate=8000 ! queue"
          " ! audioconvert ! audioresample ! audioconvert ",

            TRUE, NULL);
        break;
      case FS_MEDIA_TYPE_VIDEO:
        element = setup_video_source (context, content);
        break;
      default:
        g_warning ("Unknown media type");
        goto out;
    }

  g_signal_connect (content, "src-pad-added",
    G_CALLBACK (src_pad_added_cb), context);

  gst_bin_add (GST_BIN (context->pipeline), element);
  srcpad = gst_element_get_pad (element, "src");

  if (GST_PAD_LINK_FAILED (gst_pad_link (srcpad, sinkpad)))
    {
      tp_channel_close_async (TP_CHANNEL (context->proxy), NULL, NULL);
      g_warning ("Couldn't link source pipeline !?");
      return;
    }

  ret = gst_element_set_state (element, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE)
    {
      tp_channel_close_async (TP_CHANNEL (context->proxy), NULL, NULL);
      g_warning ("source pipeline failed to start!?");
      return;
    }

  g_object_unref (srcpad);
out:
  g_object_unref (sinkpad);
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

  g_debug ("New TfChannel");

  context->channel = TF_CHANNEL (g_async_initable_new_finish (
      G_ASYNC_INITABLE (source), result, NULL));


  if (context->channel == NULL)
    {
      g_warning ("Failed to create channel");
      return;
    }

  g_debug ("Adding timeout");
  g_timeout_add_seconds (5, dump_pipeline_cb, context);

  g_signal_connect (context->channel, "fs-conference-added",
    G_CALLBACK (conference_added_cb), context);

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

  tf_future_cli_channel_type_call_call_accept (proxy, -1,
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
  TpDBusDaemon *bus;

  g_type_init ();
  tf_init ();
  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  bus = tp_dbus_daemon_dup (NULL);

  client = tp_simple_handler_new (bus,
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
          TF_FUTURE_IFACE_CHANNEL_TYPE_CALL,
       TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        TF_FUTURE_PROP_CHANNEL_TYPE_CALL_INITIAL_AUDIO, G_TYPE_BOOLEAN,
          TRUE,
       NULL));

  tp_base_client_take_handler_filter (client,
    tp_asv_new (
       TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TF_FUTURE_IFACE_CHANNEL_TYPE_CALL,
       TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        TF_FUTURE_PROP_CHANNEL_TYPE_CALL_INITIAL_VIDEO, G_TYPE_BOOLEAN,
          TRUE,
       NULL));

  tp_base_client_add_handler_capabilities_varargs (client,
    TF_FUTURE_IFACE_CHANNEL_TYPE_CALL "/video/h264",
    TF_FUTURE_IFACE_CHANNEL_TYPE_CALL "/shm",
    TF_FUTURE_IFACE_CHANNEL_TYPE_CALL "/ice",
    TF_FUTURE_IFACE_CHANNEL_TYPE_CALL "/gtalk-p2p",
    NULL);

  tp_base_client_register (client, NULL);

  g_main_loop_run (loop);

  g_object_unref (bus);
  g_object_unref (client);
  g_main_loop_unref (loop);

  return 0;
}
