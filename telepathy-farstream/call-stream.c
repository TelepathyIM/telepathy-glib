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
 *
 * @short_description: Handle the Stream objects for a Call1 channel
 *
 * This class handles the org.freedesktop.Telepathy.Call1.Stream,
 * org.freedesktop.Telepathy.Call1.Stream.Interface.Media and
 * org.freedesktop.Telepathy.Call1.Stream.Endpoint interfaces.
 */

/*
 * TODO:
 * - Support multiple handles
 * - Allow app to fail sending or receiving during call
 *
 * Endpoints:
 * - Support multiple Endpoints (ie SIP forking with ICE)
 * - Call SetControlling
 * - Listen to CandidatePairSelected and call AcceptSelectedCandidatePair/RejectSelectedCandidatePair
 * - Support IsICELite
 */

#include "config.h"

#include "call-stream.h"

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/proxy-subclass.h>
#include <farstream/fs-conference.h>

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>


#include "tf-signals-marshal.h"
#include "utils.h"


G_DEFINE_TYPE (TfCallStream, tf_call_stream, G_TYPE_OBJECT);

static void tf_call_stream_dispose (GObject *object);

static void tf_call_stream_fail_literal (TfCallStream *self,
    TpCallStateChangeReason reason,
    const gchar *detailed_reason,
    const gchar *message);

static void tf_call_stream_fail (TfCallStream *self,
    TpCallStateChangeReason reason,
    const gchar *detailed_reason,
    const gchar *message_format,
    ...);

static void _tf_call_stream_remove_endpoint (TfCallStream *self);


static void
tf_call_stream_class_init (TfCallStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tf_call_stream_dispose;
}

static void
tf_call_stream_init (TfCallStream *self)
{
  self->sending_state = TP_STREAM_FLOW_STATE_STOPPED;
  self->receiving_state = TP_STREAM_FLOW_STATE_STOPPED;
}

static void
tf_call_stream_dispose (GObject *object)
{
  TfCallStream *self = TF_CALL_STREAM (object);

  g_debug (G_STRFUNC);

  if (self->proxy)
    g_object_unref (self->proxy);
  self->proxy = NULL;

  if (self->stun_servers)
    g_boxed_free (TP_ARRAY_TYPE_SOCKET_ADDRESS_IP_LIST, self->stun_servers);
  self->stun_servers = NULL;

  if (self->relay_info)
    g_boxed_free (TP_ARRAY_TYPE_STRING_VARIANT_MAP_LIST, self->relay_info);
  self->relay_info = NULL;

  if (self->fsstream)
    _tf_call_content_put_fsstream (self->call_content, self->fsstream);
  self->fsstream = NULL;

  if (self->endpoint)
    _tf_call_stream_remove_endpoint (self);

  if (G_OBJECT_CLASS (tf_call_stream_parent_class)->dispose)
    G_OBJECT_CLASS (tf_call_stream_parent_class)->dispose (object);
}


static void
tf_call_stream_update_sending_state (TfCallStream *self)
{
  gboolean sending = FALSE;
  FsStreamDirection dir;

  if (self->fsstream == NULL)
    goto done;

  if (self->endpoint == NULL)
    goto done;

  switch (self->sending_state)
    {
    case TP_STREAM_FLOW_STATE_PENDING_START:
      if (self->has_send_resource)
        sending = TRUE;
      break;
    case TP_STREAM_FLOW_STATE_STARTED:
      sending = TRUE;
      break;
    default:
      break;
    }

 done:
  g_object_get (self->fsstream, "direction", &dir, NULL);
  if (sending)
    g_object_set (self->fsstream, "direction", dir | FS_DIRECTION_SEND, NULL);
  else
    g_object_set (self->fsstream, "direction", dir & ~FS_DIRECTION_SEND, NULL);
}

static void
sending_state_changed (TpCallStream *proxy,
    guint arg_State,
    gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);

  self->sending_state = arg_State;

  if (!self->fsstream)
    return;

  switch (arg_State)
    {
    case TP_STREAM_FLOW_STATE_PENDING_START:
      if (self->has_send_resource ||
          _tf_content_start_sending (TF_CONTENT (self->call_content)))
        {
          self->has_send_resource = TRUE;

          tp_cli_call_stream_interface_media_call_complete_sending_state_change (
              proxy, -1, TP_STREAM_FLOW_STATE_STARTED,
              NULL, NULL, NULL, NULL);
          tf_call_stream_update_sending_state (self);
        }
      else
        {
          tp_cli_call_stream_interface_media_call_report_sending_failure (
              proxy, -1, TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
              TP_ERROR_STR_MEDIA_STREAMING_ERROR,
              "Could not start sending", NULL, NULL, NULL, NULL);
          return;
        }
      break;
    case TP_STREAM_FLOW_STATE_PENDING_STOP:
      tf_call_stream_update_sending_state (self);
      if (self->has_send_resource)
        {
          _tf_content_stop_sending (TF_CONTENT (self->call_content));

          self->has_send_resource = FALSE;
        }
      tp_cli_call_stream_interface_media_call_complete_sending_state_change (
          proxy, -1, TP_STREAM_FLOW_STATE_STOPPED, NULL, NULL, NULL, NULL);
      break;
    default:
      break;
    }
}

static void
tf_call_stream_start_receiving (TfCallStream *self, FsStreamDirection dir)
{
  if (self->has_receive_resource ||
      _tf_content_start_receiving (TF_CONTENT (self->call_content),
          &self->contact_handle, 1))
    {
      self->has_receive_resource = TRUE;
      if (self->fsstream)
        g_object_set (self->fsstream,
            "direction", dir | FS_DIRECTION_RECV, NULL);
      tp_cli_call_stream_interface_media_call_complete_receiving_state_change (
          self->proxy, -1, TP_STREAM_FLOW_STATE_STARTED,
          NULL, NULL, NULL, NULL);
    }
  else
    {
      tp_cli_call_stream_interface_media_call_report_receiving_failure (
          self->proxy, -1, TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_MEDIA_STREAMING_ERROR,
          "Could not start receiving", NULL, NULL, NULL, NULL);
    }
}

static void
receiving_state_changed (TpCallStream *proxy,
    guint arg_State,
    gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);
  FsStreamDirection dir;

  self->receiving_state = arg_State;

  if (!self->fsstream)
    return;

  g_object_get (self->fsstream, "direction", &dir, NULL);

  switch (arg_State)
    {
    case TP_STREAM_FLOW_STATE_PENDING_START:
      tf_call_stream_start_receiving (self, dir);
      break;
    case TP_STREAM_FLOW_STATE_PENDING_STOP:
      g_object_set (self->fsstream,
          "direction", dir & ~FS_DIRECTION_RECV, NULL);
      if (self->has_receive_resource)
        {
          _tf_content_stop_receiving (TF_CONTENT (self->call_content),
              &self->contact_handle, 1);

          self->has_receive_resource = FALSE;
        }
      tp_cli_call_stream_interface_media_call_complete_receiving_state_change (
          proxy, -1, TP_STREAM_FLOW_STATE_STOPPED, NULL, NULL, NULL, NULL);
      break;
    default:
      break;
    }
}

static void
tf_call_stream_try_adding_fsstream (TfCallStream *self)
{
  gchar *transmitter;
  GError *error = NULL;
  guint n_params = 0;
  GParameter params[6] = { {NULL,} };
  GList *preferred_local_candidates = NULL;
  guint i;
  FsStreamDirection dir = FS_DIRECTION_NONE;

  memset (params, 0, sizeof(params));

  if (!self->server_info_retrieved ||
      !self->has_contact ||
      !self->has_media_properties)
    return;

  switch (self->transport_type)
    {
    case TP_STREAM_TRANSPORT_TYPE_RAW_UDP:
      transmitter = "rawudp";

      g_debug ("Transmitter: rawudp");

      switch (tf_call_content_get_fs_media_type (self->call_content))
        {
        case TP_MEDIA_STREAM_TYPE_VIDEO:
          preferred_local_candidates = g_list_prepend (NULL,
              fs_candidate_new (NULL, FS_COMPONENT_RTP, FS_CANDIDATE_TYPE_HOST,
                  FS_NETWORK_PROTOCOL_UDP, NULL, 9078));
          break;
        case TP_MEDIA_STREAM_TYPE_AUDIO:
          preferred_local_candidates = g_list_prepend (NULL,
              fs_candidate_new (NULL, FS_COMPONENT_RTP, FS_CANDIDATE_TYPE_HOST,
                  FS_NETWORK_PROTOCOL_UDP, NULL, 7078));
        default:
          break;
        }

      if (preferred_local_candidates)
        {
          params[n_params].name = "preferred-local-candidates";
          g_value_init (&params[n_params].value, FS_TYPE_CANDIDATE_LIST);
          g_value_take_boxed (&params[n_params].value,
              preferred_local_candidates);
          n_params++;
        }
      break;
    case TP_STREAM_TRANSPORT_TYPE_ICE:
    case TP_STREAM_TRANSPORT_TYPE_GTALK_P2P:
    case TP_STREAM_TRANSPORT_TYPE_WLM_2009:
      transmitter = "nice";

      params[n_params].name = "controlling-mode";
      g_value_init (&params[n_params].value, G_TYPE_BOOLEAN);
      g_value_set_boolean (&params[n_params].value, self->controlling);
      n_params++;

      params[n_params].name = "compatibility-mode";
      g_value_init (&params[n_params].value, G_TYPE_UINT);
      switch (self->transport_type)
        {
        case TP_STREAM_TRANSPORT_TYPE_ICE:
          g_value_set_uint (&params[n_params].value, 0);
          break;
        case TP_STREAM_TRANSPORT_TYPE_GTALK_P2P:
          g_value_set_uint (&params[n_params].value, 1);
          self->multiple_usernames = TRUE;
          break;
        case TP_STREAM_TRANSPORT_TYPE_WLM_2009:
          g_value_set_uint (&params[n_params].value, 3);
          break;
        default:
          break;
        }

      g_debug ("Transmitter: nice: TpTransportType:%d controlling:%d",
          self->transport_type, self->controlling);

      n_params++;
      break;
    case TP_STREAM_TRANSPORT_TYPE_SHM:
      transmitter = "shm";
      params[n_params].name = "create-local-candidates";
      g_value_init (&params[n_params].value, G_TYPE_BOOLEAN);
      g_value_set_boolean (&params[n_params].value, TRUE);
      n_params++;
      g_debug ("Transmitter: shm");
      break;
    default:
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR, TP_ERROR_STR_CONFUSED,
          "Unknown transport type %d", self->transport_type);
      return;
    }

  if (self->stun_servers->len)
    {
      GValueArray *gva = g_ptr_array_index (self->stun_servers, 0);
      gchar *ip;
      guint port;
      gchar *conn_timeout_str;

      /* We only use the first STUN server if there are many */

      tp_value_array_unpack (gva, 2, &ip, &port);

      params[n_params].name = "stun-ip";
      g_value_init (&params[n_params].value, G_TYPE_STRING);
      g_value_set_string (&params[n_params].value, ip);
      n_params++;

      params[n_params].name = "stun-port";
      g_value_init (&params[n_params].value, G_TYPE_UINT);
      g_value_set_uint (&params[n_params].value, port);
      n_params++;

      conn_timeout_str = getenv ("FS_CONN_TIMEOUT");
      if (conn_timeout_str)
        {
          gint conn_timeout = strtol (conn_timeout_str, NULL, 10);

          params[n_params].name = "stun-timeout";
          g_value_init (&params[n_params].value, G_TYPE_UINT);
          g_value_set_uint (&params[n_params].value, conn_timeout);
          n_params++;
        }
    }

  if (self->relay_info->len)
    {
      GValueArray *fs_relay_info = g_value_array_new (0);
      GValue val = {0};
      g_value_init (&val, GST_TYPE_STRUCTURE);

      for (i = 0; i < self->relay_info->len; i++)
        {
          GHashTable *one_relay = g_ptr_array_index(self->relay_info, i);
          const gchar *type = NULL;
          const gchar *ip;
          guint32 port;
          const gchar *username;
          const gchar *password;
          guint component;
          GstStructure *s;

          ip = tp_asv_get_string (one_relay, "ip");
          port = tp_asv_get_uint32 (one_relay, "port", NULL);
          type = tp_asv_get_string (one_relay, "type");
          username = tp_asv_get_string (one_relay, "username");
          password = tp_asv_get_string (one_relay, "password");
          component = tp_asv_get_uint32 (one_relay, "component", NULL);

          if (!ip || !port || !username || !password)
              continue;

          if (!type)
            type = "udp";

          s = gst_structure_new ("relay-info",
              "ip", G_TYPE_STRING, ip,
              "port", G_TYPE_UINT, port,
              "username", G_TYPE_STRING, username,
              "password", G_TYPE_STRING, password,
              "type", G_TYPE_STRING, type,
              NULL);

          if (component)
            gst_structure_set (s, "component", G_TYPE_UINT, component, NULL);


          g_value_take_boxed (&val, s);

          g_value_array_append (fs_relay_info, &val);
          g_value_reset (&val);
        }

      if (fs_relay_info->n_values)
        {
          params[n_params].name = "relay-info";
          g_value_init (&params[n_params].value, G_TYPE_VALUE_ARRAY);
          g_value_set_boxed (&params[n_params].value, fs_relay_info);
          n_params++;
        }

      g_value_array_free (fs_relay_info);
    }

  if (self->receiving_state == TP_STREAM_FLOW_STATE_PENDING_START)
    {
      tf_call_stream_start_receiving (self, FS_DIRECTION_NONE);
      dir = FS_DIRECTION_RECV;
    }

  self->fsstream = _tf_call_content_get_fsstream_by_handle (self->call_content,
      self->contact_handle,
      dir,
      transmitter,
      n_params,
      params,
      &error);

  for (i = 0; i < n_params; i++)
    g_value_unset (&params[i].value);

  if (!self->fsstream)
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_MEDIA_STREAMING_ERROR,
          "Could not create FsStream: %s", error->message);
      g_clear_error (&error);
      return;
    }

  if (self->sending_state == TP_STREAM_FLOW_STATE_PENDING_START)
    sending_state_changed (self->proxy,
        self->sending_state, NULL, (GObject *) self);
}

static void
server_info_retrieved (TpCallStream *proxy,
    gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);

  self->server_info_retrieved = TRUE;

  tf_call_stream_try_adding_fsstream (self);
}

static void
relay_info_changed (TpCallStream *proxy,
    const GPtrArray *arg_Relay_Info,
    gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);

  if (self->server_info_retrieved)
    {
      tf_call_stream_fail_literal (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_NOT_IMPLEMENTED,
          "Changing relay servers after ServerInfoRetrived is not implemented");
      return;
    }

  /* Ignore signals that come before the basic info has been retrived */
  if (!self->relay_info)
    return;

  g_boxed_free (TP_ARRAY_TYPE_STRING_VARIANT_MAP_LIST, self->relay_info);
  self->relay_info = g_boxed_copy (TP_ARRAY_TYPE_STRING_VARIANT_MAP_LIST,
      arg_Relay_Info);
}

static void
stun_servers_changed (TpCallStream *proxy,
    const GPtrArray *arg_Servers,
    gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);

  if (self->server_info_retrieved)
    {
      tf_call_stream_fail_literal (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_NOT_IMPLEMENTED,
          "Changing STUN servers after ServerInfoRetrived is not implemented");
      return;
    }

  /* Ignore signals that come before the basic info has been retrived */
  if (!self->stun_servers)
    return;

  g_boxed_free (TP_ARRAY_TYPE_SOCKET_ADDRESS_IP_LIST, self->stun_servers);
  self->stun_servers = g_boxed_copy (TP_ARRAY_TYPE_SOCKET_ADDRESS_IP_LIST,
      arg_Servers);
}

static FsCandidateType
tpcandidate_type_to_fs (TpCallStreamCandidateType type)
{
  switch(type)
    {
    case TP_CALL_STREAM_CANDIDATE_TYPE_NONE:
      g_warning ("Candidate type NONE, assigning to HOST");
      /* fallthrough */
    case TP_CALL_STREAM_CANDIDATE_TYPE_HOST:
      return FS_CANDIDATE_TYPE_HOST;
    case TP_CALL_STREAM_CANDIDATE_TYPE_SERVER_REFLEXIVE:
      return FS_CANDIDATE_TYPE_SRFLX;
    case TP_CALL_STREAM_CANDIDATE_TYPE_PEER_REFLEXIVE:
      return FS_CANDIDATE_TYPE_PRFLX;
    case TP_CALL_STREAM_CANDIDATE_TYPE_RELAY:
      return FS_CANDIDATE_TYPE_RELAY;
    case TP_CALL_STREAM_CANDIDATE_TYPE_MULTICAST:
      return FS_CANDIDATE_TYPE_MULTICAST;
    default:
      g_warning ("Candidate type %d unknown, assigning to HOST", type);
      return FS_CANDIDATE_TYPE_HOST;
    }
}

static FsNetworkProtocol
tpnetworkproto_to_fs (TpMediaStreamBaseProto proto)
{
  switch(proto)
    {
    case TP_MEDIA_STREAM_BASE_PROTO_UDP:
      return FS_NETWORK_PROTOCOL_UDP;
    case TP_MEDIA_STREAM_BASE_PROTO_TCP:
      return FS_NETWORK_PROTOCOL_TCP;
    default:
      g_debug ("Network protocol %d unknown, assigning to UDP", proto);
      return FS_NETWORK_PROTOCOL_UDP;
    }
}


static void
tf_call_stream_add_remote_candidates (TfCallStream *self,
    const GPtrArray *candidates)
{
  GList *fscandidates = NULL;
  guint i;

  /* No candidates to add, ignore. This could either be caused by the CM
   * accidentally emitting an empty RemoteCandidatesAdded or when there are no
   * remote candidates on the endpoint yet when we query it */
  if (candidates->len == 0)
    return;

  for (i = 0; i < candidates->len; i++)
    {
      GValueArray *tpcandidate = g_ptr_array_index (candidates, i);
      guint component;
      gchar *ip;
      guint port;
      GHashTable *extra_info;
      const gchar *foundation;
      guint priority;
      const gchar *username;
      const gchar *password;
      gboolean valid;
      FsCandidate *cand;
      guint type;
      guint protocol;
      guint ttl;
      const gchar *base_ip;
      guint base_port;

      tp_value_array_unpack (tpcandidate, 4, &component, &ip, &port,
          &extra_info);

      foundation = tp_asv_get_string (extra_info, "foundation");
      if (!foundation)
        foundation = "";
      priority = tp_asv_get_uint32 (extra_info, "priority", &valid);
      if (!valid)
        priority = 0;

      username = tp_asv_get_string (extra_info, "username");
      if (!username)
        username = self->creds_username;

      password = tp_asv_get_string (extra_info, "password");
      if (!password)
        password = self->creds_password;

      type = tp_asv_get_uint32 (extra_info, "type", &valid);
      if (!valid)
        type = TP_CALL_STREAM_CANDIDATE_TYPE_HOST;

      protocol = tp_asv_get_uint32 (extra_info, "protocol", &valid);
      if (!valid)
        protocol = TP_MEDIA_STREAM_BASE_PROTO_UDP;

      base_ip = tp_asv_get_string (extra_info, "base-ip");
      base_port = tp_asv_get_uint32 (extra_info, "base-port", &valid);
      if (!valid)
        base_port = 0;


      ttl = tp_asv_get_uint32 (extra_info, "ttl", &valid);
      if (!valid)
        ttl = 0;

      g_debug ("Remote Candidate: %s c:%d tptype:%d tpproto: %d ip:%s port:%u prio:%d u/p:%s/%s ttl:%d base_ip:%s base_port:%d",
          foundation, component, type, protocol, ip, port, priority,
          username, password, ttl, base_ip, base_port);

      cand = fs_candidate_new (foundation, component,
          tpcandidate_type_to_fs (type), tpnetworkproto_to_fs (protocol),
          ip, port);
      cand->priority = priority;
      cand->username = g_strdup (username);
      cand->password = g_strdup (password);
      cand->ttl = ttl;
      cand->base_ip = base_ip;
      cand->base_port = base_port;

      fscandidates = g_list_append (fscandidates, cand);
    }

  if (self->fsstream)
    {
      gboolean ret;
      GError *error = NULL;

      switch (self->transport_type)
        {
        case TP_STREAM_TRANSPORT_TYPE_RAW_UDP:
        case TP_STREAM_TRANSPORT_TYPE_SHM:
        case TP_STREAM_TRANSPORT_TYPE_MULTICAST:
          ret = fs_stream_force_remote_candidates (self->fsstream,
              fscandidates, &error);
          break;
        case TP_STREAM_TRANSPORT_TYPE_ICE:
        case TP_STREAM_TRANSPORT_TYPE_GTALK_P2P:
        case TP_STREAM_TRANSPORT_TYPE_WLM_2009:
          ret = fs_stream_add_remote_candidates (self->fsstream, fscandidates,
              &error);
          break;
        default:
          ret = FALSE;
        }

      if (!ret)
        {
          tf_call_stream_fail (self,
              TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
              TP_ERROR_STR_MEDIA_STREAMING_ERROR,
              "Error setting the remote candidates: %s", error->message);
          g_clear_error (&error);
        }
      fs_candidate_list_destroy (fscandidates);
    }
  else
    {
      self->stored_remote_candidates =
          g_list_concat (self->stored_remote_candidates, fscandidates);
    }
}

static void
remote_candidates_added (TpProxy *proxy,
    const GPtrArray *arg_Candidates,
    gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);

  if (!self->has_endpoint_properties)
    return;

  if (self->endpoint != proxy)
    return;

  tf_call_stream_add_remote_candidates (self, arg_Candidates);
}

static void
remote_credentials_set (TpProxy *proxy,
    const gchar *arg_Username,
    const gchar *arg_Password,
    gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);

  if (self->endpoint != proxy)
    return;

  if ((self->creds_username && strcmp (self->creds_username, arg_Username)) ||
      (self->creds_password && strcmp (self->creds_password, arg_Password)))
    {
      g_debug ("Remote credentials changed,"
          " remote is doing an ICE restart");
      /* Remote credentials changed, this will perform a ICE restart, so
       * clear old remote candidates */
      fs_candidate_list_destroy (self->stored_remote_candidates);
      self->stored_remote_candidates = NULL;
    }

  g_free (self->creds_username);
  g_free (self->creds_password);
  self->creds_username = g_strdup (arg_Username);
  self->creds_password = g_strdup (arg_Password);

  g_debug ("Credentials set: %s / %s", arg_Username, arg_Password);
}


static void
got_endpoint_properties (TpProxy *proxy, GHashTable *out_Properties,
    const GError *error, gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);
  GValueArray *credentials;
  gchar *username, *password;
  GPtrArray *candidates;
  gboolean valid = FALSE;
  guint transport_type;

  if (self->endpoint != proxy)
    return;

  if (error)
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_CONFUSED,
          "Error getting the Streams's media properties: %s", error->message);
      return;
    }

  if (!out_Properties)
    {
      tf_call_stream_fail_literal (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_CONFUSED,
          "Error getting the Stream's media properties: there are none");
      return;
    }

  g_debug ("Got Endpoint Properties");


  credentials = tp_asv_get_boxed (out_Properties, "RemoteCredentials",
      TP_STRUCT_TYPE_STREAM_CREDENTIALS);
  if (!credentials)
    goto invalid_property;
  tp_value_array_unpack (credentials, 2, &username, &password);
  if (username && username[0])
    self->creds_username = g_strdup (username);
  if (password && password[0])
    self->creds_password = g_strdup (password);

  if (self->creds_username || self->creds_password)
    g_debug ("Credentials set: %s / %s", username, password);

  candidates = tp_asv_get_boxed (out_Properties, "RemoteCandidates",
      TP_ARRAY_TYPE_CANDIDATE_LIST);
  if (!candidates)
    goto invalid_property;

  transport_type = tp_asv_get_uint32 (out_Properties, "Transport", &valid);
  if (!valid)
  {
    g_warning ("No valid transport");
    goto invalid_property;
  }

  if (transport_type != self->transport_type)
    {
      if (transport_type != TP_STREAM_TRANSPORT_TYPE_RAW_UDP)
        {
          tf_call_stream_fail (self,
              TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
              TP_ERROR_STR_INVALID_ARGUMENT,
              "The Transport of a Endpoint can only be changed to rawudp: %d invalid", transport_type);
          return;
        }
      self->transport_type = transport_type;
    }

  self->has_endpoint_properties = TRUE;

  tf_call_stream_add_remote_candidates (self, candidates);

  tf_call_stream_update_sending_state (self);

  return;

 invalid_property:
  tf_call_stream_fail_literal (self,
      TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
      TP_ERROR_STR_CONFUSED,
      "Error getting the Endpoint's properties: invalid type");
}

static void
tf_call_stream_add_endpoint (TfCallStream *self, const gchar *obj_path)
{
  GError *error = NULL;

  self->endpoint_objpath = g_strdup (obj_path);

  tp_call_stream_endpoint_init_known_interfaces ();
  self->endpoint = g_object_new (TP_TYPE_PROXY,
      "dbus-daemon", tp_proxy_get_dbus_daemon (self->proxy),
      "bus-name", tp_proxy_get_bus_name (self->proxy),
      "object-path", self->endpoint_objpath,
      NULL);
  tp_proxy_add_interface_by_id (TP_PROXY (self->endpoint),
      TP_IFACE_QUARK_CALL_STREAM_ENDPOINT);

  tp_cli_call_stream_endpoint_connect_to_remote_credentials_set (
      TP_PROXY (self->endpoint), remote_credentials_set, NULL, NULL,
      G_OBJECT (self), &error);
  if (error)
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_CONFUSED,
          "Error connecting to RemoteCredentialsSet signal: %s",
          error->message);
      g_clear_error (&error);
      return;
    }

  tp_cli_call_stream_endpoint_connect_to_remote_candidates_added (
      TP_PROXY (self->endpoint), remote_candidates_added, NULL, NULL,
      G_OBJECT (self), &error);
  if (error)
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_CONFUSED,
          "Error connecting to RemoteCandidatesAdded signal: %s",
          error->message);
      g_clear_error (&error);
      return;
    }

  tp_cli_dbus_properties_call_get_all (self->endpoint, -1,
      TP_IFACE_CALL_STREAM_ENDPOINT,
      got_endpoint_properties, NULL, NULL, G_OBJECT (self));
}

static void
_tf_call_stream_remove_endpoint (TfCallStream *self)
{
  g_clear_object (&self->endpoint);

  self->has_endpoint_properties = FALSE;
  self->multiple_usernames = FALSE;
  self->controlling = FALSE;

  fs_candidate_list_destroy (self->stored_remote_candidates);
  self->stored_remote_candidates = NULL;

  g_free (self->creds_username);
  self->creds_username = NULL;

  g_free (self->creds_password);
  self->creds_password = NULL;

  g_free (self->endpoint_objpath);
  self->endpoint_objpath = NULL;

  tf_call_stream_update_sending_state (self);
}

static void
endpoints_changed (TpCallStream *proxy,
    const GPtrArray *arg_Endpoints_Added,
    const GPtrArray *arg_Endpoints_Removed,
    gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);

  /* Ignore signals before getting the properties to avoid races */
  if (!self->has_media_properties)
    return;

  if (arg_Endpoints_Removed->len == 1)
    {
      if (self->endpoint_objpath == NULL ||
          strcmp (self->endpoint_objpath,
              g_ptr_array_index (arg_Endpoints_Removed, 0)))

        {
          tf_call_stream_fail_literal (self,
              TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
              TP_ERROR_STR_CONFUSED,
              "Can not remove endpoint that has not been previously added");
          return;
        }
      _tf_call_stream_remove_endpoint (self);
    }
  else if (arg_Endpoints_Removed->len > 1)
    {
      tf_call_stream_fail_literal (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_NOT_IMPLEMENTED,
          "Having more than one endpoint is not implemented");
      return;
    }

  /* Nothing added, it's over */
  if (arg_Endpoints_Added->len == 0)
    return;

  if (arg_Endpoints_Added->len > 1)
    {
      tf_call_stream_fail_literal (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_NOT_IMPLEMENTED,
          "Having more than one endpoint is not implemented");
      return;
    }

  if (self->endpoint_objpath)
    {
      if (strcmp (g_ptr_array_index (arg_Endpoints_Added, 0),
              self->endpoint_objpath))
        tf_call_stream_fail_literal (self,
            TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
            TP_ERROR_STR_NOT_IMPLEMENTED,
            "Having more than one endpoint is not implemented");
      return;
    }

  tf_call_stream_add_endpoint (self,
      g_ptr_array_index (arg_Endpoints_Added, 0));
}


static void
got_stream_media_properties (TpProxy *proxy, GHashTable *out_Properties,
    const GError *error, gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);
  GPtrArray *stun_servers;
  GPtrArray *relay_info;
  GPtrArray *endpoints;
  gboolean valid;

  if (error)
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_CONFUSED,
          "Error getting the Streams's media properties: %s",
          error->message);
      return;
    }

  if (!out_Properties)
    {
      tf_call_stream_fail_literal (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_INVALID_ARGUMENT,
          "Error getting the Stream's media properties: there are none");
      return;
    }

  self->transport_type =
      tp_asv_get_uint32 (out_Properties, "Transport", &valid);
  if (!valid)
  {
    g_warning ("No valid transport");
    goto invalid_property;
  }

  stun_servers = tp_asv_get_boxed (out_Properties, "STUNServers",
      TP_ARRAY_TYPE_SOCKET_ADDRESS_IP_LIST);
  if (!stun_servers)
  {
    g_warning ("No valid STUN servers");
    goto invalid_property;
  }

  relay_info = tp_asv_get_boxed (out_Properties, "RelayInfo",
      TP_ARRAY_TYPE_STRING_VARIANT_MAP_LIST);
  if (!relay_info)
  {
    g_warning ("No valid RelayInfo");
    goto invalid_property;
  }

  self->server_info_retrieved = tp_asv_get_boolean (out_Properties,
      "HasServerInfo", &valid);
  if (!valid)
  {
    g_warning ("No valid server info");
    goto invalid_property;
  }

  self->sending_state = tp_asv_get_uint32 (out_Properties, "SendingState",
      &valid);
  if (!valid)
    {
      g_warning ("No valid sending state");
      goto invalid_property;
    }

  self->receiving_state = tp_asv_get_uint32 (out_Properties,
      "ReceivingState", &valid);
  if (!valid)
    {
      g_warning ("No valid receiving state");
      goto invalid_property;
    }

/* FIXME: controlling is on the endpoint
  self->controlling = tp_asv_get_boolean (out_Properties,
      "Controlling", &valid);
  if (!valid)
  {
    g_warning ("No Controlling property");
    goto invalid_property;
  }
*/
  self->stun_servers = g_boxed_copy (TP_ARRAY_TYPE_SOCKET_ADDRESS_IP_LIST,
      stun_servers);
  self->relay_info = g_boxed_copy (TP_ARRAY_TYPE_STRING_VARIANT_MAP_LIST,
      relay_info);

  endpoints = tp_asv_get_boxed (out_Properties, "Endpoints",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST);

  if (endpoints->len > 1)
    {
      tf_call_stream_fail_literal (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_NOT_IMPLEMENTED,
          "Having more than one endpoint is not implemented");
      return;
    }

  if (endpoints->len == 1)
    {
      tf_call_stream_add_endpoint (self, g_ptr_array_index (endpoints, 0));
    }

  self->has_media_properties = TRUE;

  tf_call_stream_try_adding_fsstream (self);

  return;
 invalid_property:
  tf_call_stream_fail_literal (self,
      TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
      TP_ERROR_STR_INVALID_ARGUMENT,
      "Error getting the Stream's properties: invalid type");
  return;
}

static void
ice_restart_requested (TpCallStream *proxy,
    gpointer user_data, GObject *weak_object)
{
  TfCallStream *self = TF_CALL_STREAM (weak_object);
  GError *myerror = NULL;

  if (!self->fsstream)
    return;

  if (self->multiple_usernames)
    {
      tf_call_stream_fail_literal (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_INVALID_ARGUMENT,
          "CM tried to ICE restart an ICE-6 or Google compatible connection");
      return;
    }

  g_debug ("Restarting ICE");

  if (fs_stream_add_remote_candidates (self->fsstream, NULL, &myerror))
    {
      g_free (self->last_local_username);
      g_free (self->last_local_password);
      self->last_local_username = NULL;
      self->last_local_password = NULL;
    }
  else
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_MEDIA_STREAMING_ERROR,
          "Error restarting the ICE process: %s", myerror->message);
      g_clear_error (&myerror);
    }
}

static void
stream_prepared (GObject *src_object, GAsyncResult *res, gpointer user_data)
{
  TfCallStream *self = TF_CALL_STREAM (user_data);
  TpProxy *proxy = TP_PROXY (src_object);
  GError *error = NULL;
  GHashTable *members;
  GHashTableIter iter;
  gpointer key, value;

  if (!tp_proxy_prepare_finish (src_object, res, &error))
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_CONFUSED,
          "Error preparing the stream Streams: %s", error->message);
      g_clear_error (&error);
      return;
    }

 if (!tp_proxy_has_interface_by_id (proxy,
         TP_IFACE_QUARK_CALL_STREAM_INTERFACE_MEDIA))
   {
     tf_call_stream_fail_literal (self,
         TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
         TP_ERROR_STR_INVALID_ARGUMENT,
         "Stream does not have the media interface,"
         " but HardwareStreaming was NOT true");
      return;
   }

 members = tp_call_stream_get_remote_members (self->proxy);

 if (g_hash_table_size (members) != 1)
   {
     tf_call_stream_fail (self,
         TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
         TP_ERROR_STR_NOT_IMPLEMENTED,
         "Only one Member per Stream is supported, there are %d",
         g_hash_table_size (members));
     return;
   }

  g_hash_table_iter_init (&iter, members);
  if (g_hash_table_iter_next (&iter, &key, &value))
    {
      self->has_contact = TRUE;
      self->contact_handle = tp_contact_get_handle (key);
    }

  tp_cli_call_stream_interface_media_connect_to_sending_state_changed (
      TP_CALL_STREAM (proxy), sending_state_changed, NULL, NULL,
      G_OBJECT (self), &error);
  if (error)
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR, "",
          "Error connecting to SendingStateChanged signal: %s",
          error->message);
      g_clear_error (&error);
      return;
    }


  tp_cli_call_stream_interface_media_connect_to_receiving_state_changed (
      TP_CALL_STREAM (proxy), receiving_state_changed, NULL, NULL,
      G_OBJECT (self), &error);
  if (error)
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR, "",
          "Error connecting to ReceivingStateChanged signal: %s",
          error->message);
      g_clear_error (&error);
      return;
    }

  tp_cli_call_stream_interface_media_connect_to_server_info_retrieved (
      TP_CALL_STREAM (proxy), server_info_retrieved, NULL, NULL,
      G_OBJECT (self), &error);
  if (error)
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR, "",
          "Error connecting to ServerInfoRetrived signal: %s",
          error->message);
      g_clear_error (&error);
      return;
    }

  tp_cli_call_stream_interface_media_connect_to_stun_servers_changed (
      TP_CALL_STREAM (proxy), stun_servers_changed, NULL, NULL,
      G_OBJECT (self), &error);
  if (error)
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR, "",
          "Error connecting to ServerInfoRetrived signal: %s",
          error->message);
      g_clear_error (&error);
      return;
    }


  tp_cli_call_stream_interface_media_connect_to_relay_info_changed (
      TP_CALL_STREAM (proxy), relay_info_changed, NULL, NULL,
      G_OBJECT (self), &error);
  if (error)
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR, "",
          "Error connecting to ServerInfoRetrived signal: %s",
          error->message);
      g_clear_error (&error);
      return;
    }


  tp_cli_call_stream_interface_media_connect_to_endpoints_changed (
      TP_CALL_STREAM (proxy), endpoints_changed, NULL, NULL,
      G_OBJECT (self), &error);
  if (error)
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR, "",
          "Error connecting to EndpointsChanged signal: %s",
          error->message);
      g_clear_error (&error);
      return;
    }


  tp_cli_call_stream_interface_media_connect_to_ice_restart_requested (
      TP_CALL_STREAM (proxy), ice_restart_requested, NULL, NULL,
      G_OBJECT (self), &error);
  if (error)
    {
      tf_call_stream_fail (self,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR, "",
          "Error connecting to ICERestartRequested signal: %s",
          error->message);
      g_clear_error (&error);
      return;
    }

  tp_cli_dbus_properties_call_get_all (TP_PROXY (self->proxy), -1,
      TP_IFACE_CALL_STREAM_INTERFACE_MEDIA,
      got_stream_media_properties, NULL, NULL, G_OBJECT (self));

  return;
}

TfCallStream *
tf_call_stream_new (TfCallContent *call_content,
    TpCallStream *stream_proxy)
{
  TfCallStream *self;

  g_assert (call_content != NULL);
  g_assert (stream_proxy != NULL);

  self = g_object_new (TF_TYPE_CALL_STREAM, NULL);

  self->call_content = call_content;
  self->proxy = g_object_ref (stream_proxy);

  tp_proxy_prepare_async (self->proxy, NULL, stream_prepared,
      g_object_ref (self));

  return self;
}

static TpCallStreamCandidateType
fscandidatetype_to_tp (FsCandidateType type)
{
 switch(type)
    {
    case FS_CANDIDATE_TYPE_HOST:
      return TP_CALL_STREAM_CANDIDATE_TYPE_HOST;
    case FS_CANDIDATE_TYPE_SRFLX:
      return TP_CALL_STREAM_CANDIDATE_TYPE_SERVER_REFLEXIVE;
    case FS_CANDIDATE_TYPE_PRFLX:
      return TP_CALL_STREAM_CANDIDATE_TYPE_PEER_REFLEXIVE;
    case FS_CANDIDATE_TYPE_RELAY:
      return TP_CALL_STREAM_CANDIDATE_TYPE_RELAY;
    case FS_CANDIDATE_TYPE_MULTICAST:
      return TP_CALL_STREAM_CANDIDATE_TYPE_MULTICAST;
    default:
      g_warning ("Unkown candidate type, assigning type NONE");
      return TP_CALL_STREAM_CANDIDATE_TYPE_NONE;
    }
}


static TpMediaStreamBaseProto
fs_network_proto_to_tp (FsNetworkProtocol proto)
{
  switch (proto)
    {
    case FS_NETWORK_PROTOCOL_UDP:
      return TP_MEDIA_STREAM_BASE_PROTO_UDP;
    case FS_NETWORK_PROTOCOL_TCP:
      return TP_MEDIA_STREAM_BASE_PROTO_TCP;
    default:
      g_warning ("Invalid protocl, assigning to UDP");
      return TP_MEDIA_STREAM_BASE_PROTO_UDP;
    }
}


static GValueArray *
fscandidate_to_tpcandidate (TfCallStream *stream, FsCandidate *candidate)
{
  GHashTable *extra_info;

  extra_info = tp_asv_new (NULL, NULL);

  tp_asv_set_uint32 (extra_info, "type",
      fscandidatetype_to_tp (candidate->type));

  if (candidate->foundation)
    tp_asv_set_string (extra_info, "foundation", candidate->foundation);

  tp_asv_set_uint32 (extra_info, "protocol",
      fs_network_proto_to_tp (candidate->proto));

  if (candidate->base_ip)
    {
      tp_asv_set_string (extra_info, "base-ip", candidate->base_ip);
      tp_asv_set_uint32 (extra_info, "base-port", candidate->base_port);
    }

  if (candidate->priority)
    tp_asv_set_uint32 (extra_info, "priority", candidate->priority);


  if (candidate->type == FS_CANDIDATE_TYPE_MULTICAST)
    tp_asv_set_uint32 (extra_info, "ttl", candidate->ttl);

  if (stream->multiple_usernames)
    {
      if (candidate->username)
        tp_asv_set_string (extra_info, "username", candidate->username);
      if (candidate->password)
        tp_asv_set_string (extra_info, "password", candidate->password);
    }


  return tp_value_array_build (4,
      G_TYPE_UINT, candidate->component_id,
      G_TYPE_STRING, candidate->ip,
      G_TYPE_UINT, candidate->port,
      TP_HASH_TYPE_CANDIDATE_INFO, extra_info,
      G_TYPE_INVALID);
}

static void
cb_fs_new_local_candidate (TfCallStream *stream, FsCandidate *candidate)
{
  GPtrArray *candidate_list = g_ptr_array_sized_new (1);

  if (!stream->multiple_usernames)
    {
      if ((!stream->last_local_username && candidate->username) ||
          (!stream->last_local_password && candidate->password) ||
          (stream->last_local_username &&
              strcmp (candidate->username, stream->last_local_username)) ||
          (stream->last_local_password &&
              strcmp (candidate->password, stream->last_local_password)))
        {
          g_free (stream->last_local_username);
          g_free (stream->last_local_password);
          stream->last_local_username = g_strdup (candidate->username);
          stream->last_local_password = g_strdup (candidate->password);

          if (!stream->last_local_username)
            stream->last_local_username = g_strdup ("");
          if (!stream->last_local_password)
            stream->last_local_password = g_strdup ("");

          /* Add a callback to kill Call on errors */
          tp_cli_call_stream_interface_media_call_set_credentials (
              stream->proxy, -1, stream->last_local_username,
              stream->last_local_password, NULL, NULL, NULL, NULL);

        }
    }

  g_debug ("Local Candidate: %s c:%d fstype:%d fsproto: %d ip:%s port:%u prio:%d u/p:%s/%s ttl:%d base_ip:%s base_port:%d",
      candidate->foundation,candidate->component_id, candidate->type,
      candidate->proto, candidate->ip, candidate->port,
      candidate->priority, candidate->username, candidate->password,
      candidate->ttl,candidate-> base_ip, candidate->base_port);


  g_ptr_array_add (candidate_list,
      fscandidate_to_tpcandidate (stream, candidate));

  /* Should also check for errors */
  tp_cli_call_stream_interface_media_call_add_candidates (stream->proxy,
      -1, candidate_list, NULL, NULL, NULL, NULL);


  g_boxed_free (TP_ARRAY_TYPE_CANDIDATE_LIST, candidate_list);
}

static void
cb_fs_local_candidates_prepared (TfCallStream *stream)
{
  g_debug ("Local candidates prepared");

  tp_cli_call_stream_interface_media_call_finish_initial_candidates (
      stream->proxy, -1, NULL, NULL, NULL, NULL);
}

static void
cb_fs_component_state_changed (TfCallStream *stream, guint component,
    FsStreamState fsstate)
{
  TpMediaStreamState state;

  if (!stream->endpoint)
    return;

  switch (fsstate)
  {
    default:
      g_warning ("Unknown Farstream state, returning ExhaustedCandidates");
      /* fall through */
    case FS_STREAM_STATE_FAILED:
      state = TP_STREAM_ENDPOINT_STATE_EXHAUSTED_CANDIDATES;
      break;
    case FS_STREAM_STATE_DISCONNECTED:
    case FS_STREAM_STATE_GATHERING:
    case FS_STREAM_STATE_CONNECTING:
      state = TP_STREAM_ENDPOINT_STATE_CONNECTING;
      break;
    case FS_STREAM_STATE_CONNECTED:
      state = TP_STREAM_ENDPOINT_STATE_PROVISIONALLY_CONNECTED;
    case FS_STREAM_STATE_READY:
      state = TP_STREAM_ENDPOINT_STATE_FULLY_CONNECTED;
      break;
  }

  g_debug ("Endpoint state changed to %d (fs: %d)",
      state, fsstate);

  tp_cli_call_stream_endpoint_call_set_endpoint_state (stream->endpoint,
      -1, component, state, NULL, NULL, NULL, NULL);
}

static void
cb_fs_new_active_candidate_pair (TfCallStream *stream,
    FsCandidate *local_candidate,
    FsCandidate *remote_candidate)
{
  GValueArray *local_tp_candidate;
  GValueArray *remote_tp_candidate;

  g_debug ("new active candidate pair local: %s (%d) remote: %s (%d)",
      local_candidate->ip, local_candidate->port,
      remote_candidate->ip, remote_candidate->port);

  if (!stream->endpoint)
    return;

  local_tp_candidate =_to_tpcandidate (stream, local_candidate);
  remote_tp_candidate = fscandidate_to_tpcandidate (stream, remote_candidate);

  tp_cli_call_stream_endpoint_call_set_selected_candidate_pair (
      stream->endpoint, -1, local_tp_candidate, remote_tp_candidate,
      NULL, NULL, NULL, NULL);

  g_boxed_free (TP_STRUCT_TYPE_CANDIDATE, local_tp_candidate);
  g_boxed_free (TP_STRUCT_TYPE_CANDIDATE, remote_tp_candidate);
}

gboolean
tf_call_stream_bus_message (TfCallStream *stream, GstMessage *message)
{
  FsError errorno;
  const gchar *msg;
  FsCandidate *candidate;
  guint component;
  FsStreamState fsstate;
  FsCandidate *local_candidate;
  FsCandidate *remote_candidate;

  if (!stream->fsstream)
    return FALSE;

  if (fs_parse_error (G_OBJECT (stream->fsstream), message, &errorno, &msg))
    {
      GEnumClass *enumclass;
      GEnumValue *enumvalue;

      enumclass = g_type_class_ref (FS_TYPE_ERROR);
      enumvalue = g_enum_get_value (enumclass, errorno);
      g_warning ("error (%s (%d)): %s",
          enumvalue->value_nick, errorno, msg);
      g_type_class_unref (enumclass);

      tf_call_stream_fail_literal (stream,
          TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
          TP_ERROR_STR_MEDIA_STREAMING_ERROR, msg);
    }
  else if (fs_stream_parse_new_local_candidate (stream->fsstream, message,
          &candidate))
    {
      cb_fs_new_local_candidate (stream, candidate);
    }
  else if (fs_stream_parse_local_candidates_prepared (stream->fsstream,
          message))
    {
      cb_fs_local_candidates_prepared (stream);
    }
  else if (fs_stream_parse_component_state_changed (stream->fsstream, message,
          &component, &fsstate))
    {
      cb_fs_component_state_changed (stream, component, fsstate);
    }
  else if (fs_stream_parse_new_active_candidate_pair (stream->fsstream, message,
          &local_candidate, &remote_candidate))
    {
      cb_fs_new_active_candidate_pair (stream, local_candidate,
          remote_candidate);
    }
  else
    {
      return FALSE;
    }

  return TRUE;
}

static void
tf_call_stream_fail_literal (TfCallStream *self,
    TpCallStateChangeReason reason,
    const gchar *detailed_reason,
    const gchar *message)
{
  g_warning ("%s", message);
  tp_cli_call_stream_interface_media_call_fail (
      self->proxy, -1,
      tp_value_array_build (4,
          G_TYPE_UINT, 0,
          G_TYPE_UINT, reason,
          G_TYPE_STRING, detailed_reason,
          G_TYPE_STRING, message,
          G_TYPE_INVALID),
      NULL, NULL, NULL, NULL);
}


static void
tf_call_stream_fail (TfCallStream *self,
    TpCallStateChangeReason reason,
    const gchar *detailed_reason,
    const gchar *message_format,
    ...)
{
  gchar *message;
  va_list valist;

  va_start (valist, message_format);
  message = g_strdup_vprintf (message_format, valist);
  va_end (valist);

  tf_call_stream_fail_literal (self, reason, detailed_reason, message);
  g_free (message);
}

void
tf_call_stream_sending_failed (TfCallStream *self, const gchar *message)
{
  g_warning ("Reporting sending failure: %s", message);

  tp_cli_call_stream_interface_media_call_report_sending_failure (
      self->proxy, -1, TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
      TP_ERROR_STR_MEDIA_STREAMING_ERROR,
      message, NULL, NULL, NULL, NULL);
}


void
tf_call_stream_receiving_failed (TfCallStream *self,
    guint *handles, guint handle_count,
    const gchar *message)
{
  if (handle_count && handle_count > 0)
    {
      guint i;

      for (i = 0; i < handle_count; i++)
        if (handles[i] == self->contact_handle)
          goto ok;
      return;
    }
 ok:

  g_warning ("Reporting receiving failure: %s", message);

  tp_cli_call_stream_interface_media_call_report_receiving_failure (
      self->proxy, -1, TP_CALL_STATE_CHANGE_REASON_INTERNAL_ERROR,
      TP_ERROR_STR_MEDIA_STREAMING_ERROR,
      message, NULL, NULL, NULL, NULL);
}


TpCallStream *
tf_call_stream_get_proxy (TfCallStream *stream)
{
  g_return_val_if_fail (TF_IS_CALL_STREAM (stream), NULL);

  return stream->proxy;
}
