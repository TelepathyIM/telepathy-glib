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
 * channel using Farstream.
 */

#include "config.h"

#include "call-channel.h"

#include <telepathy-glib/telepathy-glib.h>
#include <farstream/fs-conference.h>

#include "call-content.h"
#include "call-priv.h"


static void call_channel_async_initable_init (GAsyncInitableIface *asynciface);

G_DEFINE_TYPE_WITH_CODE (TfCallChannel, tf_call_channel, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
        call_channel_async_initable_init))

enum
{
  PROP_FS_CONFERENCES = 1
};


enum
{
  SIGNAL_FS_CONFERENCE_ADDED,
  SIGNAL_FS_CONFERENCE_REMOVED,
  SIGNAL_CONTENT_ADDED,
  SIGNAL_CONTENT_REMOVED,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

struct CallConference {
  gint use_count;
  gchar *conference_type;
  FsConference *fsconference;
};

struct CallParticipant {
  gint use_count;
  guint handle;
  FsConference *fsconference;
  FsParticipant *fsparticipant;
};

static void
tf_call_channel_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec);

static void tf_call_channel_dispose (GObject *object);


static void tf_call_channel_init_async (GAsyncInitable *initable,
    int io_priority,
    GCancellable  *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);
static gboolean tf_call_channel_init_finish (GAsyncInitable *initable,
    GAsyncResult *res,
    GError **error);

static void content_added (TpCallChannel *proxy,
    TpCallContent *context_proxy, TfCallChannel *self);
static void content_removed (TpCallChannel *proxy,
    TpCallContent *content_proxy, TpCallStateReason *reason,
    TfCallChannel *self);
static void channel_prepared (GObject *proxy, GAsyncResult *prepare_res,
    gpointer user_data);



static void
tf_call_channel_class_init (TfCallChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tf_call_channel_dispose;
  object_class->get_property = tf_call_channel_get_property;

  g_object_class_install_property (object_class, PROP_FS_CONFERENCES,
      g_param_spec_boxed ("fs-conferences",
          "Farstream FsConference object",
          "GPtrArray of Farstream FsConferences for this channel",
          G_TYPE_PTR_ARRAY,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  signals[SIGNAL_FS_CONFERENCE_ADDED] = g_signal_new ("fs-conference-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL,
      G_TYPE_NONE, 1, FS_TYPE_CONFERENCE);

  signals[SIGNAL_FS_CONFERENCE_REMOVED] = g_signal_new ("fs-conference-removed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL,
      G_TYPE_NONE, 1, FS_TYPE_CONFERENCE);

  signals[SIGNAL_CONTENT_ADDED] = g_signal_new ("content-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL,
      G_TYPE_NONE, 1, TF_TYPE_CALL_CONTENT);

  signals[SIGNAL_CONTENT_REMOVED] = g_signal_new ("content-removed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL,
      G_TYPE_NONE, 1, TF_TYPE_CALL_CONTENT);
}


static void
call_channel_async_initable_init (GAsyncInitableIface *asynciface)
{
  asynciface->init_async = tf_call_channel_init_async;
  asynciface->init_finish = tf_call_channel_init_finish;
}

static void
free_call_conference (gpointer data)
{
  struct CallConference *cc = data;

  gst_object_unref (cc->fsconference);
  g_slice_free (struct CallConference, data);
}

static void
free_participant (gpointer data)
{
  struct CallParticipant *cp = data;

  g_object_unref (cp->fsparticipant);
  gst_object_unref (cp->fsconference);
  g_slice_free (struct CallParticipant, cp);
}

static void
tf_call_channel_init (TfCallChannel *self)
{
  self->fsconferences = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      free_call_conference);

  self->participants = g_ptr_array_new_with_free_func (free_participant);
}


static void
tf_call_channel_init_async (GAsyncInitable *initable,
    int io_priority,
    GCancellable  *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TfCallChannel *self = TF_CALL_CHANNEL (initable);
  GSimpleAsyncResult *res;

  if (cancellable != NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback, user_data,
          G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
          "TfCallChannel initialisation does not support cancellation");
      return;
    }

  res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
      tf_call_channel_init_async);

  tp_g_signal_connect_object (self->proxy, "content-added",
      G_CALLBACK (content_added), self, 0);
  tp_g_signal_connect_object (self->proxy, "content-removed",
      G_CALLBACK (content_removed), self, 0);

  tp_proxy_prepare_async (self->proxy, NULL, channel_prepared, res);
}

static gboolean
tf_call_channel_init_finish (GAsyncInitable *initable,
    GAsyncResult *res,
    GError **error)
{
  GSimpleAsyncResult *simple_res;

  g_return_val_if_fail (g_simple_async_result_is_valid (res,
          G_OBJECT (initable), tf_call_channel_init_async), FALSE);
  simple_res = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (simple_res, error))
    return FALSE;

  return g_simple_async_result_get_op_res_gboolean (simple_res);
}


static void
tf_call_channel_dispose (GObject *object)
{
  TfCallChannel *self = TF_CALL_CHANNEL (object);

  g_debug (G_STRFUNC);

  /* Some of the contents may have more than our ref - if they're in the
     middle of an async op, they're reffed by the async result.
     In this case, unreffing them (implicitely) through destruction of
     the hash table they're in will not dispose them just yet.
     However, they keep an unreffed pointer to the call channel, and will,
     when eventually disposed of, call upon the call channel to put their
     conference back. Since that call channel will then be disposed of,
     I think we can all agree that this is a bit unfortunate.
     So we force dispose the contents as other objects already do, and
     add checks to the content routines to bail out when the object has
     already been disposed of. */
  if (self->contents)
    {
      g_ptr_array_free (self->contents, TRUE);
    }
  self->contents = NULL;

  if (self->participants)
    g_ptr_array_unref (self->participants);
  self->participants = NULL;

  if (self->fsconferences)
      g_hash_table_unref (self->fsconferences);
  self->fsconferences = NULL;

  if (self->proxy)
    g_object_unref (self->proxy);
  self->proxy = NULL;

  if (G_OBJECT_CLASS (tf_call_channel_parent_class)->dispose)
    G_OBJECT_CLASS (tf_call_channel_parent_class)->dispose (object);
}

static void
conf_into_ptr_array (gpointer key, gpointer value, gpointer data)
{
  struct CallConference *cc = value;
  GPtrArray *array = data;

  g_ptr_array_add (array, cc->fsconference);
}

static void
tf_call_channel_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec)
{
  TfCallChannel *self = TF_CALL_CHANNEL (object);

  switch (property_id)
    {
    case PROP_FS_CONFERENCES:
      {
        GPtrArray *array = g_ptr_array_sized_new (
            g_hash_table_size (self->fsconferences));

        g_ptr_array_set_free_func (array, gst_object_unref);
        g_hash_table_foreach (self->fsconferences, conf_into_ptr_array, array);
        g_value_take_boxed (value, array);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
content_ready (GObject *object, GAsyncResult *res, gpointer user_data)
{
  TfCallChannel *self = TF_CALL_CHANNEL (user_data);
  TfCallContent *content = TF_CALL_CONTENT (object);

  if (g_async_initable_init_finish (G_ASYNC_INITABLE (object), res, NULL))
    {
      g_signal_emit (self, signals[SIGNAL_CONTENT_ADDED], 0, content);
    }
  else
    {
      g_ptr_array_remove_fast (self->contents, content);
    }

  g_object_unref (self);
}

static gboolean
add_content (TfCallChannel *self, TpCallContent *content_proxy)
{
  GError *error = NULL;
  TfCallContent *content;
  guint i;

  /* Check if content already added */
  if (!self->contents)
    return FALSE;

  for (i = 0; i < self->contents->len; i++)
    {
      if (tf_call_content_get_proxy (g_ptr_array_index (self->contents, i)) ==
          content_proxy)
        return TRUE;
    }

  content = tf_call_content_new_async (self, content_proxy,
      &error, content_ready, g_object_ref (self));

  if (error)
    {
      /* Error was already transmitted to the CM by TfCallContent */
      g_clear_error (&error);
      g_object_unref (self);
      return FALSE;
    }

  g_ptr_array_add (self->contents, content);

  return TRUE;
}

static void
content_added (TpCallChannel *proxy,
    TpCallContent *content_proxy,
    TfCallChannel *self)
{
  /* Ignore signals before we got the "Contents" property to avoid races that
   * could cause the same content to be added twice
   */

  if (!self->contents)
    return;

  add_content (self, content_proxy);
}

static void
content_removed (TpCallChannel *proxy,
    TpCallContent *content_proxy,
    TpCallStateReason *reason,
    TfCallChannel *self)
{
  guint i;
  if (!self->contents)
    return;

  for (i = 0; i < self->contents->len; i++)
    {

      if (tf_call_content_get_proxy (g_ptr_array_index (self->contents, i)) ==
          content_proxy)
        {
          TfCallContent *content = g_ptr_array_index (self->contents, i);

          g_object_ref (content);
          g_ptr_array_remove_index_fast (self->contents, i);
          g_signal_emit (self, signals[SIGNAL_CONTENT_REMOVED], 0, content);
          g_object_unref (content);
          return;
        }
    }
}

static void
free_content (gpointer data)
{
  TfCallContent *content = data;

  _tf_call_content_destroy (content);
  g_object_unref (content);
}

static void
channel_prepared (GObject *proxy, GAsyncResult *prepare_res, gpointer user_data)
{
  GSimpleAsyncResult *res = user_data;
  TfCallChannel *self =
      TF_CALL_CHANNEL (g_async_result_get_source_object (G_ASYNC_RESULT (res)));
  GError *error = NULL;
  GPtrArray *contents;
  guint i;

  if (!tp_proxy_prepare_finish (proxy, prepare_res, &error))
    {
      g_warning ("Preparing the channel: %s",
          error->message);
      g_simple_async_result_take_error (res, error);
      goto out;
    }

  if (tp_call_channel_has_hardware_streaming (TP_CALL_CHANNEL (proxy)))
    {
      g_warning ("Hardware streaming property is TRUE, ignoring");

      g_simple_async_result_set_error (res, TP_ERROR, TP_ERROR_NOT_CAPABLE,
          "This channel does hardware streaming, not handled here");
      goto out;
    }

  contents = tp_call_channel_get_contents (TP_CALL_CHANNEL (proxy));

  self->contents = g_ptr_array_new_with_free_func (free_content);

  for (i = 0; i < contents->len; i++)
    if (!add_content (self, g_ptr_array_index (contents, i)))
      break;

  g_simple_async_result_set_op_res_gboolean (res, TRUE);

out:
  g_simple_async_result_complete (res);
  g_object_unref (res);
  g_object_unref (self);
}

void
tf_call_channel_new_async (TpChannel *channel,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TfCallChannel *self = g_object_new (TF_TYPE_CALL_CHANNEL, NULL);

  self->proxy = g_object_ref (channel);
  g_async_initable_init_async (G_ASYNC_INITABLE (self), 0, NULL, callback,
      user_data);

  /* Ownership passed to async call */
  g_object_unref (self);
}


static gboolean
find_conf_func (gpointer key, gpointer value, gpointer data)
{
  FsConference *conf = data;
  struct CallConference *cc = value;

  if (cc->fsconference == conf)
    return TRUE;
  else
    return FALSE;
}

static struct CallConference *
find_call_conference_by_conference (TfCallChannel *channel,
    GstObject *conference)
{
  return g_hash_table_find (channel->fsconferences, find_conf_func,
      conference);
}

gboolean
tf_call_channel_bus_message (TfCallChannel *channel,
    GstMessage *message)
{
  GError *error = NULL;
  gchar *debug;
  struct CallConference *cc;
  guint i;

  cc = find_call_conference_by_conference (channel, GST_MESSAGE_SRC (message));
  if (!cc)
    return FALSE;

  switch (GST_MESSAGE_TYPE (message))
    {
    case GST_MESSAGE_WARNING:
      gst_message_parse_warning (message, &error, &debug);

      g_warning ("session: %s (%s)", error->message, debug);

      g_error_free (error);
      g_free (debug);
      return TRUE;
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (message, &error, &debug);

      g_warning ("session ERROR: %s (%s)", error->message, debug);

      tf_call_channel_error (channel);

      g_error_free (error);
      g_free (debug);
      return TRUE;
    default:
      break;
    }

  for (i = 0; i < channel->contents->len; i++)
    if (tf_call_content_bus_message (g_ptr_array_index (channel->contents, i),
            message))
      return TRUE;

  return FALSE;
}

void
tf_call_channel_error (TfCallChannel *channel)
{
  tp_call_channel_hangup_async (TP_CALL_CHANNEL (channel->proxy),
      TP_CALL_STATE_CHANGE_REASON_UNKNOWN, "", "", NULL, NULL);
}


/* This always returns a reference, one should use _put_conference to unref it
 */
FsConference *
_tf_call_channel_get_conference (TfCallChannel *channel,
    const gchar *conference_type)
{
  gchar *tmp;
  struct CallConference *cc;

  cc = g_hash_table_lookup (channel->fsconferences, conference_type);

  if (cc)
    {
      cc->use_count++;
      gst_object_ref (cc->fsconference);
      return cc->fsconference;
    }

  cc = g_slice_new (struct CallConference);
  cc->use_count = 1;
  cc->conference_type = g_strdup (conference_type);

  tmp = g_strdup_printf ("fs%sconference", conference_type);
  cc->fsconference = FS_CONFERENCE (gst_element_factory_make (tmp, NULL));
  g_free (tmp);

  if (cc->fsconference == NULL)
  {
    g_slice_free (struct CallConference, cc);
    return NULL;
  }

  /* Take ownership of the conference */
  gst_object_ref_sink (cc->fsconference);
  g_hash_table_insert (channel->fsconferences, cc->conference_type, cc);

  g_signal_emit (channel, signals[SIGNAL_FS_CONFERENCE_ADDED], 0,
      cc->fsconference);
  g_object_notify (G_OBJECT (channel), "fs-conferences");

  gst_object_ref (cc->fsconference);

  return cc->fsconference;
}

void
_tf_call_channel_put_conference (TfCallChannel *channel,
    FsConference *conference)
{
  struct CallConference *cc;

  cc = find_call_conference_by_conference (channel, GST_OBJECT (conference));
  if (!cc)
    {
      g_warning ("Trying to put conference that does not exist");
      return;
    }

  cc->use_count--;

  if (cc->use_count <= 0)
    {
      g_signal_emit (channel, signals[SIGNAL_FS_CONFERENCE_REMOVED], 0,
          cc->fsconference);
      g_hash_table_remove (channel->fsconferences, cc->conference_type);
      g_object_notify (G_OBJECT (channel), "fs-conferences");
    }

  gst_object_unref (conference);
}


FsParticipant *
_tf_call_channel_get_participant (TfCallChannel *channel,
    FsConference *fsconference,
    guint contact_handle,
    GError **error)
{
  guint i;
  struct CallParticipant *cp;
  FsParticipant *p;

  for (i = 0; i < channel->participants->len; i++)
    {
      cp = g_ptr_array_index (channel->participants, i);

      if (cp->fsconference == fsconference &&
          cp->handle == contact_handle)
        {
          cp->use_count++;
          return g_object_ref (cp->fsparticipant);
        }
    }

  p = fs_conference_new_participant (fsconference, error);
  if (!p)
    return NULL;

  cp = g_slice_new (struct CallParticipant);
  cp->use_count = 1;
  cp->handle = contact_handle;
  cp->fsconference = gst_object_ref (fsconference);
  cp->fsparticipant = p;
  g_ptr_array_add (channel->participants, cp);

  return p;
}


void
_tf_call_channel_put_participant (TfCallChannel *channel,
    FsParticipant *participant)
{
  guint i;

   for (i = 0; i < channel->participants->len; i++)
    {
      struct CallParticipant *cp = g_ptr_array_index (channel->participants, i);

      if (cp->fsparticipant == participant)
        {
          cp->use_count--;
          if (cp->use_count <= 0)
            g_ptr_array_remove_index_fast (channel->participants, i);
          else
            gst_object_unref (cp->fsparticipant);
          return;
        }
    }
}
