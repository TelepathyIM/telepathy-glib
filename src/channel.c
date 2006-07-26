
/*
 * channel.c - Source for TpStreamEngineChannel
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

#include <stdlib.h>

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-constants.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-chan-type-streamed-media-gen.h>
#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-interfaces.h>
#include <libtelepathy/tp-props-iface.h>

#include <farsight/farsight-session.h>
#include <farsight/farsight-stream.h>
#include <farsight/farsight-codec.h>
#include <farsight/farsight-transport.h>

#include "common/telepathy-errors.h"

#include "tp-media-session-handler-gen.h"
#include "tp-media-stream-handler-gen.h"

#include "types.h"

#include "channel.h"

G_DEFINE_TYPE (TpStreamEngineChannel, tp_stream_engine_channel, G_TYPE_OBJECT);

#define CHANNEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TP_STREAM_ENGINE_TYPE_CHANNEL, TpStreamEngineChannelPrivate))

typedef struct _TpStreamEngineChannelPrivate TpStreamEngineChannelPrivate;

struct _TpStreamEngineChannelPrivate
{
  TpChan *channel_proxy;
  TpPropsIface *conn_props;
  TpConn *connection_proxy;
  DBusGProxy *streamed_media_proxy;
  DBusGProxy *session_proxy;
  DBusGProxy *stream_proxy;
#ifdef MAEMO_OSSO_SUPPORT
  DBusGProxy *media_engine_proxy;
#endif

  gulong channel_destroy_handler;

  gboolean got_connection_properties;
  gboolean stream_started;
  gboolean stream_start_scheduled;
  gboolean candidate_preparation_required;

  FarsightSession *fs_session;
  FarsightStream *fs_stream;

  gchar *stun_server;
  guint stun_port;

  guint output_volume;
  gboolean output_mute;
  gboolean input_mute;
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
  STREAM_ERROR,
  CLOSED,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

static void
tp_stream_engine_channel_dispose (GObject *object)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (object);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  g_debug (G_STRFUNC);

#ifdef MAEMO_OSSO_SUPPORT
  if (priv->media_engine_proxy)
    {
      g_object_unref (priv->media_engine_proxy);
      priv->media_engine_proxy = NULL;
    }
#endif

  if (priv->connection_proxy)
    {
      g_object_unref (priv->connection_proxy);
      priv->connection_proxy = NULL;
    }

  if (priv->channel_proxy)
    {
      g_object_unref (priv->channel_proxy);
      priv->channel_proxy = NULL;
    }

  if (priv->session_proxy)
    {
      g_object_unref (priv->session_proxy);
      priv->session_proxy = NULL;
    }

  if (priv->stream_proxy)
    {
      g_object_unref (priv->stream_proxy);
      priv->stream_proxy = NULL;
    }

  if (priv->fs_session)
    {
      g_object_unref (priv->fs_session);
      priv->fs_session = NULL;
    }

  if (priv->fs_stream)
    {
      g_object_unref (priv->fs_stream);
      priv->fs_stream = NULL;
    }

  if (G_OBJECT_CLASS (tp_stream_engine_channel_parent_class)->dispose)
    G_OBJECT_CLASS (tp_stream_engine_channel_parent_class)->dispose (object);
}

static void
tp_stream_engine_channel_class_init (TpStreamEngineChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpStreamEngineChannelPrivate));

  object_class->dispose = tp_stream_engine_channel_dispose;

  signals[STREAM_ERROR] =
    g_signal_new ("stream-error",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[CLOSED] =
    g_signal_new ("closed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
tp_stream_engine_channel_init (TpStreamEngineChannel *self)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  /* sensible default */
  priv->output_volume = (65535 * 7) / 10;
}

#ifdef MAEMO_OSSO_SUPPORT
static void
me_proxy_destroyed (DBusGProxy *proxy, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  if (priv->media_engine_proxy)
    {
      DBusGProxy *proxy = priv->media_engine_proxy;

      g_debug ("StreamEngine proxy destroyed, unreffing it");

      priv->media_engine_proxy = NULL;
      g_object_unref (proxy);
    }
}

static gboolean
me_proxy_init (TpStreamEngineChannel *self, GError **error)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  GError *me_error;

  priv->media_engine_proxy =
    dbus_g_proxy_new_for_name (tp_get_bus(),
                               MEDIA_SERVER_SERVICE_NAME,
                               MEDIA_SERVER_SERVICE_OBJECT,
                               MEDIA_SERVER_INTERFACE_NAME);

  g_signal_connect (priv->media_engine_proxy, "destroy",
                    G_CALLBACK (me_proxy_destroyed), obj);

  g_message ("pausing media engine");
  com_nokia_osso_media_server_disable (
      DBUS_G_PROXY (priv->media_engine_proxy),
      &me_error);

  if (!me_error)
    {
      priv->media_engine_disabled = TRUE;
    }
  else
    {
      g_message("Unable to disable media engine: %s", me_error->message);
      priv->media_engine_disabled = FALSE;
      *error = g_error_new (TELEPATHY_ERRORS, NotAvailable,
                            "DSP in use");
      g_error_free (me_error);
    }
}
#endif

static void
set_stun (TpStreamEngineChannel *self)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

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
stream_error (
       FarsightStream *stream,
       FarsightStreamError error,
       const gchar *debug,
       gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  g_message ("%s: stream error: stream=%p error=%s\n", G_STRFUNC, stream, debug);
  g_signal_emit (self, signals[STREAM_ERROR], 0);
}

/* dummy callback handler for async calling calls with no return values */
static void
dummy_callback (DBusGProxy *proxy, GError *error, gpointer user_data)
{
  if (error)
    g_critical ("%s calling %s", error->message, (char*)user_data);
}

static void
new_active_candidate_pair (FarsightStream *stream, const gchar* native_candidate, const gchar *remote_candidate, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  g_debug ("%s: new-active-candidate-pair: stream=%p\n", G_STRFUNC, stream);

  org_freedesktop_Telepathy_Media_StreamHandler_new_active_candidate_pair_async
    (priv->stream_proxy, native_candidate, remote_candidate, dummy_callback,"Media.StreamHandler::NewActiveCandidatePair");
}

static void
check_start_stream (TpStreamEngineChannelPrivate *priv)
{
#ifdef MAEMO_OSSO_SUPPORT
  if (!priv->stream_engine_disabled)
    return;
#endif

  if (priv->stream_start_scheduled && !priv->stream_started)
    {
      if (farsight_stream_get_state (priv->fs_stream) == FARSIGHT_STREAM_STATE_CONNECTED)
        {
          farsight_stream_start (priv->fs_stream);
          priv->stream_started = TRUE;
        }
     }
}

static void
state_changed (FarsightStream *stream,
               FarsightStreamState state,
               FarsightStreamDirection dir,
               gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  switch (state) {
    case FARSIGHT_STREAM_STATE_STOPPED:
          g_message ("%s: %p stopped\n", G_STRFUNC, stream);
          break;
    case FARSIGHT_STREAM_STATE_CONNECTING:
          g_message ("%s: %p connecting\n", G_STRFUNC, stream);
          break;
    case FARSIGHT_STREAM_STATE_CONNECTED:
          g_message ("%s: %p connected\n", G_STRFUNC, stream);
          /* start the stream if its supposed to be playing already*/
          check_start_stream(priv);
          break;
    case FARSIGHT_STREAM_STATE_PLAYING:
          g_message ("%s: %p playing\n", G_STRFUNC, stream);
          break;
  }

  if (priv->stream_proxy)
    {
      org_freedesktop_Telepathy_Media_StreamHandler_stream_state_async
        (priv->stream_proxy, state, dummy_callback,"Media.StreamHandler::StreamState");
    }
}

static void
new_native_candidate (FarsightStream *stream,
                      gchar *candidate_id,
                      gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
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

      g_debug ("%s: fs_transport->ip = '%s'", G_STRFUNC, fs_transport->ip);

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

  org_freedesktop_Telepathy_Media_StreamHandler_new_native_candidate_async (
      priv->stream_proxy, candidate_id, transports, dummy_callback,
      "Media.StreamHandler::NativeCandidatesPrepared");
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

      g_value_init (&codec, TP_TYPE_CODEC_STRUCT);
      g_value_set_static_boxed (&codec,
          dbus_g_type_specialized_construct (TP_TYPE_CODEC_STRUCT));

      dbus_g_type_struct_set (&codec,
          0, fsc->id,
          1, fsc->encoding_name,
          2, type,
          3, fsc->clock_rate,
          4, fsc->channels,
          5, g_hash_table_new (g_str_hash, g_str_equal), /* FIXME: parse fsc->optional_params */
          G_MAXUINT);

      g_debug ("%s: adding codec %s [%d]'",
          G_STRFUNC, fsc->encoding_name, fsc->id);

      g_ptr_array_add (tp_codecs, g_value_get_boxed (&codec));
    }

  return tp_codecs;
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

static void
add_remote_candidate (DBusGProxy *proxy, gchar* candidate,
                      GPtrArray *transports, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  GList *fs_transports;

  fs_transports = tp_transports_to_fs (candidate, transports);

  g_message ("%s:adding remote candidate %s", G_STRFUNC, candidate);
  farsight_stream_add_remote_candidate (priv->fs_stream, fs_transports);

  free_fs_transports (fs_transports);
}

static void
remove_remote_candidate (DBusGProxy *proxy, gchar* candidate, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  g_message ("%s: removing remote candidate %s", G_STRFUNC, candidate);
  farsight_stream_remove_remote_candidate (priv->fs_stream, candidate);
}

static void
set_active_candidate_pair (DBusGProxy *proxy, gchar* native_candidate,
                           gchar* remote_candidate, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  farsight_stream_set_active_candidate_pair (priv->fs_stream,
                                             native_candidate,
                                             remote_candidate);
}

static void
set_remote_candidate_list (DBusGProxy *proxy, GPtrArray *candidates,
                           gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
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

void
set_remote_codecs (DBusGProxy *proxy, GPtrArray *codecs, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  GList *fs_codecs =NULL, *lp, *lp2;
  GValueArray *codec;
  GHashTable *params = NULL;
  FarsightCodec *fs_codec;
  GList *fs_params = NULL;
  int i;
  GPtrArray *supp_codecs;

  g_debug ("%s called", G_STRFUNC);

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

      g_message ("%s: adding remote codec %s [%d]'",
          G_STRFUNC, fs_codec->encoding_name, fs_codec->id);

      fs_codecs = g_list_prepend (fs_codecs, fs_codec);
  }
  fs_codecs = g_list_reverse (fs_codecs);

  farsight_stream_set_remote_codecs (priv->fs_stream, fs_codecs);

  supp_codecs = fs_codecs_to_tp (
      farsight_stream_get_codec_intersection (priv->fs_stream));

  org_freedesktop_Telepathy_Media_StreamHandler_supported_codecs_async
    (priv->stream_proxy, supp_codecs, dummy_callback,
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

void
stop_stream (TpStreamEngineChannel *self)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  if (!priv->fs_stream)
    return;

  if (farsight_stream_get_state (priv->fs_stream) == FARSIGHT_STREAM_STATE_PLAYING)
    {
      g_debug ("%s: calling stop on farsight stream %p\n", G_STRFUNC, priv->fs_stream);
      farsight_stream_stop (priv->fs_stream);
      priv->stream_started = FALSE;
    }
}

void
set_stream_playing (DBusGProxy *proxy, gboolean play, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  g_debug ("%s: %d", G_STRFUNC, play);

  if (play)
    {
      priv->stream_start_scheduled = TRUE;
      check_start_stream (priv);
    }
  else
    {
      stop_stream (self);
    }
}

static void
codec_changed (FarsightStream *stream, gint codec_id, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  GstElement *sink = farsight_stream_get_sink (stream);
  GstElement *source = farsight_stream_get_source (stream);
  if (sink)
    {
      g_object_set (G_OBJECT (sink), "volume", priv->output_volume, NULL);
      g_debug("%s output volume set to %d",G_STRFUNC, priv->output_volume);
      g_object_set (G_OBJECT (sink), "mute", priv->output_mute, NULL);
      g_debug ("%s: output mute set to %s", G_STRFUNC, priv->output_mute?"on":"off");
    }

  if (source)
    {
      g_debug ("%s: input mute set to %s", G_STRFUNC, priv->input_mute?"on":"off");
      g_object_set (G_OBJECT (source), "mute", priv->input_mute, NULL);
    }

  g_debug ("%s: codec-changed: codec_id=%d, stream=%p\n", G_STRFUNC, codec_id, stream);
   org_freedesktop_Telepathy_Media_StreamHandler_codec_choice_async
     (priv->stream_proxy, codec_id, dummy_callback,"Media.StreamHandler::CodecChoice");
}

static void
native_candidates_prepared (FarsightStream *stream, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  const GList *transport_candidates, *lp;
  FarsightTransportInfo *info;

  g_debug ("%s: preparation-complete: stream=%p\n", G_STRFUNC, stream);

  transport_candidates = farsight_stream_get_native_candidate_list (stream);
  for (lp = transport_candidates; lp; lp = g_list_next (lp))
  {
    info = (FarsightTransportInfo*)lp->data;
    g_debug ("Local transport candidate: %s %d %s %s %s:%d, pref %f",
        info->candidate_id, info->component, (info->proto == FARSIGHT_NETWORK_PROTOCOL_TCP)?"TCP":"UDP",
        info->proto_subtype, info->ip, info->port, (double) info->preference);
  }
  org_freedesktop_Telepathy_Media_StreamHandler_native_candidates_prepared_async
     (priv->stream_proxy, dummy_callback,"Media.StreamHandler::NativeCandidatesPrepared");
}

static void
prepare_transports (TpStreamEngineChannel *self)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  GPtrArray *codecs;

  if (priv->got_connection_properties && priv->candidate_preparation_required)
    {
      farsight_stream_prepare_transports (priv->fs_stream);

      codecs = fs_codecs_to_tp (
                 farsight_stream_get_local_codecs (priv->fs_stream));

      g_debug ("Calling MediaStreamHandler::Ready");
      org_freedesktop_Telepathy_Media_StreamHandler_ready_async
        (priv->stream_proxy, codecs, dummy_callback, self);
    }
}

static void
new_media_stream_handler (DBusGProxy *proxy, gchar *stream_handler_path,
                          guint media_type, guint direction, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  FarsightStream *stream;
  gchar *bus_name;
  GstElement *src, *sink;

  g_debug ("Adding stream, media_type=%d, direction=%d",
      media_type, direction);
  if (priv->stream_proxy)
    {
      g_warning("already allocated the one supported stream.");
      return;
    }

  g_object_get (priv->channel_proxy, "name", &bus_name, NULL);

  priv->stream_proxy = dbus_g_proxy_new_for_name (tp_get_bus(),
    bus_name,
    stream_handler_path,
    TP_IFACE_MEDIA_STREAM_HANDLER);

  g_free (bus_name);

  if (!priv->stream_proxy)
    {
      g_critical ("couldn't get proxy for stream");
      return;
    }

  stream = farsight_session_create_stream (priv->fs_session,
                                           media_type, direction);

  gchar *conn_timeout_str = getenv ("FS_CONN_TIMEOUT");
  if (conn_timeout_str)
  {
    gint conn_timeout = (int)g_ascii_strtod (conn_timeout_str, NULL);
    g_debug ("Setting connection timeout at %d", conn_timeout);
    g_object_set (G_OBJECT(stream), "conn_timeout", conn_timeout, NULL);
  }

  /* TODO Make this smarter, i should only create those sources and sinks if
   * they exist */
  if (getenv("FS_FAKESTREAM"))
  {
      src = gst_element_factory_make ("fakesrc", NULL);
      sink = gst_element_factory_make ("fakesink", NULL);
      if (src)
        {
          g_object_set(G_OBJECT(src), "is-live", TRUE, NULL);
          farsight_stream_set_source (stream, src);
        }
      if (sink)
        farsight_stream_set_sink (stream, sink);
  }
  else
  {
      src = gst_element_factory_make ("alsasrc", NULL);
      sink = gst_element_factory_make ("alsasink", NULL);
      if (src)
        {
          g_object_set(G_OBJECT(src), "blocksize", 320, NULL);
          g_object_set(G_OBJECT(src), "latency-time", G_GINT64_CONSTANT (20000), NULL);
          g_object_set(G_OBJECT(src), "is-live", TRUE, NULL);
          farsight_stream_set_source (stream, src);
        }
      if (sink)
        farsight_stream_set_sink (stream, sink);
  }


  priv->fs_stream = stream;
  set_stun (self);

  g_signal_connect (G_OBJECT (stream), "error",
                    G_CALLBACK (stream_error), self);
  g_signal_connect (G_OBJECT (stream), "new-active-candidate-pair",
                    G_CALLBACK (new_active_candidate_pair), self);
  g_signal_connect (G_OBJECT (stream), "codec-changed",
                    G_CALLBACK (codec_changed), self);
  g_signal_connect (G_OBJECT (stream), "native-candidates-prepared",
                    G_CALLBACK (native_candidates_prepared), self);
  g_signal_connect (G_OBJECT (stream), "state-changed",
                    G_CALLBACK (state_changed), self);
  g_signal_connect (G_OBJECT (stream), "new-native-candidate",
                    G_CALLBACK (new_native_candidate), self);


  /*OMG, Can we make dbus-binding-tool do this stuff for us??*/
  /* tell the gproxy about the AddRemoteCandidate signal*/
  dbus_g_proxy_add_signal (priv->stream_proxy, "AddRemoteCandidate",
      G_TYPE_STRING, TP_TYPE_TRANSPORT_LIST, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (priv->stream_proxy, "AddRemoteCandidate",
      G_CALLBACK (add_remote_candidate), self, NULL);

  /* tell the gproxy about the RemoveRemoteCandidate signal*/
  dbus_g_proxy_add_signal (priv->stream_proxy, "RemoveRemoteCandidate",
      G_TYPE_STRING, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (priv->stream_proxy, "RemoveRemoteCandidate",
      G_CALLBACK (remove_remote_candidate), self, NULL);

  /* tell the gproxy about the SetActiveCandidatePair signal*/
  dbus_g_proxy_add_signal (priv->stream_proxy, "SetActiveCandidatePair",
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (priv->stream_proxy, "SetActiveCandidatePair",
      G_CALLBACK (set_active_candidate_pair), self, NULL);

  /* tell the gproxy about the SetRemoteCandidateList signal*/
  dbus_g_proxy_add_signal (priv->stream_proxy, "SetRemoteCandidateList",
      TP_TYPE_CANDIDATE_LIST, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (priv->stream_proxy, "SetRemoteCandidateList",
      G_CALLBACK (set_remote_candidate_list), self, NULL);

  /* tell the gproxy about the SetRemoteCodecs signal*/
  dbus_g_proxy_add_signal (priv->stream_proxy, "SetRemoteCodecs",
      TP_TYPE_CODEC_LIST, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (priv->stream_proxy, "SetRemoteCodecs",
      G_CALLBACK (set_remote_codecs), self, NULL);

  /* tell the gproxy about the SetStreamPlaying signal*/
  dbus_g_proxy_add_signal (priv->stream_proxy, "SetStreamPlaying",
      G_TYPE_BOOLEAN, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (priv->stream_proxy, "SetStreamPlaying",
      G_CALLBACK (set_stream_playing), self, NULL);

  priv->candidate_preparation_required = TRUE;
  prepare_transports (self);
}

static void
session_error (FarsightSession *stream,
       FarsightSessionError error,
       const gchar *debug,
       gpointer user_data)
{
  DBusGProxy *session_proxy = (DBusGProxy *) user_data;

  g_message ("%s: session error: session=%p error=%s\n", G_STRFUNC, stream, debug);
  org_freedesktop_Telepathy_Media_SessionHandler_error_async
    (session_proxy, error, debug, dummy_callback, "Media.SessionHandler::Error");
}

static void
add_session (TpStreamEngineChannel *self, guint member,
                            const char *session_handler_path,
                            const gchar* type)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  gchar *bus_name;

  g_debug("adding session for member %d, %s, %s", member, session_handler_path, type);

  if (priv->session_proxy)
    {
      g_warning("already allocated the one supported session.");
      return;
    }

  g_object_get (priv->channel_proxy, "name", &bus_name, NULL);

  priv->session_proxy = dbus_g_proxy_new_for_name (tp_get_bus(),
    bus_name,
    session_handler_path,
    TP_IFACE_MEDIA_SESSION_HANDLER);

  g_free (bus_name);

  if (!priv->session_proxy)
    {
      g_critical ("couldn't get proxy for session");
      return;
    }

  priv->fs_session = farsight_session_factory_make (type);

  if (!priv->fs_session)
    {
      g_error("RTP plugin not found");
      return;
    }
  g_debug ("protocol details:\n name: %s\n description: %s\n author: %s\n",
           farsight_plugin_get_name (priv->fs_session->plugin),
           farsight_plugin_get_description (priv->fs_session->plugin),
           farsight_plugin_get_author (priv->fs_session->plugin));
  g_signal_connect (G_OBJECT (priv->fs_session), "error",
                    G_CALLBACK (session_error), priv->session_proxy);


   /* tell the gproxy about the NewMediaSessionHandler signal*/
  dbus_g_proxy_add_signal (priv->session_proxy, "NewMediaStreamHandler",
      DBUS_TYPE_G_OBJECT_PATH, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (priv->session_proxy, "NewMediaStreamHandler",
      G_CALLBACK (new_media_stream_handler), self, NULL);

  g_debug ("Calling MediaSessionHandler::Ready -->");
  org_freedesktop_Telepathy_Media_SessionHandler_ready_async
    (priv->session_proxy, dummy_callback, "Media.SessionHandler::Ready");
  g_debug ("<-- Returned from MediaSessionHandler::Ready");
}

static void
new_media_session_handler (DBusGProxy *proxy, guint member, const char *session_handler_path, const gchar* type, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  add_session (self, member, session_handler_path, type);
}

static void channel_closed (DBusGProxy *proxy, gpointer user_data);

static void
shutdown_channel (TpStreamEngineChannel *self, gboolean destroyed)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  g_signal_handler_disconnect (
    priv->channel_proxy, priv->channel_destroy_handler);

  if (!destroyed)
    {
      if (priv->streamed_media_proxy)
        {
          g_debug ("%s: disconnecting signals from streamed_media_proxy",
              G_STRFUNC);

          dbus_g_proxy_disconnect_signal (
              DBUS_G_PROXY (priv->streamed_media_proxy),
              "NewMediaSessionHandler", G_CALLBACK (new_media_session_handler),
              self);
        }

      if (priv->session_proxy)
        {
          g_debug ("%s: disconnecting signals from session_proxy", G_STRFUNC);

          dbus_g_proxy_disconnect_signal (priv->session_proxy,
              "NewMediaStreamHandler", G_CALLBACK (new_media_stream_handler),
              self);
        }

      if (priv->stream_proxy)
        {
          g_debug ("%s: disconnecting signals from stream_proxy", G_STRFUNC);

          dbus_g_proxy_disconnect_signal (priv->stream_proxy,
              "AddRemoteCandidate", G_CALLBACK (add_remote_candidate), self);

          dbus_g_proxy_disconnect_signal (priv->stream_proxy,
              "RemoveRemoteCandidate", G_CALLBACK (remove_remote_candidate),
              self);

          dbus_g_proxy_disconnect_signal (priv->stream_proxy,
              "SetActiveCandidatePair",
              G_CALLBACK (set_active_candidate_pair), self);

          dbus_g_proxy_disconnect_signal (priv->stream_proxy,
              "SetRemoteCandidateList",
              G_CALLBACK (set_remote_candidate_list), self);

          dbus_g_proxy_disconnect_signal (priv->stream_proxy,
              "SetRemoteCodecs", G_CALLBACK (set_remote_codecs), self);

          dbus_g_proxy_disconnect_signal (priv->stream_proxy,
              "SetStreamPlaying", G_CALLBACK (set_stream_playing), self);
        }

      if (priv->channel_proxy)
        {
          g_debug ("%s: disconnecting signals from channel_proxy", G_STRFUNC);

          dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->channel_proxy),
              "Closed", G_CALLBACK (channel_closed), self);
        }
    }

#ifdef MAEMO_OSSO_SUPPORT
  if (priv->stream_engine_disabled && priv->media_engine_proxy)
    {
      GError *error = NULL;

      g_debug ("%s: enabling media server", G_STRFUNC);

      com_nokia_osso_media_server_enable (
          DBUS_G_PROXY (priv->media_engine_proxy), &error);
      if (error)
      {
        g_message ("Unable to enable stream-engine: %s", error->message);
        g_error_free (error);
      }
    }

  priv->stream_engine_disabled = FALSE;
#endif

  priv->stream_started = FALSE;
  priv->stream_start_scheduled = FALSE;

  priv->got_connection_properties = FALSE;
  priv->candidate_preparation_required = FALSE;

  g_signal_emit (self, signals[CLOSED], 0);
}

static void
channel_closed (DBusGProxy *proxy, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);

  g_debug ("Channel closed, shutting it down");

  shutdown_channel (self, FALSE);
}

static void
channel_destroyed (DBusGProxy *proxy, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);

  g_debug ("Channel destroyed, shutting it down");

  shutdown_channel (self, TRUE);
}

static void
properties_ready_cb (TpPropsIface *iface, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
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

void
get_session_handlers_reply (DBusGProxy *proxy, GPtrArray *session_handlers, GError *error, gpointer user_data)
{
  TpStreamEngineChannel *self = TP_STREAM_ENGINE_CHANNEL (user_data);
  GValueArray *session;
  int i;
  if (error)
    g_critical ("Error calling GetSessionHandlers: %s", error->message);

  g_debug ("GetSessionHandlers replied: ");
  for (i = 0; i < session_handlers->len; i++)
    {
      session = g_ptr_array_index (session_handlers, i);
      g_assert(G_VALUE_HOLDS_UINT (g_value_array_get_nth (session, 0)));
      g_assert(G_VALUE_TYPE (g_value_array_get_nth (session, 1)) == DBUS_TYPE_G_OBJECT_PATH);
      g_assert(G_VALUE_HOLDS_STRING (g_value_array_get_nth (session, 2)));

      add_session (self,
          g_value_get_uint (g_value_array_get_nth (session, 0)),
          g_value_get_boxed (g_value_array_get_nth (session, 1)),
          g_value_get_string (g_value_array_get_nth (session, 2)));
    }
}

gboolean
tp_stream_engine_channel_handle_channel (
  TpStreamEngineChannel *self,
  const gchar *bus_name,
  const gchar *connection_path,
  const gchar *channel_path,
  guint handle_type,
  guint handle,
  GError **error)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  g_assert (NULL == priv->channel_proxy);

#ifdef MAEMO_OSSO_SUPPORT
  if (!me_proxy_init (self, error))
    goto ERROR;
#endif

  priv->channel_proxy = tp_chan_new (
    tp_get_bus(),                              /* connection  */
    bus_name,                                  /* bus_name    */
    channel_path,                              /* object_name */
    TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,      /* type        */
    handle_type,                               /* handle_type */
    handle);                                   /* handle      */

  if (!priv->channel_proxy)
    {
      *error = g_error_new (TELEPATHY_ERRORS, NotAvailable,
                            "Unable to bind to channel");
      return FALSE;
    }

  /* TODO: check for group interface
  chan_interfaces = (GSList *) tp_chan_local_get_interface_objs(priv->chan);

  if (chan_interfaces == NULL)
  {
    g_error("Channel does not have interfaces.");
    exit(1);
  }
  */

  priv->channel_destroy_handler = g_signal_connect (
    priv->channel_proxy, "destroy", G_CALLBACK (channel_destroyed), self);

  dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->channel_proxy), "Closed",
                               G_CALLBACK (channel_closed),
                               self, NULL);

  priv->streamed_media_proxy = tp_chan_get_interface (priv->channel_proxy,
      TELEPATHY_CHAN_IFACE_STREAMED_QUARK);

  if (!priv->streamed_media_proxy)
    {
      *error = g_error_new (TELEPATHY_ERRORS, NotAvailable,
                            "Channel doesn't have StreamedMedia interface");
      return FALSE;
    }

  dbus_g_proxy_add_signal (DBUS_G_PROXY (priv->streamed_media_proxy),
      "NewMediaSessionHandler", G_TYPE_UINT, DBUS_TYPE_G_OBJECT_PATH,
      G_TYPE_STRING, G_TYPE_INVALID);

  dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->streamed_media_proxy),
      "NewMediaSessionHandler", G_CALLBACK (new_media_session_handler),
      self, NULL);

  priv->connection_proxy = tp_conn_new (
    tp_get_bus(),
    bus_name,
    connection_path);

  if (!priv->connection_proxy)
    {
      *error = g_error_new (TELEPATHY_ERRORS, NotAvailable,
                            "Unable to bind to connection");
      return FALSE;
    }

  /* NB: we should behave nicely if the connection doesnt have properties:
   * sure, it's unlikely, but it's not the end of the world if it doesn't ;)
   */
  priv->conn_props = TELEPATHY_PROPS_IFACE (tp_conn_get_interface (
        priv->connection_proxy, TELEPATHY_PROPS_IFACE_QUARK));

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
                    G_CALLBACK (properties_ready_cb), self);

  tp_chan_type_streamed_media_get_session_handlers_async (
        DBUS_G_PROXY (priv->streamed_media_proxy),
        get_session_handlers_reply, self);

  return TRUE;
}

TpStreamEngineChannel*
tp_stream_engine_channel_new (void)
{
  return g_object_new (TP_STREAM_ENGINE_TYPE_CHANNEL, NULL);
}

void tp_stream_engine_channel_mute_input (
  TpStreamEngineChannel *self,
  gboolean mute_state)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  GstElement *source;
  priv->input_mute = mute_state;

  g_message ("%s: input mute set to %s", G_STRFUNC, mute_state?"on":"off");

  if (priv->fs_stream &&
      farsight_stream_get_state (priv->fs_stream) ==
        FARSIGHT_STREAM_STATE_PLAYING)
    {
      source = farsight_stream_get_source (priv->fs_stream);
      if (source)
        g_object_set (G_OBJECT (source), "mute", mute_state, NULL);
    }
}

void tp_stream_engine_channel_mute_output (
  TpStreamEngineChannel *self,
  gboolean mute_state)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  GstElement *sink;
  priv->output_mute = mute_state;

  g_message ("%s: output mute set to %s", G_STRFUNC, mute_state?"on":"off");

  if (priv->fs_stream &&
      farsight_stream_get_state (priv->fs_stream) ==
        FARSIGHT_STREAM_STATE_PLAYING)
    {
      sink = farsight_stream_get_sink (priv->fs_stream);
      if (sink)
        g_object_set (G_OBJECT (sink), "mute", mute_state, NULL);
    }
}

void tp_stream_engine_channel_set_output_volume (
  TpStreamEngineChannel *self,
  guint volume)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);
  GstElement *sink;

  if (volume > 100) volume=100;

  priv->output_volume = (volume * 65535)/100;
  g_debug ("%s: Setting output volume to %d", G_STRFUNC, priv->output_volume);
  if (priv->fs_stream &&
      farsight_stream_get_state (priv->fs_stream) ==
        FARSIGHT_STREAM_STATE_PLAYING)
    {
      sink = farsight_stream_get_sink (priv->fs_stream);
      if (sink)
        {
          g_debug ("Setting volume to %d", priv->output_volume);
          g_object_set (G_OBJECT (sink), "volume", priv->output_volume, NULL);
          g_message ("Finished setting volume to %d", priv->output_volume);
        }
    }
}

void tp_stream_engine_channel_error (
  TpStreamEngineChannel *self,
  guint error,
  const gchar *message)
{
  TpStreamEngineChannelPrivate *priv = CHANNEL_PRIVATE (self);

  org_freedesktop_Telepathy_Media_StreamHandler_error_async (
    priv->stream_proxy, error, message, dummy_callback,
    "Media.StreamHandler::Error");
  shutdown_channel (self, FALSE);
}

