/*
 * call-stream.c - Source for TfCallStream
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
 * SECTION:tfcallstream
 */


#include "call-stream.h"

#include <telepathy-glib/util.h>
#include <telepathy-glib/interfaces.h>
#include <gst/farsight/fs-conference-iface.h>

#include <stdarg.h>
#include <string.h>

#include <telepathy-glib/proxy-subclass.h>

#include "extensions/extensions.h"

#include "tf-signals-marshal.h"
#include "utils.h"


G_DEFINE_TYPE (TfCallStream, tf_call_stream, G_TYPE_OBJECT);


static void tf_call_stream_dispose (GObject *object);

static void
tf_call_stream_class_init (TfCallStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tf_call_stream_dispose;
}

static void
tf_call_stream_init (TfCallStream *self)
{
}

static void
tf_call_stream_dispose (GObject *object)
{
  TfCallStream *self = TF_CALL_STREAM (object);

  g_debug (G_STRFUNC);

  if (self->proxy)
    g_object_unref (self->proxy);
  self->proxy = NULL;

  if (G_OBJECT_CLASS (tf_call_stream_parent_class)->dispose)
    G_OBJECT_CLASS (tf_call_stream_parent_class)->dispose (object);
}


static void
local_sending_state_changed (TfFutureCallStream *proxy,
    guint arg_State,
    gpointer user_data, GObject *weak_object)
{
}

static void
remote_members_changed (TfFutureCallStream *proxy,
    GHashTable *arg_Updates,
    const GArray *arg_Removed,
    gpointer user_data, GObject *weak_object)
{
}

static void
server_info_retrieved (TfFutureCallStream *proxy,
    gpointer user_data, GObject *weak_object)
{
}

static void
endpoints_changed (TfFutureCallStream *proxy,
    const GPtrArray *arg_Endpoints_Added,
    const GPtrArray *arg_Endpoints_Removed,
    gpointer user_data, GObject *weak_object)
{
}

static void
got_stream_media_properties (TpProxy *proxy, GHashTable *out_Properties,
    const GError *error, gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);

  if (error)
    {
      tf_call_content_error (self->call_content,
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
          "", "Error getting the Streams's media properties: %s",
          error->message);
      return;
    }


  if (!out_Properties)
    {
      tf_call_content_error (self->call_content,
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
          "", "Error getting the Stream's media properties: there are none");
      return;
    }
}


static void
got_stream_properties (TpProxy *proxy, GHashTable *out_Properties,
    const GError *error, gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);
  gboolean valid;
  GError *myerror = NULL;
  guint i;
  const gchar * const * interfaces;
  gboolean got_media_interface = FALSE;
  gboolean local_sending_state;
  GHashTable *members;

  if (error)
    {
      tf_call_content_error (self->call_content,
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
          "", "Error getting the Streams's properties: %s", error->message);
      return;
    }

  if (!out_Properties)
    {
      tf_call_content_error (self->call_content,
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
          "", "Error getting the Content's properties: there are none");
      return;
    }

  interfaces = tp_asv_get_strv (out_Properties, "Interfaces");

  for (i = 0; interfaces[i]; i++)
    if (!strcmp (interfaces[i], TF_FUTURE_IFACE_CALL_STREAM_INTERFACE_MEDIA))
      {
        got_media_interface = TRUE;
        break;
      }

  if (!got_media_interface)
    {
      tf_call_content_error (self->call_content,
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR,
          "", "Stream does not have the media interface,"
          " but HardwareStreaming was NOT true");
      return;
    }

  members = tp_asv_get_boxed (out_Properties, "RemoteMembers",
      TF_FUTURE_HASH_TYPE_CONTACT_SENDING_STATE_MAP);
  if (!members)
    goto invalid_property;

  local_sending_state = tp_asv_get_boolean (out_Properties, "LocalSendingState",
      &valid);
  if (!valid)
    goto invalid_property;


  tp_proxy_add_interface_by_id (TP_PROXY (self->proxy),
      TF_FUTURE_IFACE_QUARK_CALL_STREAM_INTERFACE_MEDIA);

  tf_future_cli_call_stream_interface_media_connect_to_server_info_retrieved (
      TF_FUTURE_CALL_STREAM (proxy), server_info_retrieved, NULL, NULL,
      G_OBJECT (self), &myerror);
  if (myerror)
    {
      tf_call_content_error (self->call_content,
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error connectiong to ServerInfoRetrived signal: %s",
          myerror->message);
      g_clear_error (&myerror);
      return;
    }


  tf_future_cli_call_stream_interface_media_connect_to_endpoints_changed (
      TF_FUTURE_CALL_STREAM (proxy), endpoints_changed, NULL, NULL,
      G_OBJECT (self), &myerror);
  if (myerror)
    {
      tf_call_content_error (self->call_content,
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error connectiong to EndpointsChanged signal: %s",
          myerror->message);
      g_clear_error (&myerror);
      return;
    }

  tp_cli_dbus_properties_call_get_all (proxy, -1,
      TF_FUTURE_IFACE_CALL_STREAM_INTERFACE_MEDIA,
      got_stream_media_properties, NULL, NULL, G_OBJECT (self));

  return;

 invalid_property:
  tf_call_content_error (self->call_content,
      TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
      "Error getting the Stream's properties: invalid type");
  return;
}

TfCallStream *
tf_call_stream_new (TfCallChannel *call_channel,
    TfCallContent *call_content,
    const gchar *object_path,
    GError **error)
{
  TfCallStream *self;
  TfFutureCallStream *proxy = tf_future_call_stream_new (call_channel->proxy,
      object_path, error);
  GError *myerror = NULL;

  if (!proxy)
    return NULL;

  self = g_object_new (TF_TYPE_STREAM, NULL);

  self->call_content = call_content;
  self->proxy = proxy;

  tf_future_cli_call_stream_connect_to_local_sending_state_changed (
      TF_FUTURE_CALL_STREAM (proxy), local_sending_state_changed, NULL, NULL,
      G_OBJECT (self), &myerror);
  if (myerror)
    {
      tf_call_content_error (self->call_content,
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error connectiong to LocalSendingStateChanged signal: %s",
          myerror->message);
      g_object_unref (self);
      g_propagate_error (error, myerror);
      return NULL;
    }

  tf_future_cli_call_stream_connect_to_remote_members_changed (
      TF_FUTURE_CALL_STREAM (proxy), remote_members_changed, NULL, NULL,
      G_OBJECT (self), &myerror);
  if (myerror)
    {
      tf_call_content_error (self->call_content,
          TF_FUTURE_CONTENT_REMOVAL_REASON_ERROR, "",
          "Error connectiong to RemoteMembersChanged signal: %s",
          myerror->message);
      g_object_unref (self);
      g_propagate_error (error, myerror);
      return NULL;
    }

  tp_cli_dbus_properties_call_get_all (proxy, -1, TF_FUTURE_IFACE_CALL_STREAM,
      got_stream_properties, NULL, NULL, G_OBJECT (self));

  return self;
}


