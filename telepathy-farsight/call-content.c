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
 * SECTION:content

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

#include <stdarg.h>
#include <string.h>

#include <telepathy-glib/proxy-subclass.h>

#include "extensions/extensions.h"

#include "tf-signals-marshal.h"
#include "utils.h"

static void
tf_call_content_error (TfCallContent *content,
    TfFutureContentRemovalReason reason,
    const gchar *detailed_reason,
    const gchar *message_format, ...) G_GNUC_PRINTF (4, 5);


struct _TfCallContent {
  GObject parent;

  TfCallChannel *call_channel;
  FsConference *fsconference;

  TfFutureCallContent *proxy;

  FsSession *fssession;
  TpMediaStreamType media_type;

  GList *current_codecs;
  TpProxy *current_offer;

  GHashTable *streams; /* NULL before getting the first streams */

  GPtrArray *fsstreams;

  gboolean got_codec_offer_property;
};

struct _TfCallContentClass{
  GObjectClass parent_class;
};


G_DEFINE_TYPE (TfCallContent, tf_call_content, G_TYPE_OBJECT);


enum
{
  PROP_FS_CONFERENCE = 1,
  PROP_FS_SESSION
};

enum
{
  SIGNAL_COUNT
};

struct CallFsStream {
  TfCallChannel *parent_channel;
  guint use_count;
  guint contact_handle;
  FsParticipant *fsparticipant;
  FsStream *fsstream;
};

// static guint signals[SIGNAL_COUNT] = {0};

static void
tf_call_content_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec);

static void tf_call_content_dispose (GObject *object);


static void
tf_call_content_class_init (TfCallContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tf_call_content_dispose;
  object_class->get_property = tf_call_content_get_property;

  g_object_class_install_property (object_class, PROP_FS_SESSION,
      g_param_spec_object ("fs-conference",
          "Farsight2 FsConference used by the Content ",
          "The Farsight2 conference for this content "
          "(could be the same as other contents)",
          FS_TYPE_CONFERENCE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FS_SESSION,
      g_param_spec_object ("fs-session",
          "Farsight2 FsSession ",
          "The Farsight2 session for this content",
          FS_TYPE_SESSION,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
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
  self->fsstreams = g_ptr_array_new_with_free_func (free_content_fsstream);
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
    g_ptr_array_unref (self->fsstreams);
  self->fsstreams = NULL;

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
tf_call_content_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec)
{
  TfCallContent *self = TF_CALL_CONTENT (object);

  switch (property_id)
    {
    case PROP_FS_CONFERENCE:
      if (self->fsconference)
        g_value_set_object (value, self->fsconference);
      break;
    case PROP_FS_SESSION:
      if (self->fssession)
        g_value_set_object (value, self->fssession);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
add_stream (TfCallContent *self, const gchar *stream_path)
{
  GError *error = NULL;
  // TfCallStream *stream = tf_call_stream_new (self,
  //    stream_path, &error);

  if (error)
    {
      /* TODO: Use per-stream errors */
      tf_call_content_error (self, TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
          "", "Error creating the stream object: %s", error->message);
      return;
    }

  // g_hash_table_insert (self->streams, g_strdup (stream_path), stream);

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
process_codec_offer (TfCallContent *self, const gchar *offer_objpath,
    guint contact, const GPtrArray *codecs)
{
  TpProxy *proxy;
  GError *error = NULL;
  GList *fscodecs;

  if (!tp_dbus_check_valid_object_path (offer_objpath, &error))
    {
      tf_call_content_error (self,  TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
          "", "Invalid offer path: %s", error->message);
      g_clear_error (&error);
      return;
    }

  proxy = g_object_new (TP_TYPE_PROXY,
      "dbus-daemon", tp_proxy_get_dbus_daemon (self->proxy),
      "bus-name", tp_proxy_get_bus_name (self->proxy),
      "object-path", offer_objpath,
      NULL);

  fscodecs = tpcodecs_to_fscodecs (tp_media_type_to_fs (self->media_type),
      codecs);

  /* Find the right FsStream object for this contact  and try setting the codecs
   * if possible
   */

  fs_codec_list_destroy (fscodecs);


  g_object_unref (proxy);
}

static void
got_codec_offer_property (TpProxy *proxy, const GValue *out_Value,
    const GError *error, gpointer user_data, GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);
  GValueArray *gva;
  const gchar *offer_objpath;
  guint contact;
  GPtrArray *codecs;

  if (!G_VALUE_HOLDS (out_Value, TF_FUTURE_STRUCT_TYPE_CODEC_OFFERING))
    {
      tf_call_content_error (self, TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
          "", "Error getting the Content's properties: invalid type");
      return;
    }

  gva = g_value_get_boxed (out_Value);

  tp_value_array_unpack (gva, 3, &offer_objpath, &contact, &codecs);

  process_codec_offer (self, offer_objpath, contact, codecs);

  self->got_codec_offer_property = TRUE;
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

  process_codec_offer (self, arg_Offer, arg_Contact, arg_Codecs);
}


static void
got_content_properties (TpProxy *proxy, GHashTable *out_Properties,
    const GError *error, gpointer user_data, GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);
  gboolean valid;
  GPtrArray *streams;
  GError *myerror = NULL;
  guint i;
  const gchar * const *interfaces;
  const gchar * conference_type;
  gboolean got_media_interface = FALSE;;

  if (error)
    {
      tf_call_content_error (self, TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
          "", "Error getting the Content's properties: %s", error->message);
      return;
    }

  if (!out_Properties)
    {
      tf_call_content_error (self, TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
          "", "Error getting the Content's properties: there are none");
      return;
    }

  interfaces = tp_asv_get_strv (out_Properties, "Interfaces");

  for (i = 0; interfaces[i]; i++)
    if (!strcmp (interfaces[i], TF_FUTURE_IFACE_CALL_CONTENT_INTERFACE_MEDIA))
      {
        got_media_interface = TRUE;
        break;
      }

  if (!got_media_interface)
    {
      tf_call_content_error (self, TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
          "", "Content does not have the media interface,"
          " but HardwareStreaming was NOT true");
      return;
    }


  self->media_type = tp_asv_get_uint32 (out_Properties, "Type", &valid);
  if (!valid)
    goto invalid_property;


  conference_type = tp_asv_get_string (out_Properties, "Packetization");
  if (!conference_type)
    goto invalid_property;

  streams = tp_asv_get_boxed (out_Properties, "Streams",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST);
  if (!streams)
    goto invalid_property;

  g_assert (self->fssession == NULL);

  self->fsconference = _tf_call_channel_get_conference (self->call_channel,
      conference_type);
  if (!self->fsconference)
    {
      tf_call_content_error (self, TF_FUTURE_CONTENT_REMOVAL_REASON_UNSUPPORTED,
          "", "Could not create FsConference for type %s", conference_type);
      return;
    }

  self->fssession = fs_conference_new_session (self->fsconference,
      tp_media_type_to_fs (self->media_type), &myerror);

  if (!self->fssession)
    {
      tf_call_content_error (self, TF_FUTURE_CONTENT_REMOVAL_REASON_UNSUPPORTED,
          "", "Could not create FsSession: %s", myerror->message);
      g_clear_error (&myerror);
      return;
    }

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
      tf_call_content_error (self, TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error connectiong to NewCodecOffer signal: %s",
          myerror->message);
      g_clear_error (&myerror);
      return;
    }

  tp_cli_dbus_properties_call_get (proxy, -1,
      TF_FUTURE_IFACE_CALL_CONTENT_INTERFACE_MEDIA, "CodecOffer",
      got_codec_offer_property, NULL, NULL, G_OBJECT (self));


  return;

 invalid_property:
  tf_call_content_error (self, TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
      "", "Error getting the Content's properties: invalid type");
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
    add_stream (self, g_ptr_array_index (arg_Streams, i));
}


TfCallContent *
tf_call_content_new (TfCallChannel *call_channel,
    const gchar *object_path,
    GError **error)
{
  TfCallContent *self;
  TfFutureCallContent *proxy = tf_future_call_content_new (
      call_channel->proxy, object_path, error);
  GError *myerror = NULL;

  if (!proxy)
    return NULL;

  self = g_object_new (TF_TYPE_CONTENT, NULL);

  self->call_channel = call_channel;
  self->proxy = proxy;

  tf_future_cli_call_content_connect_to_streams_added (
      TF_FUTURE_CALL_CONTENT (proxy), streams_added, NULL, NULL,
      G_OBJECT (self), &myerror);
  if (myerror)
    {
      tf_call_content_error (self, TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error connectiong to StreamAdded signal: %s",
          myerror->message);
      g_object_unref (self);
      g_propagate_error (error, myerror);
      return NULL;
    }

  tf_future_cli_call_content_connect_to_streams_removed (
      TF_FUTURE_CALL_CONTENT (proxy), streams_removed, NULL, NULL,
      G_OBJECT (self), &myerror);
  if (myerror)
    {
      tf_call_content_error (self, TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error connectiong to StreamRemoved signal: %s",
          myerror->message);
      g_object_unref (self);
      g_propagate_error (error, myerror);
      return NULL;
    }

  tp_cli_dbus_properties_call_get_all (proxy, -1, TF_FUTURE_IFACE_CALL_CONTENT,
      got_content_properties, NULL, NULL, G_OBJECT (self));

  return self;
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
try_sending_codecs (TfCallContent *self)
{
  gboolean ready;
  GList *codecs;
  GPtrArray *tpcodecs;

  g_debug ("new local codecs");

  g_object_get (self->fssession, "ready", &ready, NULL);

  if (ready)
    g_object_get (self->fssession, "codecs", &codecs, NULL);
  else
    g_object_get (self->fssession, "codecs-without-config", &codecs, NULL);

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
      self->current_offer = NULL;

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

          tf_call_content_error (content,
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

      try_sending_codecs (content);

      ret = TRUE;
    }

  return ret;
}

static void
tf_call_content_error (TfCallContent *content,
    TfFutureContentRemovalReason reason,
    const gchar *detailed_reason,
    const gchar *message_format, ...)
{
  gchar *message;
  va_list valist;

  va_start (valist, message_format);
  message = g_strdup_vprintf (message_format, valist);
  va_end (valist);

  g_warning (message);
  tf_future_cli_call_content_call_remove (
      content->proxy, -1, reason, detailed_reason, message, NULL, NULL,
      NULL, NULL);

  g_free (message);
}



static FsStream *
tf_call_content_get_existing_fsstream_by_handle (TfCallContent *content,
    guint contact_handle)
{
  guint i;

  for (i = 0; i < content->fsstreams->len; i++)
    {
      struct CallFsStream *cfs = g_ptr_array_index (content->fsstreams, i);

      if (cfs->contact_handle == contact_handle)
        {
          cfs->use_count++;
          return cfs->fsstream;
        }
    }

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

  s = fs_session_new_stream (content->fssession, p, FS_DIRECTION_NONE,
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

  return s;
}

void
_tf_call_content_put_fsstream (TfCallContent *content, FsStream *fsstream)
{
  guint i;

  for (i = 0; i < content->fsstreams->len; i++)
    {
      struct CallFsStream *cfs = g_ptr_array_index (content->fsstreams, i);

      if (cfs->fsstream == fsstream)
        {
          cfs->use_count--;
          if (cfs->use_count <= 0)
            g_ptr_array_remove_index_fast (content->fsstreams, i);
          return;
        }
    }
}
