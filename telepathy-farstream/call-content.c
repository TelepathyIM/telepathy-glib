/*
 * call-content.c - Source for TfCallContent
 * Copyright (C) 2010 Collabora Ltd.
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

/**
 * SECTION:call-content

 * @short_description: Handle the Call interface on a Channel
 *
 * This class handles the
 * org.freedesktop.Telepathy.Channel.Interface.Call on a
 * channel using Farsight2.
 */


#include "call-content.h"

#include <telepathy-glib/util.h>
#include <telepathy-glib/interfaces.h>
#include <gst/farsight/fs-conference-iface.h>
#include <gst/farsight/fs-utils.h>
#include <gst/farsight/fs-element-added-notifier.h>

#include <stdarg.h>
#include <string.h>

#include <telepathy-glib/proxy-subclass.h>

#include "call-stream.h"
#include "tf-signals-marshal.h"
#include "utils.h"

#include "extensions/extensions.h"

struct _TfCallContent {
  TfContent parent;

  TfCallChannel *call_channel;
  FsConference *fsconference;

  TfFutureCallContent *proxy;

  FsSession *fssession;
  TpMediaStreamType media_type;

  GList *current_codecs;
  TpProxy *current_offer;
  guint current_offer_contact_handle;
  GList *current_offer_fscodecs;

  GHashTable *streams; /* NULL before getting the first streams */
  /* Streams for which we don't have a session yet*/
  GList *outstanding_streams;

  GMutex *mutex;

  /* Content protected by the Mutex */
  GPtrArray *fsstreams;

  gboolean got_codec_offer_property;

  /* VideoControl API */
  FsElementAddedNotifier *notifier;

  volatile gint bitrate;
  volatile gint mtu;
  gboolean manual_keyframes;

  guint framerate;
  guint width;
  guint height;
};

struct _TfCallContentClass{
  TfContentClass parent_class;
};


static void call_content_async_initable_init (GAsyncInitableIface *asynciface);

G_DEFINE_TYPE_WITH_CODE (TfCallContent, tf_call_content, TF_TYPE_CONTENT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                             call_content_async_initable_init))

#define TF_CALL_CONTENT_LOCK(self)   g_mutex_lock ((self)->mutex)
#define TF_CALL_CONTENT_UNLOCK(self) g_mutex_unlock ((self)->mutex)


enum
{
  PROP_TF_CHANNEL = 1,
  PROP_FS_CONFERENCE,
  PROP_FS_SESSION,
  PROP_SINK_PAD,
  PROP_MEDIA_TYPE,
  PROP_OBJECT_PATH,
  PROP_FRAMERATE,
  PROP_WIDTH,
  PROP_HEIGHT
};

enum
{
  RESOLUTION_CHANGED = 0,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};


struct CallFsStream {
  TfCallChannel *parent_channel;
  guint use_count;
  guint contact_handle;
  FsParticipant *fsparticipant;
  FsStream *fsstream;
};

static void
tf_call_content_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec);

static void tf_call_content_dispose (GObject *object);
static void tf_call_content_finalize (GObject *object);

static void tf_call_content_init_async (GAsyncInitable *initable,
    int io_priority,
    GCancellable  *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);
static gboolean tf_call_content_init_finish (GAsyncInitable *initable,
    GAsyncResult *res,
    GError **error);


static gboolean tf_call_content_set_codec_preferences (TfContent *content,
    GList *codec_preferences,
    GError **error);

static void tf_call_content_try_sending_codecs (TfCallContent *self);
static FsStream * tf_call_content_get_existing_fsstream_by_handle (
    TfCallContent *content, guint contact_handle);

static void
src_pad_added (FsStream *fsstream, GstPad *pad, FsCodec *codec,
    TfCallContent *content);

static void tf_call_content_error (TfContent *content,
    guint reason,  /* TfFutureContentRemovalReason */
    const gchar *detailed_reason,
    const gchar *message);

static void
tf_call_content_class_init (TfCallContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TfContentClass *content_class = TF_CONTENT_CLASS (klass);

  content_class->content_error = tf_call_content_error;

  object_class->dispose = tf_call_content_dispose;
  object_class->finalize = tf_call_content_finalize;
  object_class->get_property = tf_call_content_get_property;

  content_class->set_codec_preferences = tf_call_content_set_codec_preferences;

  g_object_class_override_property (object_class, PROP_TF_CHANNEL,
      "tf-channel");
  g_object_class_override_property (object_class, PROP_FS_CONFERENCE,
      "fs-conference");
  g_object_class_override_property (object_class, PROP_FS_SESSION,
      "fs-session");
  g_object_class_override_property (object_class, PROP_SINK_PAD,
      "sink-pad");
  g_object_class_override_property (object_class, PROP_MEDIA_TYPE,
      "media-type");
  g_object_class_override_property (object_class, PROP_OBJECT_PATH,
      "object-path");

  g_object_class_install_property (object_class, PROP_FRAMERATE,
    g_param_spec_uint ("framerate",
      "Framerate",
      "The framerate as indicated by the VideoControl interface"
      "or the media layer",
      0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FRAMERATE,
    g_param_spec_uint ("width",
      "Width",
      "The video width indicated by the VideoControl interface"
      "or the media layer",
      0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FRAMERATE,
    g_param_spec_uint ("height",
      "Height",
      "The video height as indicated by the VideoControl interface"
      "or the media layer",
      0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  signals[RESOLUTION_CHANGED] = g_signal_new ("resolution-changed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      _tf_marshal_VOID__UINT_UINT,
      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
}

static void
call_content_async_initable_init (GAsyncInitableIface *asynciface)
{
  asynciface->init_async = tf_call_content_init_async;
  asynciface->init_finish = tf_call_content_init_finish;
}


static void
free_content_fsstream (gpointer data)
{
  struct CallFsStream *cfs = data;

  g_object_unref (cfs->fsstream);
  _tf_call_channel_put_participant (cfs->parent_channel, cfs->fsparticipant);
  g_slice_free (struct CallFsStream, cfs);
}

static void
tf_call_content_init (TfCallContent *self)
{
  self->fsstreams = g_ptr_array_new ();

  self->mutex = g_mutex_new ();
}

static void
tf_call_content_dispose (GObject *object)
{
  TfCallContent *self = TF_CALL_CONTENT (object);

  g_debug (G_STRFUNC);

  if (self->streams)
    g_hash_table_destroy (self->streams);
  self->streams = NULL;

  if (self->fssession)
    g_object_unref (self->fssession);
  self->fssession = NULL;

  if (self->fsstreams)
    {
      while (self->fsstreams->len)
        free_content_fsstream (
            g_ptr_array_remove_index_fast (self->fsstreams, 0));
      g_ptr_array_unref (self->fsstreams);
  }
  self->fsstreams = NULL;

  if (self->notifier)
    g_object_unref (self->notifier);
  self->notifier = NULL;

  if (self->fsconference)
    _tf_call_channel_put_conference (self->call_channel,
        self->fsconference);
  self->fsconference = NULL;

  if (self->proxy)
    g_object_unref (self->proxy);
  self->proxy = NULL;

  if (G_OBJECT_CLASS (tf_call_content_parent_class)->dispose)
    G_OBJECT_CLASS (tf_call_content_parent_class)->dispose (object);
}


static void
tf_call_content_finalize (GObject *object)
{
  TfCallContent *self = TF_CALL_CONTENT (object);

  g_mutex_free (self->mutex);

  if (G_OBJECT_CLASS (tf_call_content_parent_class)->finalize)
    G_OBJECT_CLASS (tf_call_content_parent_class)->finalize (object);
}


static void
tf_call_content_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec)
{
  TfCallContent *self = TF_CALL_CONTENT (object);

  switch (property_id)
    {
    case PROP_TF_CHANNEL:
      if (self->call_channel)
        g_value_set_object (value, self->call_channel);
      break;
    case PROP_FS_CONFERENCE:
      if (self->fsconference)
        g_value_set_object (value, self->fsconference);
      break;
    case PROP_FS_SESSION:
      if (self->fssession)
        g_value_set_object (value, self->fssession);
      break;
    case PROP_SINK_PAD:
      if (self->fssession)
        g_object_get_property (G_OBJECT (self->fssession), "sink-pad", value);
      break;
    case PROP_MEDIA_TYPE:
      g_value_set_enum (value, tf_call_content_get_fs_media_type (self));
      break;
    case PROP_OBJECT_PATH:
      g_object_get_property (G_OBJECT (self->proxy), "object-path", value);
      break;
    case PROP_FRAMERATE:
      g_value_set_uint (value, self->framerate);
      break;
    case PROP_WIDTH:
      g_value_set_uint (value, self->width);
      break;
    case PROP_HEIGHT:
      g_value_set_uint (value, self->height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
create_stream (TfCallContent *self, gchar *stream_path)
{
  GError *error = NULL;
  TfCallStream *stream = tf_call_stream_new (self->call_channel, self,
      stream_path, &error);

  if (error)
    {
      /* TODO: Use per-stream errors */
      tf_content_error (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error creating the stream object: %s", error->message);
      return;
    }

  g_hash_table_insert (self->streams, stream_path, stream);
}

static void
add_stream (TfCallContent *self, const gchar *stream_path)
{

  if (!self->fsconference)
  {
    self->outstanding_streams = g_list_prepend (self->outstanding_streams,
      g_strdup (stream_path));
  } else {
    create_stream (self, g_strdup (stream_path));
  }
}

static void
update_streams (TfCallContent *self)
{
  GList *l;

  g_assert (self->fsconference);

  for (l = self->outstanding_streams ; l != NULL; l = l->next)
    create_stream (self, l->data);

  g_list_free (self->outstanding_streams);
  self->outstanding_streams = NULL;
}

static void
tpparam_to_fsparam (gpointer key, gpointer value, gpointer user_data)
{
  gchar *name = key;
  gchar *val = value;
  FsCodec *fscodec = user_data;

  fs_codec_add_optional_parameter (fscodec, name, val);
}

static GList *
tpcodecs_to_fscodecs (FsMediaType fsmediatype, const GPtrArray *tpcodecs)
{
  GList *fscodecs = NULL;
  guint i;

  for (i = 0; i < tpcodecs->len; i++)
    {
      GValueArray *tpcodec = g_ptr_array_index (tpcodecs, i);
      guint pt;
      gchar *name;
      guint clock_rate;
      guint channels;
      GHashTable *params;
      FsCodec *fscodec;

      tp_value_array_unpack (tpcodec, 5, &pt, &name, &clock_rate, &channels,
          &params);

      fscodec = fs_codec_new (pt, name, fsmediatype, clock_rate);
      fscodec->channels = channels;

      g_hash_table_foreach (params, tpparam_to_fsparam, fscodec);

      fscodecs = g_list_prepend (fscodecs, fscodec);
    }

  fscodecs = g_list_reverse (fscodecs);

  return fscodecs;
}

static void
process_codec_offer_try_codecs (TfCallContent *self, FsStream *fsstream,
    TpProxy *offer, GList *fscodecs)
{
  gboolean success = TRUE;
  GError *error = NULL;

  if (fscodecs != NULL)
    success = fs_stream_set_remote_codecs (fsstream, fscodecs, &error);

  fs_codec_list_destroy (fscodecs);

  if (success)
    {
      self->current_offer = offer;
      tf_call_content_try_sending_codecs (self);
    }
  else
    {
      tf_future_cli_call_content_codec_offer_call_reject (offer,
          -1, NULL, NULL, NULL, NULL);
      g_object_unref (offer);
    }

}

static void
process_codec_offer (TfCallContent *self, const gchar *offer_objpath,
    guint contact_handle, const GPtrArray *codecs)
{
  TpProxy *proxy;
  GError *error = NULL;
  FsStream *fsstream;
  GList *fscodecs;

  if (!tp_dbus_check_valid_object_path (offer_objpath, &error))
    {
      tf_content_error (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Invalid offer path: %s", error->message);
      g_clear_error (&error);
      return;
    }

  proxy = g_object_new (TP_TYPE_PROXY,
      "dbus-daemon", tp_proxy_get_dbus_daemon (self->proxy),
      "bus-name", tp_proxy_get_bus_name (self->proxy),
      "object-path", offer_objpath,
      NULL);
  tp_proxy_add_interface_by_id (TP_PROXY (proxy),
      TF_FUTURE_IFACE_QUARK_CALL_CONTENT_CODEC_OFFER);

  fscodecs = tpcodecs_to_fscodecs (tp_media_type_to_fs (self->media_type),
      codecs);

  fsstream = tf_call_content_get_existing_fsstream_by_handle (self,
      contact_handle);

  if (!fsstream)
    {
      g_debug ("Delaying codec offer processing");
      self->current_offer = proxy;
      self->current_offer_contact_handle = contact_handle;
      self->current_offer_fscodecs = fscodecs;
      return;
    }

  process_codec_offer_try_codecs (self, fsstream, proxy, fscodecs);
}

static void
on_content_video_keyframe_requested (TfFutureCallContent *proxy,
  gpointer user_data,
  GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);
  GstPad *pad;

  /* In case there is no session, ignore the request a new session should start
   * with sending a KeyFrame in any case */
  if (self->fssession == NULL)
    return;

  g_object_get (self->fssession, "sink-pad", &pad, NULL);

  if (pad == NULL)
    {
      g_warning ("Failed to get a pad for the keyframe request");
      return;
    }

  g_message ("Sending out a keyframe request");
  gst_pad_send_event (pad,
      gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
          gst_structure_new ("GstForceKeyUnit",
            "all-headers", G_TYPE_BOOLEAN, TRUE,
            NULL)));

  g_object_unref (pad);
}

static void
on_content_video_resolution_changed (TfFutureCallContent *proxy,
  const GValueArray *resolution,
  gpointer user_data,
  GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);
  guint width, height;

  tp_value_array_unpack ((GValueArray *)resolution, 2,
    &width, &height, NULL);

  self->width = width;
  self->height = height;

  g_signal_emit (self, signals[RESOLUTION_CHANGED], 0, width, height);

  g_message ("requested video resolution: %dx%d", width, height);
}

static void
on_content_video_bitrate_changed (TfFutureCallContent *proxy,
  guint bitrate,
  gpointer user_data,
  GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);

  g_message ("Setting bitrate to %d bits/s", bitrate);
  self->bitrate = bitrate;

  if (self->fssession != NULL && self->bitrate > 0)
    g_object_set (self->fssession, "send-bitrate", self->bitrate, NULL);
}

static void
on_content_video_framerate_changed (TfFutureCallContent *proxy,
  guint framerate,
  gpointer user_data,
  GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);
  g_message ("updated framerate requested: %d", framerate);

  self->framerate = framerate;
  g_object_notify (G_OBJECT (self), "framerate");
}

static void
on_content_video_mtu_changed (TfFutureCallContent *proxy,
  guint mtu,
  gpointer user_data,
  GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);

  g_atomic_int_set (&self->mtu, mtu);

  if (self->fsconference != NULL)
    {
      fs_element_added_notifier_remove (self->notifier,
          GST_BIN (self->fsconference));

      if (mtu > 0 || self->manual_keyframes)
        fs_element_added_notifier_add (self->notifier,
          GST_BIN (self->fsconference));
    }
}


static void
got_content_media_properties (TpProxy *proxy, GHashTable *properties,
    const GError *error, gpointer user_data, GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);
  GSimpleAsyncResult *res = user_data;
  GValueArray *gva;
  const gchar *offer_objpath;
  guint contact;
  GError *myerror = NULL;
  GPtrArray *codecs;
  guint32 packetization;
  const gchar *conference_type;
  gboolean valid;
  GList *codec_prefs;

  if (error != NULL)
    {
      tf_content_error (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error getting the Content's media properties: %s", error->message);
      g_simple_async_result_set_from_error (res, error);
      g_simple_async_result_complete (res);
      g_object_unref (res);
      return;
    }

  packetization = tp_asv_get_uint32 (properties, "Packetization", &valid);

  if (!valid)
    goto invalid_property;

  g_assert (self->fssession == NULL);

  switch (packetization)
    {
      case TF_FUTURE_CALL_CONTENT_PACKETIZATION_TYPE_RTP:
        conference_type = "rtp";
        break;
      case TF_FUTURE_CALL_CONTENT_PACKETIZATION_TYPE_RAW:
        conference_type = "raw";
        break;
      default:
        tf_content_error (TF_CONTENT (self),
            TF_FUTURE_CONTENT_REMOVAL_REASON_UNSUPPORTED, "",
            "Could not create FsConference for type %d", packetization);
        g_simple_async_result_set_error (res, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
          "Could not create FsConference for type %d", packetization);
        g_simple_async_result_complete (res);
        g_object_unref (res);
        return;
    }

  self->fsconference = _tf_call_channel_get_conference (self->call_channel,
      conference_type);
  if (!self->fsconference)
    {
      tf_content_error (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_UNSUPPORTED, "",
          "Could not create FsConference for type %s", conference_type);
      g_simple_async_result_set_error (res, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
          "Error getting the Content's properties: invalid type");
      g_simple_async_result_complete (res);
      g_object_unref (res);
      return;
    }

  self->fssession = fs_conference_new_session (self->fsconference,
      tp_media_type_to_fs (self->media_type), &myerror);

  if (!self->fssession)
    {
      tf_content_error (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_UNSUPPORTED, "",
          "Could not create FsSession: %s", myerror->message);
      g_simple_async_result_set_from_error (res, myerror);
      g_simple_async_result_complete (res);
      g_clear_error (&myerror);
      g_object_unref (res);
      return;
    }

  if (self->notifier != NULL)
    fs_element_added_notifier_add (self->notifier,
      GST_BIN (self->fsconference));

  /* Now process outstanding streams */
  update_streams (self);

  gva = tp_asv_get_boxed (properties, "CodecOffer",
    TF_FUTURE_STRUCT_TYPE_CODEC_OFFERING);
  if (gva == NULL)
    {
      goto invalid_property;
    }

  codec_prefs = fs_utils_get_default_codec_preferences (
      GST_ELEMENT (self->fsconference));

  if (codec_prefs)
    {
      if (!fs_session_set_codec_preferences (self->fssession, codec_prefs,
              &myerror))
        {
          g_warning ("Could not set codec preference: %s", myerror->message);
          g_clear_error (&myerror);
        }
    }

  /* First complete so we get signalled and the preferences can be set, then
   * start looking at the offer */
  g_simple_async_result_set_op_res_gboolean (res, TRUE);
  g_simple_async_result_complete (res);
  g_object_unref (res);

  tp_value_array_unpack (gva, 3, &offer_objpath, &contact, &codecs);

  process_codec_offer (self, offer_objpath, contact, codecs);

  self->got_codec_offer_property = TRUE;

  return;

 invalid_property:
  tf_content_error_literal (TF_CONTENT (self),
      TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
      "Error getting the Content's properties: invalid type");
  g_simple_async_result_set_error (res, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
      "Error getting the Content's properties: invalid type");
  g_simple_async_result_complete (res);
  g_object_unref (res);
  return;
}

static void
setup_content_media_properties (TfCallContent *self,
  TpProxy *proxy,
  GSimpleAsyncResult *res)
{
  tp_cli_dbus_properties_call_get_all (proxy, -1,
      TF_FUTURE_IFACE_CALL_CONTENT_INTERFACE_MEDIA,
      got_content_media_properties, res, NULL, G_OBJECT (self));
}

static gboolean
object_has_property (GObject *object, const gchar *property)
{
  return g_object_class_find_property (G_OBJECT_GET_CLASS (object),
      property) != NULL;
}

static void
content_video_element_added (FsElementAddedNotifier *notifier,
  GstBin *conference,
  GstElement *element,
  TfCallContent *self)
{
  gint mtu = g_atomic_int_get (&self->mtu);

  if (G_UNLIKELY (mtu == 0 && !self->manual_keyframes))
    return;

  if (G_UNLIKELY (mtu > 0 && object_has_property (G_OBJECT (element), "mtu")))
    {
      g_message ("Setting %d as mtu on payloader", mtu);
      g_object_set (element, "mtu", mtu, NULL);
    }

  if (G_UNLIKELY (self->manual_keyframes))
    {
      if (object_has_property (G_OBJECT (element), "key-int-max"))
        {
          g_message ("Setting key-int-max to max uint");
          g_object_set (element, "key-int-max", G_MAXUINT, NULL);
        }

      if (object_has_property (G_OBJECT (element), "intra-period"))
        {
          g_message ("Setting intra-period to 0");
          g_object_set (element, "intra-period", 0, NULL);
        }
    }
}

static void
got_content_video_control_properties (TpProxy *proxy, GHashTable *properties,
    const GError *error, gpointer user_data, GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);
  GSimpleAsyncResult *res = user_data;
  guint32 bitrate, mtu;
  gboolean valid;
  gboolean manual_keyframes;

  if (error)
    {
      tf_content_error (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error getting the Content's VideoControl properties: %s",
          error->message);
      g_simple_async_result_set_from_error (res, error);
      goto error;
    }

  if (properties == NULL)
    {
      tf_content_error_literal (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error getting the Content's VideoControl properties: "
          "there are none");
      g_simple_async_result_set_error (res, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
          "Error getting the VideoControl Content's properties: "
          "there are none");
      goto error;
    }


  /* Only get the various variables, we will not have an FsSession untill the
   * media properties are retrieved so no need to act just yet */
  bitrate = tp_asv_get_uint32 (properties, "Bitrate", &valid);
  if (valid)
    self->bitrate = bitrate;

  mtu = tp_asv_get_uint32 (properties, "MTU", &valid);
  if (valid)
    self->mtu = mtu;

  manual_keyframes = tp_asv_get_boolean (properties,
    "ManualKeyFrames", &valid);
  if (valid)
    self->manual_keyframes = manual_keyframes;

  self->notifier = fs_element_added_notifier_new ();
  g_signal_connect (self->notifier, "element-added",
    G_CALLBACK (content_video_element_added), self);

  setup_content_media_properties (self, proxy, res);
  return;

error:
  g_simple_async_result_complete (res);
  g_object_unref (res);
  return;
}


static void
setup_content_video_control (TfCallContent *self,
  TpProxy *proxy,
  GSimpleAsyncResult *res)
{
  GError *error = NULL;

  tp_proxy_add_interface_by_id (proxy,
    TF_FUTURE_IFACE_QUARK_CALL_CONTENT_INTERFACE_VIDEO_CONTROL);

  if (tf_future_cli_call_content_interface_video_control_connect_to_key_frame_requested (
      TF_FUTURE_CALL_CONTENT (proxy),
      on_content_video_keyframe_requested,
      NULL, NULL, G_OBJECT (self), &error) == NULL)
    goto connect_failed;

  if (tf_future_cli_call_content_interface_video_control_connect_to_video_resolution_changed (
      TF_FUTURE_CALL_CONTENT (proxy),
      on_content_video_resolution_changed,
      NULL, NULL, G_OBJECT (self), &error) == NULL)
    goto connect_failed;

  if (tf_future_cli_call_content_interface_video_control_connect_to_bitrate_changed (
      TF_FUTURE_CALL_CONTENT (proxy),
      on_content_video_bitrate_changed,
      NULL, NULL, G_OBJECT (self), NULL) == NULL)
    goto connect_failed;

  if (tf_future_cli_call_content_interface_video_control_connect_to_framerate_changed (
      TF_FUTURE_CALL_CONTENT (proxy),
      on_content_video_framerate_changed,
      NULL, NULL, G_OBJECT (self), NULL) == NULL)
    goto connect_failed;

  if (tf_future_cli_call_content_interface_video_control_connect_to_mtu_changed (
      TF_FUTURE_CALL_CONTENT (proxy),
      on_content_video_mtu_changed,
      NULL, NULL, G_OBJECT (self), NULL) == NULL)
    goto connect_failed;

  printf ("Connection to the video interface\n");

  tp_cli_dbus_properties_call_get_all (proxy, -1,
      TF_FUTURE_IFACE_CALL_CONTENT_INTERFACE_MEDIA,
      got_content_video_control_properties, res, NULL, G_OBJECT (self));

  return;

connect_failed:
  tf_content_error (TF_CONTENT (self),
      TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
      "Error getting the Content's VideoControl properties: %s",
      error->message);
  g_simple_async_result_take_error (res, error);
  g_simple_async_result_complete (res);
  g_object_unref (res);
}

static void
new_codec_offer (TfFutureCallContent *proxy,
    guint arg_Contact,
    const gchar *arg_Offer,
    const GPtrArray *arg_Codecs,
    gpointer user_data,
    GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);

  /* Ignore signals before we get the first codec offer property */
  if (!self->got_codec_offer_property)
    return;

  if (self->current_offer) {
    g_object_unref (self->current_offer);
    fs_codec_list_destroy (self->current_offer_fscodecs);
    self->current_offer = NULL;
    self->current_offer_fscodecs = NULL;
  }

  process_codec_offer (self, arg_Offer, arg_Contact, arg_Codecs);
}


static void
got_content_properties (TpProxy *proxy, GHashTable *out_Properties,
    const GError *error, gpointer user_data, GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);
  GSimpleAsyncResult *res = user_data;
  gboolean valid;
  GPtrArray *streams;
  GError *myerror = NULL;
  guint i;
  const gchar * const *interfaces;
  gboolean got_media_interface = FALSE;;
  gboolean got_video_control_interface = FALSE;;

  if (error)
    {
      tf_content_error (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error getting the Content's properties: %s", error->message);
      g_simple_async_result_set_from_error (res, error);
      g_simple_async_result_complete (res);
      g_object_unref (res);
      return;
    }

  if (!out_Properties)
    {
      tf_content_error_literal (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error getting the Content's properties: there are none");
      g_simple_async_result_set_error (res, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
          "Error getting the Content's properties: there are none");
      g_simple_async_result_complete (res);
      g_object_unref (res);
      return;
    }

  interfaces = tp_asv_get_strv (out_Properties, "Interfaces");

  if (interfaces == NULL)
    {
      tf_content_error_literal (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Content does not have the Interfaces property, "
          "but HardwareStreaming was NOT true");
      g_simple_async_result_set_error (res, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
          "Content does not have the Interfaces property, "
          "but HardwareStreaming was NOT true");
      g_simple_async_result_complete (res);
      g_object_unref (res);
      return;
    }

  for (i = 0; interfaces[i]; i++)
    {
      if (!strcmp (interfaces[i],
            TF_FUTURE_IFACE_CALL_CONTENT_INTERFACE_MEDIA))
        got_media_interface = TRUE;

      if (!strcmp (interfaces[i],
            TF_FUTURE_IFACE_CALL_CONTENT_INTERFACE_VIDEO_CONTROL))
        got_video_control_interface = TRUE;
    }

  if (!got_media_interface)
    {
      tf_content_error_literal (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Content does not have the media interface,"
          " but HardwareStreaming was NOT true");
      g_simple_async_result_set_error (res, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
          "Content does not have the media interface,"
          " but HardwareStreaming was NOT true");
      g_simple_async_result_complete (res);
      g_object_unref (res);
      return;
    }

  self->media_type = tp_asv_get_uint32 (out_Properties, "Type", &valid);
  if (!valid)
    goto invalid_property;


  streams = tp_asv_get_boxed (out_Properties, "Streams",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST);
  if (!streams)
    goto invalid_property;

  self->streams = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, g_object_unref);

  for (i = 0; i < streams->len; i++)
    add_stream (self, g_ptr_array_index (streams, i));

  tp_proxy_add_interface_by_id (TP_PROXY (self->proxy),
      TF_FUTURE_IFACE_QUARK_CALL_CONTENT_INTERFACE_MEDIA);


  tf_future_cli_call_content_interface_media_connect_to_new_codec_offer (
      TF_FUTURE_CALL_CONTENT (proxy), new_codec_offer, NULL, NULL,
      G_OBJECT (self), &myerror);

  if (myerror)
    {
      tf_content_error (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error connectiong to NewCodecOffer signal: %s",
          myerror->message);
      g_simple_async_result_set_from_error (res, myerror);
      g_simple_async_result_complete (res);
      g_object_unref (res);
      g_clear_error (&myerror);
      return;
    }

  if (got_video_control_interface)
    setup_content_video_control (self, proxy, res);
  else
    setup_content_media_properties (self, proxy, res);

  return;

 invalid_property:
  tf_content_error_literal (TF_CONTENT (self), TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
      "", "Error getting the Content's properties: invalid type");
  g_simple_async_result_set_error (res, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
      "Error getting the Content's properties: invalid type");
  g_simple_async_result_complete (res);
  g_object_unref (res);
  return;
}

static void
streams_added (TfFutureCallContent *proxy,
    const GPtrArray *arg_Streams,
    gpointer user_data,
    GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);
  guint i;

  /* Ignore signals before we got the "Contents" property to avoid races that
   * could cause the same content to be added twice
   */

  if (!self->streams)
    return;

  for (i = 0; i < arg_Streams->len; i++)
    add_stream (self, g_ptr_array_index (arg_Streams, i));
}

static void
streams_removed (TfFutureCallContent *proxy,
    const GPtrArray *arg_Streams,
    gpointer user_data,
    GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);
  guint i;

  if (!self->streams)
    return;

  for (i = 0; i < arg_Streams->len; i++)
    g_hash_table_remove (self->streams, g_ptr_array_index (arg_Streams, i));
}


static void
tf_call_content_init_async (GAsyncInitable *initable,
    int io_priority,
    GCancellable  *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TfCallContent *self = TF_CALL_CONTENT (initable);
  GError *myerror = NULL;
  GSimpleAsyncResult *res;

  if (cancellable != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback, user_data,
          G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
          "TfCallChannel initialisation does not support cancellation");
      return;
    }

  tf_future_cli_call_content_connect_to_streams_added (
      TF_FUTURE_CALL_CONTENT (self->proxy), streams_added, NULL, NULL,
      G_OBJECT (self), &myerror);
  if (myerror)
    {
      tf_content_error (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error connectiong to StreamAdded signal: %s", myerror->message);
      g_simple_async_report_gerror_in_idle (G_OBJECT (self), callback,
          user_data, myerror);
      return;
    }

  tf_future_cli_call_content_connect_to_streams_removed (
      TF_FUTURE_CALL_CONTENT (self->proxy), streams_removed, NULL, NULL,
      G_OBJECT (self), &myerror);
  if (myerror)
    {
      tf_content_error (TF_CONTENT (self),
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error connectiong to StreamRemoved signal: %s", myerror->message);
      g_simple_async_report_gerror_in_idle (G_OBJECT (self), callback,
          user_data, myerror);
      return;
    }


  res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      tf_call_content_init_async);

  tp_cli_dbus_properties_call_get_all (self->proxy, -1,
      TF_FUTURE_IFACE_CALL_CONTENT, got_content_properties, res,
      NULL, G_OBJECT (self));
}

static gboolean
tf_call_content_init_finish (GAsyncInitable *initable,
    GAsyncResult *res,
    GError **error)
{
  GSimpleAsyncResult *simple_res;

  g_return_val_if_fail (g_simple_async_result_is_valid (res,
          G_OBJECT (initable), tf_call_content_init_async), FALSE);
  simple_res = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (simple_res, error))
    return FALSE;

  return g_simple_async_result_get_op_res_gboolean (simple_res);
}

TfCallContent *
tf_call_content_new_async (TfCallChannel *call_channel,
    const gchar *object_path, GError **error,
    GAsyncReadyCallback callback, gpointer user_data)
{
  TfCallContent *self;
  TfFutureCallContent *proxy = tf_future_call_content_new (
      call_channel->proxy, object_path, error);

  if (!proxy)
    return NULL;

  self = g_object_new (TF_TYPE_CALL_CONTENT, NULL);

  self->call_channel = call_channel;
  self->proxy = proxy;

  g_async_initable_init_async (G_ASYNC_INITABLE (self), 0, NULL,
      callback, user_data);

  return g_object_ref (self);
}



static GPtrArray *
fscodecs_to_tpcodecs (GList *codecs)
{
  GPtrArray *tpcodecs = g_ptr_array_new ();
  GList *item;

  for (item = codecs; item; item = item->next)
    {
      FsCodec *fscodec = item->data;
      GValue tpcodec = { 0, };
      GHashTable *params;
      GList *param_item;

      params = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

      for (param_item = fscodec->optional_params;
           param_item;
           param_item = param_item->next)
        {
          FsCodecParameter *param = (FsCodecParameter *) param_item->data;

          g_hash_table_insert (params, g_strdup (param->name),
                               g_strdup (param->value));
        }

      g_value_init (&tpcodec, TF_FUTURE_STRUCT_TYPE_CODEC);
      g_value_take_boxed (&tpcodec,
          dbus_g_type_specialized_construct (TF_FUTURE_STRUCT_TYPE_CODEC));

      dbus_g_type_struct_set (&tpcodec,
          0, fscodec->id,
          1, fscodec->encoding_name,
          2, fscodec->clock_rate,
          3, fscodec->channels,
          4, params,
          G_MAXUINT);


      g_hash_table_destroy (params);

      g_ptr_array_add (tpcodecs, g_value_get_boxed (&tpcodec));
    }

  return tpcodecs;
}

static void
tf_call_content_try_sending_codecs (TfCallContent *self)
{
  gboolean ready;
  GList *codecs;
  GPtrArray *tpcodecs;

  if (self->current_offer_fscodecs != NULL)
  {
    g_debug ("Ignoring updated codecs unprocessed offer outstanding");
    return;
  }

  g_debug ("updating local codecs");

  if (TF_CONTENT (self)->sending_count == 0)
    ready = TRUE;
  else
    g_object_get (self->fssession, "codecs-ready", &ready, NULL);

  if (!ready)
    return;

  g_object_get (self->fssession, "codecs", &codecs, NULL);

  if (fs_codec_list_are_equal (codecs, self->current_codecs))
    {
      fs_codec_list_destroy (codecs);
      return;
    }

  tpcodecs = fscodecs_to_tpcodecs (codecs);

  if (self->current_offer)
    {
      tf_future_cli_call_content_codec_offer_call_accept (self->current_offer,
          -1, tpcodecs, NULL, NULL, NULL, NULL);

      g_object_unref (self->current_offer);
      self->current_offer = NULL;
    }
  else
    {
      tf_future_cli_call_content_interface_media_call_update_codecs (
          self->proxy, -1, tpcodecs, NULL, NULL, NULL, NULL);
    }

  g_boxed_free (TF_FUTURE_ARRAY_TYPE_CODEC_LIST, tpcodecs);
}

gboolean
tf_call_content_bus_message (TfCallContent *content,
    GstMessage *message)
{
  const GstStructure *s;
  gboolean ret = FALSE;
  const gchar *debug;
  GHashTableIter iter;
  gpointer key, value;

  if (!content->fssession)
    return FALSE;

  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return FALSE;

  s = gst_message_get_structure (message);

  if (gst_structure_has_name (s, "farsight-error"))
    {
      GObject *object;
      const GValue *value = NULL;

      value = gst_structure_get_value (s, "src-object");
      object = g_value_get_object (value);

      if (object == (GObject*) content->fssession)
        {
          const gchar *msg;
          FsError errorno;
          GEnumClass *enumclass;
          GEnumValue *enumvalue;

          value = gst_structure_get_value (s, "error-no");
          errorno = g_value_get_enum (value);
          msg = gst_structure_get_string (s, "error-msg");
          debug = gst_structure_get_string (s, "debug-msg");

          enumclass = g_type_class_ref (FS_TYPE_ERROR);
          enumvalue = g_enum_get_value (enumclass, errorno);
          g_warning ("error (%s (%d)): %s : %s",
              enumvalue->value_nick, errorno, msg, debug);
          g_type_class_unref (enumclass);

          tf_content_error_literal (TF_CONTENT (content),
              TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "", msg);

          ret = TRUE;
        }
    }
  else if (gst_structure_has_name (s, "farsight-codecs-changed"))
    {
      FsSession *fssession;
      const GValue *value;

      value = gst_structure_get_value (s, "session");
      fssession = g_value_get_object (value);

      if (fssession != content->fssession)
        return FALSE;

      g_debug ("Codecs changed");

      tf_call_content_try_sending_codecs (content);

      ret = TRUE;
    }

  g_hash_table_iter_init (&iter, content->streams);
  while (g_hash_table_iter_next (&iter, &key, &value))
    if (tf_call_stream_bus_message (value, message))
      return TRUE;

  return ret;
}

static void
tf_call_content_error (TfContent *content,
    guint reason,  /* TfFutureContentRemovalReason */
    const gchar *detailed_reason,
    const gchar *message)
{
  TfCallContent *self = TF_CALL_CONTENT (content);

  g_warning ("%s", message);
  tf_future_cli_call_content_call_remove (
      self->proxy, -1, reason, detailed_reason, message, NULL, NULL,
      NULL, NULL);
}



static FsStream *
tf_call_content_get_existing_fsstream_by_handle (TfCallContent *content,
    guint contact_handle)
{
  guint i;

  TF_CALL_CONTENT_LOCK (content);

  for (i = 0; i < content->fsstreams->len; i++)
    {
      struct CallFsStream *cfs = g_ptr_array_index (content->fsstreams, i);

      if (cfs->contact_handle == contact_handle)
        {
          cfs->use_count++;
          TF_CALL_CONTENT_UNLOCK (content);
          return cfs->fsstream;
        }
    }

  TF_CALL_CONTENT_UNLOCK (content);

  return NULL;
}


FsStream *
_tf_call_content_get_fsstream_by_handle (TfCallContent *content,
    guint contact_handle,
    const gchar *transmitter,
    guint stream_transmitter_n_parameters,
    GParameter *stream_transmitter_parameters,
    GError **error)
{
  struct CallFsStream *cfs;
  FsParticipant *p;
  FsStream *s;

  s = tf_call_content_get_existing_fsstream_by_handle (content,
      contact_handle);
  if (s)
    return s;

  p = _tf_call_channel_get_participant (content->call_channel,
      content->fsconference, contact_handle, error);
  if (!p)
    return NULL;

  s = fs_session_new_stream (content->fssession, p, FS_DIRECTION_RECV,
      transmitter, stream_transmitter_n_parameters,
      stream_transmitter_parameters, error);
  if (!s)
    {
      _tf_call_channel_put_participant (content->call_channel, p);
      return NULL;
    }

  cfs = g_slice_new (struct CallFsStream);
  cfs->use_count = 1;
  cfs->contact_handle = contact_handle;
  cfs->parent_channel = content->call_channel;
  cfs->fsparticipant = p;
  cfs->fsstream = s;

  tp_g_signal_connect_object (s, "src-pad-added",
      G_CALLBACK (src_pad_added), content, 0);

  g_ptr_array_add (content->fsstreams, cfs);
  if (content->current_offer != NULL
      && content->current_offer_contact_handle == contact_handle)
  {
    GList *codecs = content->current_offer_fscodecs;
    TpProxy *current_offer = content->current_offer;
    content->current_offer_fscodecs = NULL;
    content->current_offer = NULL;

    /* ownership transfers to try_codecs */
    process_codec_offer_try_codecs (content, s,
        current_offer, codecs);
  }

  return s;
}

void
_tf_call_content_put_fsstream (TfCallContent *content, FsStream *fsstream)
{
  guint i;
  struct CallFsStream *cfs = NULL;

  TF_CALL_CONTENT_LOCK (content);
  for (i = 0; i < content->fsstreams->len; i++)
    {
      struct CallFsStream *cfs = g_ptr_array_index (content->fsstreams, i);

      if (cfs->fsstream == fsstream)
        {
          cfs->use_count--;
          if (cfs->use_count <= 0)
            cfs = g_ptr_array_remove_index_fast (content->fsstreams, i);
          break;
        }
    }
  TF_CALL_CONTENT_UNLOCK (content);

  if (cfs)
    free_content_fsstream (cfs);
}

FsMediaType
tf_call_content_get_fs_media_type (TfCallContent *content)
{
  return tp_media_type_to_fs (content->media_type);
}

static void
src_pad_added (FsStream *fsstream, GstPad *pad, FsCodec *codec,
    TfCallContent *content)
{
  guint handle = 0;
  guint i;

  TF_CALL_CONTENT_LOCK (content);

  if (!content->fsstreams)
    {
      TF_CALL_CONTENT_UNLOCK (content);
      return;
    }

  for (i = 0; i < content->fsstreams->len; i++)
    {
      struct CallFsStream *cfs = g_ptr_array_index (content->fsstreams, i);
      if (cfs->fsstream == fsstream)
        {
          handle = cfs->contact_handle;
          break;
        }
    }

  TF_CALL_CONTENT_UNLOCK (content);

  _tf_content_emit_src_pad_added (TF_CONTENT (content), handle,
      fsstream, pad, codec);
}


static gboolean
tf_call_content_set_codec_preferences (TfContent *content,
    GList *codec_preferences,
    GError **error)
{
  TfCallContent *self;

  g_return_val_if_fail (TF_IS_CALL_CONTENT (content), FALSE);
  self = TF_CALL_CONTENT (content);

  if (self->fssession == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
          "TfCallContent not initialised yet");
      return FALSE;
    }

  if (self->got_codec_offer_property)
    {
      g_warning ("Too late, already got the initial codec offer");
      return FALSE;
    }


  return fs_session_set_codec_preferences (self->fssession, codec_preferences,
      error);
}
