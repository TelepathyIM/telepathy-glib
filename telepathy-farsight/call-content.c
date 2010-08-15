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

#include <string.h>

#include <telepathy-glib/proxy-subclass.h>

#include "extensions/extensions.h"

#include "tf-signals-marshal.h"
#include "utils.h"


struct _TfCallContent {
  GObject parent;

  TfCallChannel *call_channel;

  TfFutureCallContent *proxy;

  FsSession *fssession;
  TpMediaStreamType media_type;

  GList *current_codecs;
  TpProxy *current_offer;

  GHashTable *streams; /* NULL before getting the first streams */
};

struct _TfCallContentClass{
  GObjectClass parent_class;
};


G_DEFINE_TYPE (TfCallContent, tf_call_content, G_TYPE_OBJECT);


enum
{
  PROP_FS_SESSION = 1
};

enum
{
  SIGNAL_COUNT
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
      g_param_spec_object ("fs-session",
          "Farsight2 FsSession ",
          "The Farsight2 session for this channel",
          FS_TYPE_SESSION,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

}


static void
tf_call_content_init (TfCallContent *self)
{
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
    case PROP_FS_SESSION:
      if (self->fssession)
        g_value_set_object (value, self->fssession);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static gboolean
add_stream (TfCallContent *self, const gchar *stream_path)
{
  GError *error = NULL;
  // TfCallStream *stream = tf_call_stream_new (self,
  //    stream_path, &error);

  if (error)
    {
      g_warning ("Error creating the stream object: %s", error->message);
      tf_call_channel_error (self->call_channel);
      return FALSE;
    }

  // g_hash_table_insert (self->streams, g_strdup (stream_path), stream);

  return TRUE;
}


static void
got_content_media_properties (TpProxy *proxy, GHashTable *out_Properties,
    const GError *error, gpointer user_data, GObject *weak_object)
{
  // TfCallContent *self = TF_CALL_CONTENT (weak_object);

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
  gboolean got_media_interface = FALSE;;

  if (error)
    {
      g_warning ("Error getting the Content's properties: %s",
          error->message);
      tf_call_channel_error (self->call_channel);
      return;
    }

  if (!out_Properties)
    {
      g_warning ("Error getting the Content's properties: there are none");
      tf_call_channel_error (self->call_channel);
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
      g_warning ("Content does not have the media interface,"
          " but HardwareStreaming was NOT true");
      tf_call_channel_error (self->call_channel);
    }


  self->media_type = tp_asv_get_uint32 (out_Properties, "Type", &valid);
  if (!valid)
    goto invalid_property;

  streams = tp_asv_get_boxed (out_Properties, "Streams",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST);
  if (!streams)
    goto invalid_property;

  g_assert (self->fssession == NULL);

  self->fssession = fs_conference_new_session (self->call_channel->fsconference,
      tp_media_type_to_fs (self->media_type), &myerror);

  if (!self->fssession)
    {
      g_warning ("Could not create FsSession: %s", myerror->message);
      g_clear_error (&myerror);
      tf_call_channel_error (self->call_channel);
      return;
    }

  self->streams = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, g_object_unref);

  for (i = 0; i < streams->len; i++)
    if (!add_stream (self, g_ptr_array_index (streams, i)))
      break;

  tp_proxy_add_interface_by_id (TP_PROXY (self->proxy),
      TF_FUTURE_IFACE_QUARK_CALL_CONTENT_INTERFACE_MEDIA);

  tp_cli_dbus_properties_call_get_all (proxy, -1,
      TF_FUTURE_IFACE_CALL_CONTENT_INTERFACE_MEDIA,
      got_content_media_properties, NULL, NULL, G_OBJECT (self));


  return;

 invalid_property:
  g_warning ("Error getting the Content's properties: invalid type");
  tf_call_channel_error (self->call_channel);
  return;
}

static void
stream_added (TfFutureCallContent *proxy,
    const gchar *arg_Stream,
    gpointer user_data,
    GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);

  /* Ignore signals before we got the "Contents" property to avoid races that
   * could cause the same content to be added twice
   */

  if (!self->streams)
    return;

  add_stream (self, arg_Stream);
}

static void
stream_removed (TfFutureCallContent *proxy,
    const gchar *arg_Stream,
    gpointer user_data,
    GObject *weak_object)
{
  TfCallContent *self = TF_CALL_CONTENT (weak_object);

  if (!self->streams)
    return;

  g_hash_table_remove (self->streams, arg_Stream);
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

  tf_future_cli_call_content_connect_to_stream_added (
      TF_FUTURE_CALL_CONTENT (proxy), stream_added, NULL, NULL,
      G_OBJECT (self), &myerror);
  if (myerror)
    {
      g_warning ("Error connectiong to StreamAdded signal: %s",
          (*error)->message);
      tf_call_channel_error (call_channel);
      g_object_unref (self);
      g_propagate_error (error, myerror);
      return NULL;
    }

  tf_future_cli_call_content_connect_to_stream_removed (
      TF_FUTURE_CALL_CONTENT (proxy), stream_removed, NULL, NULL,
      G_OBJECT (self), &myerror);
  if (myerror)
    {
      g_warning ("Error connectiong to StreamRemoved signal: %s",
          myerror->message);
      tf_call_channel_error (call_channel);
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
    }
  else
    {
      tf_future_cli_call_content_interface_media_call_set_codecs (self->proxy,
          -1, tpcodecs, NULL, NULL, NULL, NULL);
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

          /*
           * We ignore the Unknown Cname error because current signalling
           * does no provide us with a cname
           */
          if (errorno == FS_ERROR_UNKNOWN_CNAME)
            return TRUE;


          enumclass = g_type_class_ref (FS_TYPE_ERROR);
          enumvalue = g_enum_get_value (enumclass, errorno);
          g_warning ("error (%s (%d)): %s : %s",
              enumvalue->value_nick, errorno, msg, debug);
          g_type_class_unref (enumclass);

          /* FIXME: Error propagation not possible */
          g_warning ("ERROR propagation not possible");
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
