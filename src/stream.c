/*
 * stream.c - Source for TpStreamEngineStream
 * Copyright (C) 2006 Collabora Ltd.
 * Copyright (C) 2006 Nokia Corporation
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

#include <string.h>

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-interfaces.h>
#include <libtelepathy/tp-props-iface.h>
#include <libtelepathy/tp-props-iface.h>
#include <libtelepathy/tp-media-stream-handler-gen.h>

#include <farsight/farsight-session.h>
#include <farsight/farsight-stream.h>
#include <farsight/farsight-transport.h>

#include <gst/interfaces/xoverlay.h>

#include "common/telepathy-errors.h"

#include "xerrorhandler.h"
#include "tp-stream-engine.h"
#include "types.h"

#ifdef MAEMO_OSSO_SUPPORT
#include "media-engine-gen.h"

#define MEDIA_SERVER_SERVICE_NAME "com.nokia.osso_media_server"
#define MEDIA_SERVER_INTERFACE_NAME "com.nokia.osso_media_server"
#define MEDIA_SERVER_SERVICE_OBJECT "/com/nokia/osso_media_server"
#endif

#include "stream.h"

G_DEFINE_TYPE (TpStreamEngineStream, tp_stream_engine_stream, G_TYPE_OBJECT);

#define DEBUG(stream, format, ...) \
  g_debug ("stream %d (%s) %s: " format, \
    stream->stream_id, \
    STREAM_PRIVATE (stream)->media_type == FARSIGHT_MEDIA_TYPE_AUDIO ? \
      "audio" : "video", \
    G_STRFUNC, \
    ##__VA_ARGS__)

#define STREAM_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TP_STREAM_ENGINE_TYPE_STREAM, \
   TpStreamEngineStreamPrivate))

typedef struct _TpStreamEngineStreamPrivate TpStreamEngineStreamPrivate;

struct _TpStreamEngineStreamPrivate
{
  DBusGProxy *stream_handler_proxy;
  TpPropsIface *conn_props;
  TpConn *connection_proxy;

  guint media_type;
  FarsightStream *fs_stream;
  guint state_changed_handler_id;
  guint bad_window_handler_id;

  gchar *stun_server;
  guint stun_port;

  guint output_volume;
  gboolean output_mute;
  gboolean input_mute;
  guint output_window_id;

  gboolean stream_started;
  gboolean stream_start_scheduled;
  gboolean got_connection_properties;
  gboolean candidate_preparation_required;

#ifdef MAEMO_OSSO_SUPPORT
  gboolean media_engine_disabled;
  DBusGProxy *media_engine_proxy;
#endif
};

enum
{
  CONN_PROP_STUN_SERVER = 0,
  CONN_PROP_STUN_PORT,
  CONN_PROP_STUN_RELAY_SERVER,
  CONN_PROP_STUN_RELAY_UDP_PORT,
  CONN_PROP_STUN_RELAY_TCP_PORT,
  CONN_PROP_STUN_RELAY_SSLTCP_PORT,
  CONN_PROP_STUN_RELAY_USERNAME,
  CONN_PROP_STUN_RELAY_PASSWORD,
};

enum
{
  STREAM_CLOSED,
  STREAM_ERROR,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

static void
add_remote_candidate (DBusGProxy *proxy, gchar *candidate,
                      GPtrArray *transports, gpointer user_data);
static void
remove_remote_candidate (DBusGProxy *proxy, gchar *candidate,
                         gpointer user_data);
static void
set_active_candidate_pair (DBusGProxy *proxy, gchar* native_candidate,
                           gchar* remote_candidate, gpointer user_data);
static void
set_remote_candidate_list (DBusGProxy *proxy, GPtrArray *candidates,
                           gpointer user_data);
static void
set_remote_codecs (DBusGProxy *proxy, GPtrArray *codecs, gpointer user_data);
static void
set_stream_playing (DBusGProxy *proxy, gboolean play, gpointer user_data);

static gboolean
g_object_has_property (GObject *object, const gchar *property)
{
  GObjectClass *klass;

  klass = G_OBJECT_GET_CLASS (object);
  return NULL != g_object_class_find_property (klass, property);
}

static void
tp_stream_engine_stream_dispose (GObject *object)
{
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (object);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);

#ifdef MAEMO_OSSO_SUPPORT
  if (priv->media_engine_proxy)
    {
      g_object_unref (priv->media_engine_proxy);
      priv->media_engine_proxy = NULL;
    }
#endif

  if (priv->stun_server)
    {
      g_free (priv->stun_server);
      priv->stun_server = NULL;
    }

  if (priv->connection_proxy)
    {
      g_object_unref (priv->connection_proxy);
      priv->connection_proxy = NULL;
    }

  if (priv->stream_handler_proxy)
    {
      dbus_g_proxy_disconnect_signal (priv->stream_handler_proxy,
          "AddRemoteCandidate", G_CALLBACK (add_remote_candidate), stream);

      dbus_g_proxy_disconnect_signal (priv->stream_handler_proxy,
          "RemoveRemoteCandidate", G_CALLBACK (remove_remote_candidate),
          stream);

      dbus_g_proxy_disconnect_signal (priv->stream_handler_proxy,
          "SetActiveCandidatePair",
          G_CALLBACK (set_active_candidate_pair), stream);

      dbus_g_proxy_disconnect_signal (priv->stream_handler_proxy,
          "SetRemoteCandidateList",
          G_CALLBACK (set_remote_candidate_list), stream);

      dbus_g_proxy_disconnect_signal (priv->stream_handler_proxy,
          "SetRemoteCodecs", G_CALLBACK (set_remote_codecs), stream);

      dbus_g_proxy_disconnect_signal (priv->stream_handler_proxy,
          "SetStreamPlaying", G_CALLBACK (set_stream_playing), stream);

      g_object_unref (priv->stream_handler_proxy);
      priv->stream_handler_proxy = NULL;
    }

  if (priv->fs_stream)
    {
      g_signal_handler_disconnect (priv->fs_stream,
        priv->state_changed_handler_id);
      g_object_unref (priv->fs_stream);
      priv->fs_stream = NULL;
    }

  if (priv->bad_window_handler_id)
    {
      TpStreamEngineXErrorHandler *handler =
        tp_stream_engine_x_error_handler_get ();

      g_signal_handler_disconnect (handler, priv->bad_window_handler_id);
      priv->bad_window_handler_id = 0;
    }

  if (G_OBJECT_CLASS (tp_stream_engine_stream_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_stream_parent_class)->dispose (object);
}

static void
tp_stream_engine_stream_class_init (TpStreamEngineStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpStreamEngineStreamPrivate));

  object_class->dispose = tp_stream_engine_stream_dispose;

  signals[STREAM_CLOSED] =
    g_signal_new ("stream-closed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[STREAM_ERROR] =
    g_signal_new ("stream-error",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
tp_stream_engine_stream_init (TpStreamEngineStream *self)
{
}

static void
check_start_stream (TpStreamEngineStream *stream)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);

#ifdef MAEMO_OSSO_SUPPORT
  if (!priv->media_engine_disabled)
    return;
#endif

  DEBUG (stream, "stream_start_scheduled = %d; stream_started = %d",
    priv->stream_start_scheduled, priv->stream_started);

  if (priv->stream_start_scheduled && !priv->stream_started)
    {
      if (farsight_stream_get_state (priv->fs_stream) == FARSIGHT_STREAM_STATE_CONNECTED)
        {
          farsight_stream_start (priv->fs_stream);

          if (priv->media_type == FARSIGHT_MEDIA_TYPE_VIDEO)
            {
              TpStreamEngine *engine = tp_stream_engine_get ();
              GstElement *pipeline = tp_stream_engine_get_pipeline (engine);
              gst_element_set_state (pipeline, GST_STATE_PLAYING);
            }

          priv->stream_started = TRUE;
        }
     }
}

/* dummy callback handler for async calling calls with no return values */
static void
dummy_callback (DBusGProxy *proxy, GError *error, gpointer user_data)
{
  if (error)
    g_critical ("%s calling %s", error->message, (char*)user_data);
}

static void
state_changed (FarsightStream *stream,
               FarsightStreamState state,
               FarsightStreamDirection dir,
               gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  switch (state) {
    case FARSIGHT_STREAM_STATE_DISCONNECTED:
          DEBUG (self, "stream %p disconnected", stream);
          break;
    case FARSIGHT_STREAM_STATE_CONNECTING:
          DEBUG (self, "stream %p connecting", stream);
          break;
    case FARSIGHT_STREAM_STATE_CONNECTED:
          DEBUG (self, "stream %p connected", stream);
          /* start the stream if its supposed to be playing already*/
          check_start_stream (self);
          break;
  }

  if (priv->stream_handler_proxy)
    {
      tp_media_stream_handler_stream_state_async (
        priv->stream_handler_proxy, state, dummy_callback,
        "Media.StreamHandler::StreamState");
    }
}

static void
new_native_candidate (FarsightStream *stream,
                      gchar *candidate_id,
                      gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  const GList *fs_candidates, *lp;
  GPtrArray *transports;

  fs_candidates = farsight_stream_get_native_candidate (stream, candidate_id);
  transports = g_ptr_array_new ();

  for (lp = fs_candidates; lp; lp = lp->next)
    {
      FarsightTransportInfo *fs_transport = lp->data;
      GValue transport = { 0, };
      TelepathyMediaStreamProto proto;
      TelepathyMediaStreamTransportType type;

      g_value_init (&transport, TP_TYPE_TRANSPORT_STRUCT);
      g_value_set_static_boxed (&transport,
          dbus_g_type_specialized_construct (TP_TYPE_TRANSPORT_STRUCT));

      switch (fs_transport->proto) {
        case FARSIGHT_NETWORK_PROTOCOL_UDP:
          proto = TP_MEDIA_STREAM_PROTO_UDP;
          break;
        case FARSIGHT_NETWORK_PROTOCOL_TCP:
          proto = TP_MEDIA_STREAM_PROTO_TCP;
          break;
        default:
          g_critical ("%s: FarsightTransportInfo.proto has an invalid value",
              G_STRFUNC);
          return;
      }

      switch (fs_transport->type) {
        case FARSIGHT_CANDIDATE_TYPE_LOCAL:
          type = TP_MEDIA_STREAM_TRANSPORT_TYPE_LOCAL;
          break;
        case FARSIGHT_CANDIDATE_TYPE_DERIVED:
          type = TP_MEDIA_STREAM_TRANSPORT_TYPE_DERIVED;
          break;
        case FARSIGHT_CANDIDATE_TYPE_RELAY:
          type = TP_MEDIA_STREAM_TRANSPORT_TYPE_RELAY;
          break;
        default:
          g_critical ("%s: FarsightTransportInfo.proto has an invalid value",
              G_STRFUNC);
          return;
      }

      DEBUG (self, "fs_transport->ip = '%s'", fs_transport->ip);

      dbus_g_type_struct_set (&transport,
          0, fs_transport->component,
          1, fs_transport->ip,
          2, fs_transport->port,
          3, proto,
          4, fs_transport->proto_subtype,
          5, fs_transport->proto_profile,
          6, (double) fs_transport->preference,
          7, type,
          8, fs_transport->username,
          9, fs_transport->password,
          G_MAXUINT);

      g_ptr_array_add (transports, g_value_get_boxed (&transport));
    }

  tp_media_stream_handler_new_native_candidate_async (
      priv->stream_handler_proxy, candidate_id, transports, dummy_callback,
      "Media.StreamHandler::NativeCandidatesPrepared");
}

/**
 * small helper function to help converting a
 * telepathy dbus candidate to a list of FarsightTransportInfos
 * nothing is copied, so always keep the usage of this within a function
 * if you need to do multiple candidates, call this repeatedly and
 * g_list_join them together.
 * Free the list using free_fs_transports
 */
static GList *
tp_transports_to_fs (gchar* candidate, GPtrArray *transports)
{
  GList *fs_trans_list = NULL;
  GValueArray *transport;
  FarsightTransportInfo *fs_transport;
  int i;

  for (i=0; i< transports->len; i++)
    {
      transport = g_ptr_array_index (transports, i);
      fs_transport = g_new0 (FarsightTransportInfo, 1);

      g_assert(G_VALUE_HOLDS_UINT   (g_value_array_get_nth (transport, 0)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (transport, 1)));
      g_assert(G_VALUE_HOLDS_UINT   (g_value_array_get_nth (transport, 2)));
      g_assert(G_VALUE_HOLDS_UINT   (g_value_array_get_nth (transport, 3)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (transport, 4)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (transport, 5)));
      g_assert(G_VALUE_HOLDS_DOUBLE (g_value_array_get_nth (transport, 6)));
      g_assert(G_VALUE_HOLDS_UINT   (g_value_array_get_nth (transport, 7)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (transport, 8)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (transport, 9)));

      fs_transport->candidate_id = candidate;
      fs_transport->component =
        g_value_get_uint (g_value_array_get_nth (transport, 0));
      fs_transport->ip =
        g_value_get_string (g_value_array_get_nth (transport, 1));
      fs_transport->port =
        (guint16) g_value_get_uint (g_value_array_get_nth (transport, 2));
      fs_transport->proto =
        g_value_get_uint (g_value_array_get_nth (transport, 3));
      fs_transport->proto_subtype =
        g_value_get_string (g_value_array_get_nth (transport, 4));
      fs_transport->proto_profile =
        g_value_get_string (g_value_array_get_nth (transport, 5));
      fs_transport->preference =
        (float) g_value_get_double (g_value_array_get_nth (transport, 6));
      fs_transport->type =
        g_value_get_uint (g_value_array_get_nth (transport, 7));
      fs_transport->username =
        g_value_get_string (g_value_array_get_nth (transport, 8));
      fs_transport->password =
        g_value_get_string (g_value_array_get_nth (transport, 9));

      fs_trans_list = g_list_prepend (fs_trans_list, fs_transport);
    }
  fs_trans_list = g_list_reverse (fs_trans_list);

  return fs_trans_list;
}

static void
free_fs_transports (GList *fs_trans_list)
{
  GList *lp;
  for (lp = g_list_first (fs_trans_list); lp; lp = g_list_next (lp))
    {
      g_free(lp->data);
    }
  g_list_free (fs_trans_list);
}

/**
 * Small helper function to help converting a list of FarsightCodecs
 * to a Telepathy codec list.
 */
static GPtrArray *
fs_codecs_to_tp (const GList *codecs)
{
  GPtrArray *tp_codecs;
  const GList *el;

  tp_codecs = g_ptr_array_new ();

  for (el = codecs; el; el = g_list_next (el))
    {
      FarsightCodec *fsc = el->data;
      GValue codec = { 0, };
      TelepathyMediaStreamType type;
      GHashTable *params;
      GList *cur;

      switch (fsc->media_type) {
        case FARSIGHT_MEDIA_TYPE_AUDIO:
          type = TP_MEDIA_STREAM_TYPE_AUDIO;
          break;
        case FARSIGHT_MEDIA_TYPE_VIDEO:
          type = TP_MEDIA_STREAM_TYPE_VIDEO;
          break;
        default:
          g_critical ("%s: FarsightCodec [%d, %s]'s media_type has an invalid value",
              G_STRFUNC, fsc->id, fsc->encoding_name);
          return NULL;
      }

      /* fill in optional parameters */
      params = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

      for (cur = fsc->optional_params; cur != NULL; cur = cur->next)
        {
          FarsightCodecParameter *param = (FarsightCodecParameter *) cur->data;

          g_hash_table_insert (params, g_strdup (param->name),
                               g_strdup (param->value));
        }

      g_value_init (&codec, TP_TYPE_CODEC_STRUCT);
      g_value_set_static_boxed (&codec,
          dbus_g_type_specialized_construct (TP_TYPE_CODEC_STRUCT));

      dbus_g_type_struct_set (&codec,
          0, fsc->id,
          1, fsc->encoding_name,
          2, type,
          3, fsc->clock_rate,
          4, fsc->channels,
          5, params,
          G_MAXUINT);

      g_hash_table_destroy (params);

      g_debug ("%s: adding codec %s [%d]",
          G_STRFUNC, fsc->encoding_name, fsc->id);

      g_ptr_array_add (tp_codecs, g_value_get_boxed (&codec));
    }

  return tp_codecs;
}

static void
add_remote_candidate (DBusGProxy *proxy, gchar *candidate,
                      GPtrArray *transports, gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  GList *fs_transports;

  fs_transports = tp_transports_to_fs (candidate, transports);

  DEBUG (self, "adding remote candidate %s", candidate);
  farsight_stream_add_remote_candidate (priv->fs_stream, fs_transports);

  free_fs_transports (fs_transports);
}

static void
remove_remote_candidate (DBusGProxy *proxy, gchar *candidate,
                         gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  DEBUG (self, "removing remote candidate %s", candidate);
  farsight_stream_remove_remote_candidate (priv->fs_stream, candidate);
}

static void
set_active_candidate_pair (DBusGProxy *proxy, gchar* native_candidate,
                           gchar* remote_candidate, gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  farsight_stream_set_active_candidate_pair (priv->fs_stream,
                                             native_candidate,
                                             remote_candidate);
}

static void
set_remote_candidate_list (DBusGProxy *proxy, GPtrArray *candidates,
                           gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  GList *fs_transports = NULL;
  GValueArray *candidate = NULL;
  GPtrArray *transports = NULL;
  gchar *candidate_id = NULL;
  int i;

  for (i = 0; i < candidates->len; i++)
    {
      candidate = g_ptr_array_index (candidates, i);
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (candidate,0)));
      g_assert(G_VALUE_TYPE (g_value_array_get_nth (candidate, 1)) ==
                               TP_TYPE_TRANSPORT_LIST);

      /* TODO: mmm, candidate_id should be const in Farsight API */
      candidate_id =
        (gchar*) g_value_get_string (g_value_array_get_nth (candidate, 0));
      transports =
        g_value_get_boxed (g_value_array_get_nth (candidate, 1));

      fs_transports = g_list_concat(fs_transports,
                        tp_transports_to_fs (candidate_id, transports));
    }

  farsight_stream_set_remote_candidate_list (priv->fs_stream, fs_transports);
  free_fs_transports (fs_transports);
}

static void
fill_fs_params (gpointer key, gpointer value, gpointer user_data)
{
  GList **fs_params = (GList **) user_data;
  FarsightCodecParameter *param = g_new0(FarsightCodecParameter,1);
  param->name = key;
  param->value = value;
  *fs_params = g_list_prepend (*fs_params, param);
}

static void
set_remote_codecs (DBusGProxy *proxy, GPtrArray *codecs, gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  GList *fs_codecs =NULL, *lp, *lp2;
  GValueArray *codec;
  GHashTable *params = NULL;
  FarsightCodec *fs_codec;
  GList *fs_params = NULL;
  int i;
  GPtrArray *supp_codecs;

  DEBUG (self, "called");

  for (i = 0; i < codecs->len; i++)
    {
      codec = g_ptr_array_index (codecs, i);
      fs_codec = g_new0(FarsightCodec,1);

      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (codec,0)));
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (codec,1)));
      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (codec,2)));
      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (codec,3)));
      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (codec,4)));
      g_assert(G_VALUE_TYPE (g_value_array_get_nth (codec, 5)) ==
                               DBUS_TYPE_G_STRING_STRING_HASHTABLE);

      fs_codec->id =
        g_value_get_uint (g_value_array_get_nth (codec, 0));
      /* TODO: Farsight API should take const strings */
      fs_codec->encoding_name =
        (gchar*)g_value_get_string (g_value_array_get_nth (codec, 1));
      fs_codec->media_type =
        g_value_get_uint (g_value_array_get_nth (codec, 2));
      fs_codec->clock_rate =
        g_value_get_uint (g_value_array_get_nth (codec, 3));
      fs_codec->channels =
        g_value_get_uint (g_value_array_get_nth (codec, 4));

      params = g_value_get_boxed (g_value_array_get_nth (codec, 5));
      fs_params = NULL;
      g_hash_table_foreach (params, fill_fs_params, &fs_params);

      fs_codec->optional_params = fs_params;

      g_message ("%s: adding remote codec %s [%d]",
          G_STRFUNC, fs_codec->encoding_name, fs_codec->id);

      fs_codecs = g_list_prepend (fs_codecs, fs_codec);
  }
  fs_codecs = g_list_reverse (fs_codecs);

  farsight_stream_set_remote_codecs (priv->fs_stream, fs_codecs);

  supp_codecs = fs_codecs_to_tp (
      farsight_stream_get_codec_intersection (priv->fs_stream));

  tp_media_stream_handler_supported_codecs_async
    (priv->stream_handler_proxy, supp_codecs, dummy_callback,
     "Media.StreamHandler::SupportedCodecs");

  for (lp = g_list_first (fs_codecs); lp; lp = g_list_next (lp))
    {
      /*free the optional parameters lists*/
      fs_codec = (FarsightCodec*) lp->data;
      fs_params = fs_codec->optional_params;
      for (lp2 = g_list_first (fs_params); lp2; lp2 = g_list_next (lp2))
      {
        g_free(lp2->data);
      }
      g_list_free(fs_params);
      g_free(lp->data);
    }
  g_list_free (fs_codecs);

}

static void
stop_stream (TpStreamEngineStream *self)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  if (!priv->fs_stream)
    return;

  DEBUG (self, "calling stop on farsight stream %p", priv->fs_stream);

  farsight_stream_stop (priv->fs_stream);

  priv->stream_started = FALSE;

#ifdef MAEMO_OSSO_SUPPORT
  if (priv->media_engine_disabled && priv->media_engine_proxy)
    {
      GError *error = NULL;

      DEBUG (self, "enabling media server");

      com_nokia_osso_media_server_enable (
          DBUS_G_PROXY (priv->media_engine_proxy), &error);
      if (error)
      {
        g_message ("Unable to enable stream-engine: %s", error->message);
        g_error_free (error);
      }
    }

  priv->media_engine_disabled = FALSE;
#endif

}

static void
set_stream_playing (DBusGProxy *proxy, gboolean play, gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  g_assert (priv->fs_stream != NULL);

  DEBUG (self, "%d", play);

  if (play)
    {
      priv->stream_start_scheduled = TRUE;
      check_start_stream (self);
    }
  else
    {
      stop_stream (self);
    }
}

static void
close (DBusGProxy *proxy, gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);

  stop_stream (self);
  g_signal_emit (self, signals[STREAM_CLOSED], 0);
}

static void
prepare_transports (TpStreamEngineStream *self)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  GPtrArray *codecs;

  if (priv->got_connection_properties && priv->candidate_preparation_required)
    {
      farsight_stream_prepare_transports (priv->fs_stream);

      codecs = fs_codecs_to_tp (
                 farsight_stream_get_local_codecs (priv->fs_stream));

      DEBUG (self, "calling MediaStreamHandler::Ready");
      tp_media_stream_handler_ready_async (
        priv->stream_handler_proxy, codecs, dummy_callback, self);
    }
}

static void
codec_changed (FarsightStream *stream, gint codec_id, gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  if (priv->media_type == FARSIGHT_MEDIA_TYPE_AUDIO)
    {
      tp_stream_engine_stream_mute_output (self, priv->output_mute, NULL);
      tp_stream_engine_stream_mute_input (self, priv->input_mute, NULL);
      tp_stream_engine_stream_set_output_volume (self, priv->output_volume,
        NULL);
    }

  DEBUG (self, "codec_id=%d, stream=%p", codec_id, stream);
  tp_media_stream_handler_codec_choice_async (
    priv->stream_handler_proxy, codec_id, dummy_callback,
    "Media.StreamHandler::CodecChoice");
}

static void
stream_error (
  FarsightStream *stream,
  FarsightStreamError error,
  const gchar *debug,
  gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  g_message ("%s: stream error: stream=%p error=%s", G_STRFUNC, stream, debug);
  /* FIXME: check if error is EOS */
  tp_media_stream_handler_error (priv->stream_handler_proxy, 0, debug, NULL);
  g_signal_emit (self, signals[STREAM_ERROR], 0);
}

static void
new_active_candidate_pair (FarsightStream *stream, const gchar* native_candidate, const gchar *remote_candidate, gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  DEBUG (self, "stream=%p", stream);

  tp_media_stream_handler_new_active_candidate_pair_async
    (priv->stream_handler_proxy, native_candidate, remote_candidate, dummy_callback, "Media.StreamHandler::NewActiveCandidatePair");
}

static void
native_candidates_prepared (FarsightStream *stream, gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  const GList *transport_candidates, *lp;
  FarsightTransportInfo *info;

  DEBUG (self, "stream=%p", stream);

  transport_candidates = farsight_stream_get_native_candidate_list (stream);
  for (lp = transport_candidates; lp; lp = g_list_next (lp))
  {
    info = (FarsightTransportInfo*)lp->data;
    DEBUG (self, "local transport candidate: %s %d %s %s %s:%d, pref %f",
        info->candidate_id, info->component,
        (info->proto == FARSIGHT_NETWORK_PROTOCOL_TCP) ? "TCP" : "UDP",
        info->proto_subtype, info->ip, info->port, (double) info->preference);
  }
  tp_media_stream_handler_native_candidates_prepared_async (
    priv->stream_handler_proxy, dummy_callback,
    "Media.StreamHandler::NativeCandidatesPrepared");
}

static void
set_stun (TpStreamEngineStream *self)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  if (priv->fs_stream)
    {
      if (priv->stun_server && priv->stun_port)
       {
         g_object_set (priv->fs_stream,
                       "stun-ip", priv->stun_server,
                       "stun-port", priv->stun_port,
                       NULL);
       }
    }
}

static void
cb_properties_ready (TpPropsIface *iface, gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  GValue server = {0,}, port = {0,};

  g_value_init (&server, G_TYPE_STRING);
  g_value_init (&port, G_TYPE_UINT);

  priv->got_connection_properties = TRUE;

  if (tp_props_iface_get_value (iface, CONN_PROP_STUN_SERVER, &server))
    {
      if (tp_props_iface_get_value (iface, CONN_PROP_STUN_PORT, &port))
        {
          priv->stun_server = g_value_dup_string (&server);
          priv->stun_port = g_value_get_uint (&port);
          set_stun (self);
        }
    }

  /* this here in case properties_ready_cb gets called after we have
   * recieved all the streams
   */
  prepare_transports (self);
}

#ifdef MAEMO_OSSO_SUPPORT
static void
media_engine_proxy_destroyed (DBusGProxy *proxy, gpointer user_data)
{
  TpStreamEngineStream *self = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);

  if (priv->media_engine_proxy)
    {
      DBusGProxy *proxy = priv->media_engine_proxy;

      DEBUG (self, "MediaEngine proxy destroyed, unreffing it");

      priv->media_engine_proxy = NULL;
      g_object_unref (proxy);
    }
}

static gboolean
media_engine_proxy_init (TpStreamEngineStream *self)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (self);
  GError *me_error;

  DEBUG (self, "initialising media engine proxy");

  priv->media_engine_proxy =
    dbus_g_proxy_new_for_name (tp_get_bus(),
                               MEDIA_SERVER_SERVICE_NAME,
                               MEDIA_SERVER_SERVICE_OBJECT,
                               MEDIA_SERVER_INTERFACE_NAME);

  g_signal_connect (priv->media_engine_proxy, "destroy",
                    G_CALLBACK (media_engine_proxy_destroyed), self);

  g_message ("disabling media engine");

  if (com_nokia_osso_media_server_disable (
        DBUS_G_PROXY (priv->media_engine_proxy),
        &me_error))
    {
      priv->media_engine_disabled = TRUE;
      return TRUE;
    }
  else
    {
      if (me_error)
        g_message ("failed to disable media engine: %s", me_error->message);
      else
        g_message ("failed to disable media engine");

      priv->media_engine_disabled = FALSE;
      return FALSE;
    }
}
#endif

static GstElement *
make_src (TpStreamEngineStream *stream, guint media_type)
{
  const gchar *elem;
  GstElement *src = NULL;

  if (media_type == FARSIGHT_MEDIA_TYPE_AUDIO)
    {
      if ((elem = getenv ("FS_AUDIO_SRC")) || (elem = getenv ("FS_AUDIOSRC")))
        {
          DEBUG (stream, "making audio src with pipeline \"%s\"", elem);
          src = gst_parse_bin_from_description (elem, TRUE, NULL);
          g_assert (src);
        }
      else
        {
          DEBUG (stream, "making audio src with alsasrc element");
          src = gst_element_factory_make ("alsasrc", NULL);

          if (src)
            {
              g_object_set(G_OBJECT(src), "blocksize", 320, NULL);
              g_object_set(G_OBJECT(src), "latency-time",
                G_GINT64_CONSTANT (20000), NULL);
            }
        }

      if (src && g_object_has_property (G_OBJECT (src), "is-live"))
        g_object_set(G_OBJECT(src), "is-live", TRUE, NULL);
    }
  else
    {
      TpStreamEngine *engine = tp_stream_engine_get ();
      GstElement *pipeline = tp_stream_engine_get_pipeline (engine);
      GstElement *tee = gst_bin_get_by_name (GST_BIN (pipeline), "tee");

      src = tee;
      gst_object_unref (tee);
    }

    return src;
}

static GstElement *
make_sink (TpStreamEngineStream *stream, guint media_type)
{
  const gchar *elem;
  GstElement *sink = NULL;

  if (media_type == FARSIGHT_MEDIA_TYPE_AUDIO)
    {
      if ((elem = getenv ("FS_AUDIO_SINK")) || (elem = getenv("FS_AUDIOSINK")))
        {
          DEBUG (stream, "making audio sink with pipeline \"%s\"", elem);
          sink = gst_parse_bin_from_description (elem, TRUE, NULL);
          g_assert (sink);
        }
      else
        {
          DEBUG (stream, "making audio sink with alsasink element");
          sink = gst_element_factory_make ("alsasink", NULL);
        }
    }
  else
    {
      if ((elem = getenv ("FS_VIDEO_SINK")) || (elem = getenv("FS_VIDEOSINK")))
        {
          TpStreamEngine *engine;
          DEBUG (stream, "making video sink with pipeline \"%s\"", elem);
          sink = gst_parse_bin_from_description (elem, TRUE, NULL);
          engine = tp_stream_engine_get ();
          gst_bin_add (GST_BIN (tp_stream_engine_get_pipeline (engine)), sink);
          g_assert (sink);
        }
      else
        {
          /* do nothing: we set a sink when we get a window ID to send video
           * to */

          DEBUG (stream, "not making a video sink");
        }
    }

  if (sink && g_object_has_property (G_OBJECT (sink), "sync"))
    g_object_set (G_OBJECT (sink), "sync", FALSE, NULL);

  return sink;
}

static gboolean
bad_window (TpStreamEngineXErrorHandler *handler, guint window_id,
  gpointer user_data)
{
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (user_data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);

  if (window_id == priv->output_window_id)
    {
      DEBUG (stream, "embedding window %d went away", window_id);
      farsight_stream_set_sink (priv->fs_stream, NULL);

      return TRUE;
    }

  return FALSE;
}

gboolean
tp_stream_engine_stream_go (
  TpStreamEngineStream *stream,
  const gchar *bus_name,
  const gchar *connection_path,
  const gchar *stream_handler_path,
  FarsightSession *fs_session,
  guint id,
  guint media_type,
  guint direction)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);
  TpStreamEngine *engine;
  GstElement *pipeline, *src, *sink;
  gchar *conn_timeout_str;

#ifdef MAEMO_OSSO_SUPPORT
  if (!media_engine_proxy_init (stream))
    return FALSE;
#endif

  stream->stream_id = id;
  priv->media_type = media_type;
  priv->stream_handler_proxy = dbus_g_proxy_new_for_name (
    tp_get_bus(),
    bus_name,
    stream_handler_path,
    TP_IFACE_MEDIA_STREAM_HANDLER);

  if (!priv->stream_handler_proxy)
    {
      g_critical ("couldn't get proxy for stream");
      return FALSE;
    }

  if (media_type == FARSIGHT_MEDIA_TYPE_VIDEO)
    {
      TpStreamEngineXErrorHandler *handler =
        tp_stream_engine_x_error_handler_get ();

      priv->bad_window_handler_id =
        g_signal_connect (handler, "bad-window", (GCallback) bad_window,
          stream);
    }


  priv->fs_stream = farsight_session_create_stream (
    fs_session, media_type, direction);

  if (media_type == FARSIGHT_MEDIA_TYPE_VIDEO)
    {
      /* tell the Farsight stream to use the stream engine pipeline */
      engine = tp_stream_engine_get ();
      pipeline = tp_stream_engine_get_pipeline (engine);
      farsight_stream_set_pipeline (priv->fs_stream, pipeline);
    }

  conn_timeout_str = getenv ("FS_CONN_TIMEOUT");

  if (conn_timeout_str)
    {
      gint conn_timeout = (int) g_ascii_strtod (conn_timeout_str, NULL);
      DEBUG (stream, "setting connection timeout to %d", conn_timeout);
      g_object_set (G_OBJECT(stream), "conn_timeout", conn_timeout, NULL);
    }

  /* TODO Make this smarter, we should only create those sources and sinks if
   * they exist. */
  src = make_src (stream, media_type);
  sink = make_sink (stream, media_type);

  if (src)
    {
      DEBUG (stream, "setting source on Farsight stream");
      farsight_stream_set_source (priv->fs_stream, src);
    }
  else
    {
      DEBUG (stream, "not setting source on Farsight stream");
    }

  if (sink)
    {
      DEBUG (stream, "setting sink on Farsight stream");
      farsight_stream_set_sink (priv->fs_stream, sink);
    }
  else
    {
      DEBUG (stream, "not setting sink on Farsight stream");
    }

  g_signal_connect (G_OBJECT (priv->fs_stream), "error",
                    G_CALLBACK (stream_error), stream);
  g_signal_connect (G_OBJECT (priv->fs_stream), "new-active-candidate-pair",
                    G_CALLBACK (new_active_candidate_pair), stream);
  g_signal_connect (G_OBJECT (priv->fs_stream), "codec-changed",
                    G_CALLBACK (codec_changed), stream);
  g_signal_connect (G_OBJECT (priv->fs_stream), "native-candidates-prepared",
                    G_CALLBACK (native_candidates_prepared), stream);
  priv->state_changed_handler_id =
    g_signal_connect (G_OBJECT (priv->fs_stream), "state-changed",
                      G_CALLBACK (state_changed), stream);
  g_signal_connect (G_OBJECT (priv->fs_stream), "new-native-candidate",
                    G_CALLBACK (new_native_candidate), stream);

  /* OMG, Can we make dbus-binding-tool do this stuff for us?? */

  dbus_g_proxy_add_signal (priv->stream_handler_proxy, "AddRemoteCandidate",
      G_TYPE_STRING, TP_TYPE_TRANSPORT_LIST, G_TYPE_INVALID);
  dbus_g_proxy_add_signal (priv->stream_handler_proxy, "RemoveRemoteCandidate",
      G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_add_signal (priv->stream_handler_proxy, "SetActiveCandidatePair",
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_add_signal (priv->stream_handler_proxy, "SetRemoteCandidateList",
      TP_TYPE_CANDIDATE_LIST, G_TYPE_INVALID);
  dbus_g_proxy_add_signal (priv->stream_handler_proxy, "SetRemoteCodecs",
      TP_TYPE_CODEC_LIST, G_TYPE_INVALID);
  dbus_g_proxy_add_signal (priv->stream_handler_proxy, "SetStreamPlaying",
      G_TYPE_BOOLEAN, G_TYPE_INVALID);
  dbus_g_proxy_add_signal (priv->stream_handler_proxy, "Close",
      G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (priv->stream_handler_proxy, "AddRemoteCandidate",
      G_CALLBACK (add_remote_candidate), stream, NULL);
  dbus_g_proxy_connect_signal (priv->stream_handler_proxy, "RemoveRemoteCandidate",
      G_CALLBACK (remove_remote_candidate), stream, NULL);
  dbus_g_proxy_connect_signal (priv->stream_handler_proxy, "SetActiveCandidatePair",
      G_CALLBACK (set_active_candidate_pair), stream, NULL);
  dbus_g_proxy_connect_signal (priv->stream_handler_proxy, "SetRemoteCandidateList",
      G_CALLBACK (set_remote_candidate_list), stream, NULL);
  dbus_g_proxy_connect_signal (priv->stream_handler_proxy, "SetRemoteCodecs",
      G_CALLBACK (set_remote_codecs), stream, NULL);
  dbus_g_proxy_connect_signal (priv->stream_handler_proxy, "SetStreamPlaying",
      G_CALLBACK (set_stream_playing), stream, NULL);
  dbus_g_proxy_connect_signal (priv->stream_handler_proxy, "Close",
      G_CALLBACK (close), stream, NULL);

  priv->candidate_preparation_required = TRUE;

  set_stun (stream);
  prepare_transports (stream);

  priv->connection_proxy = tp_conn_new (
    tp_get_bus(),
    bus_name,
    connection_path);

  if (!priv->connection_proxy)
    {
      /* FIXME
      *error = g_error_new (TELEPATHY_ERRORS, NotAvailable,
                            "Unable to bind to connection");
      */
      return FALSE;
    }

  /* NB: we should behave nicely if the connection doesnt have properties:
   * sure, it's unlikely, but it's not the end of the world if it doesn't ;)
   */
  priv->conn_props = TELEPATHY_PROPS_IFACE (tp_conn_get_interface (
        priv->connection_proxy, TELEPATHY_PROPS_IFACE_QUARK));
  g_assert (priv->conn_props);

  /* surely we don't need all of these properties */
  tp_props_iface_set_mapping (priv->conn_props,
      "stun-server", CONN_PROP_STUN_SERVER,
      "stun-port", CONN_PROP_STUN_PORT,
      "stun-relay-server", CONN_PROP_STUN_RELAY_SERVER,
      "stun-relay-udp-port", CONN_PROP_STUN_RELAY_UDP_PORT,
      "stun-relay-tcp-port", CONN_PROP_STUN_RELAY_TCP_PORT,
      "stun-relay-ssltcp-port", CONN_PROP_STUN_RELAY_SSLTCP_PORT,
      "stun-relay-username", CONN_PROP_STUN_RELAY_USERNAME,
      "stun-relay-password", CONN_PROP_STUN_RELAY_PASSWORD,
      NULL);

  g_signal_connect (priv->conn_props, "properties-ready",
                    G_CALLBACK (cb_properties_ready), stream);

  return TRUE;
}

TpStreamEngineStream*
tp_stream_engine_stream_new (void)
{
  return g_object_new (TP_STREAM_ENGINE_TYPE_STREAM, NULL);
}

gboolean tp_stream_engine_stream_mute_output (
  TpStreamEngineStream *stream,
  gboolean mute_state,
  GError **error)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);
  GstElement *sink;

  g_return_val_if_fail (priv->fs_stream, FALSE);

  if (priv->media_type != FARSIGHT_MEDIA_TYPE_AUDIO)
    {
      *error = g_error_new (TELEPATHY_ERRORS, InvalidArgument,
        "MuteInput can only be called on audio streams");
      return FALSE;
    }

  priv->output_mute = mute_state;
  sink = farsight_stream_get_sink (priv->fs_stream);

  g_message ("%s: output mute set to %s", G_STRFUNC,
    mute_state ? "on" : "off");

  if (sink && g_object_has_property (G_OBJECT(sink), "mute"))
    g_object_set (G_OBJECT (sink), "mute", mute_state, NULL);

  return TRUE;
}

gboolean tp_stream_engine_stream_set_output_volume (
  TpStreamEngineStream *stream,
  guint volume,
  GError **error)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);
  GstElement *sink;

  g_return_val_if_fail (priv->fs_stream, FALSE);

  if (priv->media_type != FARSIGHT_MEDIA_TYPE_AUDIO)
    {
      *error = g_error_new (TELEPATHY_ERRORS, InvalidArgument,
        "SetOutputVolume can only be called on audio streams");
      return FALSE;
    }

  if (volume > 100)
    volume = 100;

  priv->output_volume = (volume * 65535)/100;
  DEBUG (stream, "setting output volume to %d", priv->output_volume);
  sink = farsight_stream_get_sink (priv->fs_stream);

  if (sink && g_object_has_property (G_OBJECT (sink), "volume"))
    g_object_set (G_OBJECT (sink), "volume", priv->output_volume, NULL);

  return TRUE;
}

gboolean tp_stream_engine_stream_mute_input (
  TpStreamEngineStream *stream,
  gboolean mute_state,
  GError **error)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);
  GstElement *source;

  g_return_val_if_fail (priv->fs_stream, FALSE);

  if (priv->media_type != FARSIGHT_MEDIA_TYPE_AUDIO)
    {
      *error = g_error_new (TELEPATHY_ERRORS, InvalidArgument,
        "MuteInput can only be called on audio streams");
      return FALSE;
    }

  priv->input_mute = mute_state;
  source = farsight_stream_get_source (priv->fs_stream);

  g_message ("%s: input mute set to %s", G_STRFUNC,
    mute_state ? " on" : "off");

  if (source && g_object_has_property (G_OBJECT (source), "mute"))
    g_object_set (G_OBJECT (source), "mute", mute_state, NULL);

  return TRUE;
}

#if 0
static gboolean
bad_window_cb (TpStreamEngineXErrorHandler *handler,
               guint window_id,
               gpointer data)
{
  TpStreamEngineStream *stream = TP_STREAM_ENGINE_STREAM (data);
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);

  if (window_id == priv->output_window_id)
    {
      farsight_stream_set_sink (priv->fs_stream, NULL);
      return TRUE;
    }

  return FALSE;
}
#endif

gboolean
tp_stream_engine_stream_set_output_window (
  TpStreamEngineStream *stream,
  guint window_id,
  GError **error)
{
  TpStreamEngineStreamPrivate *priv = STREAM_PRIVATE (stream);
//  TpStreamEngineXErrorHandler *handler;
  TpStreamEngine *engine;
  GstElement *pipeline, *sink;

  if (priv->media_type != FARSIGHT_MEDIA_TYPE_VIDEO)
    {
      *error = g_error_new (TELEPATHY_ERRORS, InvalidArgument,
        "SetOutputWindow can only be called on video streams");
      return FALSE;
    }

  DEBUG (stream, "putting video output in window %d", window_id);

  priv->output_window_id = window_id;

#if 0
  handler = tp_stream_engine_x_error_handler_get ();
  g_signal_connect (handler, "bad-window", (GCallback) bad_window_cb, stream);
#endif
  sink = gst_element_factory_make ("xvimagesink", NULL);
  g_object_set (sink, "sync", FALSE, NULL);

  engine = tp_stream_engine_get ();
  pipeline = tp_stream_engine_get_pipeline (engine);
  gst_bin_add (GST_BIN (pipeline), sink);
  tp_stream_engine_add_output_window (engine, sink, window_id);

  farsight_stream_set_sink (priv->fs_stream, sink);
  return TRUE;
}

