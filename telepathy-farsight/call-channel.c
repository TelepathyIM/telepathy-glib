/*
 * call-channel.c - Source for TfCallChannel
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
 * SECTION:call-channel

 * @short_description: Handle the Call interface on a Channel
 *
 * This class handles the
 * org.freedesktop.Telepathy.Channel.Interface.Call on a
 * channel using Farsight2.
 */


#include "call-channel.h"

#include <telepathy-glib/util.h>
#include <telepathy-glib/interfaces.h>

#include "extensions/extensions.h"

#include "tf-signals-marshal.h"


struct _TfCallChannel {
  GObject parent;

  TpChannel *channel_proxy;

  GHashTable *contents; /* NULL before getting the first contents */
};

struct _TfCallChannelClass{
  GObjectClass parent_class;
};


G_DEFINE_TYPE (TfCallChannel, tf_call_channel,
    G_TYPE_OBJECT);

enum
{
  SIGNAL_COUNT
};

// static guint signals[SIGNAL_COUNT] = {0};

static void tf_call_channel_dispose (GObject *object);



static void
tf_call_channel_class_init (TfCallChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tf_call_channel_dispose;
}


static void
tf_call_channel_init (TfCallChannel *self)
{
}

static void
tf_call_channel_dispose (GObject *object)
{
  TfCallChannel *self = TF_CALL_CHANNEL (object);

  g_debug (G_STRFUNC);

  if (self->contents)
    g_hash_table_destroy (self->contents);
  self->contents = NULL;

  if (G_OBJECT_CLASS (tf_call_channel_parent_class)->dispose)
    G_OBJECT_CLASS (tf_call_channel_parent_class)->dispose (object);
}

static gboolean
add_content (TfCallChannel *self, const gchar *content_path)
{
  GError *error = NULL;
  // TfCallContent *content = tf_call_content_new (self->channel_proxy,
  //    content_path, &error);

  if (error)
    {
      g_warning ("Error creating the content object: %s", error->message);
      tf_call_channel_error (self);
      return FALSE;
    }

  // g_hash_table_insert (self->contents, g_strdup (content_path), content);

  return TRUE;
}

static void
got_contents (TpProxy *proxy, const GValue *out_value,
    const GError *error, gpointer user_data, GObject *weak_object)
{
  TfCallChannel *self = TF_CALL_CHANNEL (weak_object);
  GPtrArray *contents;
  guint i;

  if (error)
    {
      g_warning ("Error getting the Contents property: %s",
          error->message);
      tf_call_channel_error (self);
      return;
    }

  contents = g_value_get_boxed (out_value);

  self->contents = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, g_object_unref);

  for (i = 0; i < contents->len; i++)
    if (!add_content (self, g_ptr_array_index (contents, i)))
      break;
}

static void
content_added (TpChannel *proxy,
    const gchar *arg_Content,
    guint arg_Content_Type,
    gpointer user_data,
    GObject *weak_object)
{
  TfCallChannel *self = TF_CALL_CHANNEL (weak_object);

  /* Ignore signals before we got the "Contents" property to avoid races that
   * could cause the same content to be added twice
   */

  if (!self->contents)
    return;

  add_content (self, arg_Content);
}

static void
content_removed (TpChannel *proxy,
    const gchar *arg_Content,
    gpointer user_data,
    GObject *weak_object)
{
  TfCallChannel *self = TF_CALL_CHANNEL (weak_object);

  if (!self->contents)
    return;

  g_hash_table_remove (self->contents, arg_Content);
}


static void
got_hardware_streaming (TpProxy *proxy, const GValue *out_value,
    const GError *error, gpointer user_data, GObject *weak_object)
{
  TfCallChannel *self = TF_CALL_CHANNEL (weak_object);
  GError *myerror = NULL;

  if (error)
    {
      g_warning ("Error getting the hardware streaming property: %s",
          error->message);
      tf_call_channel_error (self);
      return;
    }

  if (!g_value_get_boolean (out_value))
    {
      g_warning ("Hardware streaming property is not a boolean");
      tf_call_channel_error (self);
      return;
    }

  tp_cli_dbus_properties_call_get (proxy, -1,
      TF_FUTURE_IFACE_CHANNEL_TYPE_CALL,
      TF_FUTURE_PROP_CHANNEL_TYPE_CALL_CONTENTS,
      got_contents, NULL, NULL, G_OBJECT (self));

  tf_future_cli_channel_type_call_connect_to_content_added (TP_CHANNEL (proxy),
      content_added, NULL, NULL, G_OBJECT (self), &myerror);
  if (myerror)
    {
      g_warning ("Error connectiong to ContentAdded signal: %s",
          myerror->message);
      g_clear_error (&myerror);
      tf_call_channel_error (self);
      return;
    }

  tf_future_cli_channel_type_call_connect_to_content_removed (
      TP_CHANNEL (proxy), content_removed, NULL, NULL, G_OBJECT (self),
      &myerror);
  if (myerror)
    {
      g_warning ("Error connectiong to ContentRemove signal: %s",
          myerror->message);
      g_clear_error (&myerror);
      tf_call_channel_error (self);
      return;
    }

}

TfCallChannel *
tf_call_channel_new (TpChannel *channel)
{
  TfCallChannel *self = g_object_new (
      TF_TYPE_CALL_CHANNEL, NULL);

  self->channel_proxy = channel;

  tp_cli_dbus_properties_call_get (channel, -1,
      TF_FUTURE_IFACE_CHANNEL_TYPE_CALL,
      TF_FUTURE_PROP_CHANNEL_TYPE_CALL_HARDWARE_STREAMING,
      got_hardware_streaming, NULL, NULL, G_OBJECT (self));

  return self;
}

gboolean
tf_call_channel_bus_message (TfCallChannel *channel,
    GstMessage *message)
{
  return FALSE;
}

void
tf_call_channel_error (TfCallChannel *channel)
{
  tf_future_cli_channel_type_call_call_hangup (channel->channel_proxy,
      -1, TFFUTURE_CALL_STATE_CHANGE_REASON_UNKNOWN, "", "",
      NULL, NULL, NULL, NULL);
}
