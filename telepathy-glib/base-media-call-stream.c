/*
 * base-media-call-stream.c - Source for TpBaseMediaCallStream
 * Copyright (C) 2009-2011 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
 * @author Jonny Lamb <jonny.lamb@collabora.co.uk>
 * @author David Laban <david.laban@collabora.co.uk>
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
 * SECTION:base-media-call-stream
 * @title: TpBaseMediaCallStream
 * @short_description: base class for #TpSvcCallStreamInterfaceMedia
 *  implementations
 * @see_also: #TpSvcCallStreamInterfaceMedia, #TpBaseCallChannel,
 *  #TpBaseCallStream and #TpBaseCallContent
 *
 * This base class makes it easier to write #TpSvcCallStreamInterfaceMedia
 * implementations by implementing some of its properties and methods.
 *
 * Subclasses must still implement #TpBaseCallStream's virtual methods plus
 * #TpBaseMediaCallStreamClass.add_local_candidates and
 * #TpBaseMediaCallStreamClass.finish_initial_candidates.
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpBaseMediaCallStream:
 *
 * A base class for media call stream implementations
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpBaseMediaCallStreamClass:
 * @report_sending_failure: optional; called to indicate a failure in the
 *  outgoing portion of the stream
 * @report_receiving_failure: optional; called to indicate a failure in the
 *  incoming portion of the stream
 * @add_local_candidates: mandatory; called when new candidates are added
 * @finish_initial_candidates: optional; called when the initial batch of
 *  candidates has been added, and should now be processed/sent to the remote
 *  side
 *
 * The class structure for #TpBaseMediaCallStream
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpBaseMediaCallStreamVoidFunc:
 * @self: a #TpBaseMediaCallStream
 *
 * Signature of an implementation of
 * #TpBaseMediaCallStreamClass.finish_initial_candidates.
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpBaseMediaCallStreamAddCandidatesFunc:
 * @self: a #TpBaseMediaCallStream
 * @candidates: a #GPtrArray of #GValueArray containing candidates info
 * @error: a #GError to fill
 *
 * Signature of an implementation of
 * #TpBaseMediaCallStreamClass.add_local_candidates.
 *
 * Implementation should validate the added @candidates and return a subset
 * (or all) of them that are accepted. Implementation should return a new
 * #GPtrArray build in a way that g_ptr_array_unref() is enough to free all its
 * memory. It is fine to just add element pointers from @candidates to the
 * returned #GPtrArray without deep-copy them.
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpBaseMediaCallStreamReportFailureFunc:
 * @self: a #TpBaseMediaCallStream
 * @reason: the #TpCallStateChangeReason of the change
 * @dbus_reason: a specific reason for the change, which may be a D-Bus error in
 *  the Telepathy namespace, a D-Bus error in any other namespace (for
 *  implementation-specific errors), or the empty string to indicate that the
 *  state change was not an error.
 * @message: an optional debug message, to expediate debugging the potentially
 *  many processes involved in a call.
 *
 * Signature of an implementation of
 * #TpBaseMediaCallStreamClass.report_sending_failure and
 * #TpBaseMediaCallStreamClass.report_receiving_failure.
 *
 * Since: 0.UNRELEASED
 */

#include "config.h"
#include "base-media-call-stream.h"

#define DEBUG_FLAG TP_DEBUG_CALL
#include "telepathy-glib/base-call-content.h"
#include "telepathy-glib/base-call-internal.h"
#include "telepathy-glib/base-channel.h"
#include "telepathy-glib/base-connection.h"
#include "telepathy-glib/call-stream-endpoint.h"
#include "telepathy-glib/dbus.h"
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/enums.h"
#include "telepathy-glib/gtypes.h"
#include "telepathy-glib/interfaces.h"
#include "telepathy-glib/svc-properties-interface.h"
#include "telepathy-glib/svc-call.h"
#include "telepathy-glib/util.h"

static void call_stream_media_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE(TpBaseMediaCallStream, tp_base_media_call_stream,
   TP_TYPE_BASE_CALL_STREAM,
   G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CALL_STREAM_INTERFACE_MEDIA,
    call_stream_media_iface_init);
  );

static const gchar *tp_base_media_call_stream_interfaces[] = {
    TP_IFACE_CALL_STREAM_INTERFACE_MEDIA,
    NULL
};

/* properties */
enum
{
  PROP_SENDING_STATE = 1,
  PROP_RECEIVING_STATE,
  PROP_TRANSPORT,
  PROP_LOCAL_CANDIDATES,
  PROP_LOCAL_CREDENTIALS,
  PROP_STUN_SERVERS,
  PROP_RELAY_INFO,
  PROP_HAS_SERVER_INFO,
  PROP_ENDPOINTS,
  PROP_ICE_RESTART_PENDING
};

/* private structure */
struct _TpBaseMediaCallStreamPrivate
{
  TpStreamFlowState sending_state;
  TpStreamFlowState receiving_state;
  TpStreamTransportType transport;
  /* GPtrArray of owned GValueArray (dbus struct) */
  GPtrArray *local_candidates;
  gchar *username;
  gchar *password;
  /* GPtrArray of owned GValueArray (dbus struct) */
  GPtrArray *stun_servers;
  /* GPtrArray of reffed GHashTable (asv) */
  GPtrArray *relay_info;
  gboolean has_server_info;
  /* GList of reffed TpCallStreamEndpoint */
  GList *endpoints;
  gboolean ice_restart_pending;
  /* Array of TpHandle that have requested to receive */
  GArray *receiving_requests;
};

static gboolean tp_base_media_call_stream_request_receiving (
    TpBaseCallStream *bcs, TpHandle contact, gboolean receive, GError **error);
static gboolean tp_base_media_call_stream_set_sending (TpBaseCallStream *self,
    gboolean sending, GError **error);

static void
tp_base_media_call_stream_init (TpBaseMediaCallStream *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_MEDIA_CALL_STREAM, TpBaseMediaCallStreamPrivate);

  self->priv->local_candidates = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_value_array_free);
  self->priv->username = g_strdup ("");
  self->priv->password = g_strdup ("");
  self->priv->receiving_requests = g_array_new (TRUE, FALSE, sizeof (TpHandle));
  self->priv->sending_state = TP_STREAM_FLOW_STATE_STOPPED;
  self->priv->receiving_state = TP_STREAM_FLOW_STATE_STOPPED;
}

static void
endpoints_list_destroy (GList *endpoints)
{
  g_list_free_full (endpoints, g_object_unref);
}

static void
tp_base_media_call_stream_dispose (GObject *object)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (object);

  tp_clear_pointer (&self->priv->endpoints, endpoints_list_destroy);

  if (G_OBJECT_CLASS (tp_base_media_call_stream_parent_class)->dispose)
    G_OBJECT_CLASS (tp_base_media_call_stream_parent_class)->dispose (object);
}

static void
tp_base_media_call_stream_finalize (GObject *object)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (object);

  tp_clear_pointer (&self->priv->local_candidates, g_ptr_array_unref);
  tp_clear_pointer (&self->priv->stun_servers, g_ptr_array_unref);
  tp_clear_pointer (&self->priv->relay_info, g_ptr_array_unref);
  tp_clear_pointer (&self->priv->username, g_free);
  tp_clear_pointer (&self->priv->password, g_free);
  g_array_free (self->priv->receiving_requests, TRUE);

  G_OBJECT_CLASS (tp_base_media_call_stream_parent_class)->finalize (object);
}

static void
tp_base_media_call_stream_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (object);

  switch (property_id)
    {
      case PROP_SENDING_STATE:
        g_value_set_uint (value, self->priv->sending_state);
        break;
      case PROP_RECEIVING_STATE:
        g_value_set_uint (value, self->priv->receiving_state);
        break;
      case PROP_TRANSPORT:
        g_value_set_uint (value, self->priv->transport);
        break;
      case PROP_LOCAL_CANDIDATES:
        g_value_set_boxed (value, self->priv->local_candidates);
        break;
      case PROP_LOCAL_CREDENTIALS:
        {
          g_value_take_boxed (value, tp_value_array_build (2,
              G_TYPE_STRING, self->priv->username,
              G_TYPE_STRING, self->priv->password,
              G_TYPE_INVALID));
          break;
        }
      case PROP_STUN_SERVERS:
        {
          if (self->priv->stun_servers != NULL)
            g_value_set_boxed (value, self->priv->stun_servers);
          else
            g_value_take_boxed (value, g_ptr_array_new ());
          break;
        }
      case PROP_RELAY_INFO:
        {
          if (self->priv->relay_info != NULL)
            g_value_set_boxed (value, self->priv->relay_info);
          else
            g_value_take_boxed (value, g_ptr_array_new ());
          break;
        }
      case PROP_HAS_SERVER_INFO:
        g_value_set_boolean (value, self->priv->has_server_info);
        break;
      case PROP_ENDPOINTS:
        {
          GPtrArray *arr = g_ptr_array_sized_new (1);
          GList *l;

          for (l = self->priv->endpoints; l != NULL; l = g_list_next (l))
            {
              TpCallStreamEndpoint *e = l->data;

              g_ptr_array_add (arr,
                  g_strdup (tp_call_stream_endpoint_get_object_path (e)));
            }

          g_value_take_boxed (value, arr);
          break;
        }
      case PROP_ICE_RESTART_PENDING:
        g_value_set_boolean (value, self->priv->ice_restart_pending);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
tp_base_media_call_stream_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (object);

  switch (property_id)
    {
      case PROP_TRANSPORT:
        self->priv->transport = g_value_get_uint (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
tp_base_media_call_stream_class_init (TpBaseMediaCallStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;
  TpBaseCallStreamClass *bcs_class = TP_BASE_CALL_STREAM_CLASS (klass);

  static TpDBusPropertiesMixinPropImpl stream_media_props[] = {
    { "SendingState", "sending-state", NULL },
    { "ReceivingState", "receiving-state", NULL },
    { "Transport", "transport", NULL },
    { "LocalCandidates", "local-candidates", NULL },
    { "LocalCredentials", "local-credentials", NULL },
    { "STUNServers", "stun-servers", NULL },
    { "RelayInfo", "relay-info", NULL },
    { "HasServerInfo", "has-server-info", NULL },
    { "Endpoints", "endpoints", NULL },
    { "ICERestartPending", "ice-restart-pending", NULL },
    { NULL }
  };

  g_type_class_add_private (klass, sizeof (TpBaseMediaCallStreamPrivate));

  object_class->set_property = tp_base_media_call_stream_set_property;
  object_class->get_property = tp_base_media_call_stream_get_property;
  object_class->dispose = tp_base_media_call_stream_dispose;
  object_class->finalize = tp_base_media_call_stream_finalize;

  bcs_class->extra_interfaces = tp_base_media_call_stream_interfaces;
  bcs_class->request_receiving = tp_base_media_call_stream_request_receiving;
  bcs_class->set_sending = tp_base_media_call_stream_set_sending;;

  /**
   * TpBaseMediaCallStream:sending-state:
   *
   * The sending #TpStreamFlowState.
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_uint ("sending-state", "SendingState",
      "The sending state",
      0, G_MAXUINT, TP_STREAM_FLOW_STATE_STOPPED,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SENDING_STATE,
      param_spec);

  /**
   * TpBaseMediaCallStream:receiving-state:
   *
   * The receiving #TpStreamFlowState.
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_uint ("receiving-state", "ReceivingState",
      "The receiving state",
      0, G_MAXUINT, TP_STREAM_FLOW_STATE_STOPPED,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_RECEIVING_STATE,
      param_spec);

  /**
   * TpBaseMediaCallStream:transport:
   *
   * The #TpStreamTransportType of this stream.
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_uint ("transport", "Transport",
      "The transport type of this stream",
      0, G_MAXUINT, TP_STREAM_TRANSPORT_TYPE_UNKNOWN,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TRANSPORT,
      param_spec);

  /**
   * TpBaseMediaCallStream:local-candidates:
   *
   * #GPtrArray{candidate #GValueArray}
   * List of local candidates.
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_boxed ("local-candidates", "LocalCandidates",
      "List of local candidates",
      TP_ARRAY_TYPE_CANDIDATE_LIST,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LOCAL_CANDIDATES,
      param_spec);

  /**
   * TpBaseMediaCallStream:local-credentials:
   *
   * #GValueArray{username string, password string}
   * ufrag and pwd as defined by ICE.
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_boxed ("local-credentials", "LocalCredentials",
      "ufrag and pwd as defined by ICE",
      TP_STRUCT_TYPE_STREAM_CREDENTIALS,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LOCAL_CREDENTIALS,
      param_spec);

  /**
   * TpBaseMediaCallStream:stun-servers:
   *
   * #GPtrArray{stun-server #GValueArray}
   * List of STUN servers.
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_boxed ("stun-servers", "STUNServers",
      "List of STUN servers",
      TP_ARRAY_TYPE_SOCKET_ADDRESS_IP_LIST,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STUN_SERVERS,
      param_spec);

  /**
   * TpBaseMediaCallStream:relay-info:
   *
   * #GPtrArray{relay-info asv}
   * List of relay information.
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_boxed ("relay-info", "RelayInfo",
      "List of relay information",
      TP_ARRAY_TYPE_STRING_VARIANT_MAP_LIST,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_RELAY_INFO,
      param_spec);

  /**
   * TpBaseMediaCallStream:has-server-info:
   *
   * %TRUE if #TpBaseMediaCallStream:relay-info and
   * #TpBaseMediaCallStream:stun-servers have been set.
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_boolean ("has-server-info", "HasServerInfo",
      "True if the server information about STUN and "
      "relay servers has been retrieved",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_HAS_SERVER_INFO,
      param_spec);

  /**
   * TpBaseMediaCallStream:endpoints:
   *
   * #GPtrArray{object-path string}
   * The endpoints of this content.
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_boxed ("endpoints", "Endpoints",
      "The endpoints of this content",
      TP_ARRAY_TYPE_OBJECT_PATH_LIST,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ENDPOINTS,
      param_spec);

  /**
   * TpBaseMediaCallStream:ice-restart-pending:
   *
   * %TRUE when ICERestartRequested signal is emitted, and %FALSE when
   * SetCredentials is called. Useful for debugging.
   *
   * Since: 0.UNRELEASED
   */
  param_spec = g_param_spec_boolean ("ice-restart-pending", "ICERestartPending",
      "True when ICERestartRequested signal is emitted, and False when "
      "SetCredentials is called. Useful for debugging",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ICE_RESTART_PENDING,
      param_spec);

  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CALL_STREAM_INTERFACE_MEDIA,
      tp_dbus_properties_mixin_getter_gobject_properties,
      NULL,
      stream_media_props);
}

/**
 * tp_base_media_call_stream_get_username:
 * @self: a #TpBaseMediaCallStream
 *
 * <!-- -->
 *
 * Returns: the username part of #TpBaseMediaCallStream:local-credentials
 * Since: 0.UNRELEASED
 */
const gchar *
tp_base_media_call_stream_get_username (TpBaseMediaCallStream *self)
{
  g_return_val_if_fail (TP_IS_BASE_MEDIA_CALL_STREAM (self), NULL);

  return self->priv->username;
}

/**
 * tp_base_media_call_stream_get_password:
 * @self: a #TpBaseMediaCallStream
 *
 * <!-- -->
 *
 * Returns: the password part of #TpBaseMediaCallStream:local-credentials
 * Since: 0.UNRELEASED
 */
const gchar *
tp_base_media_call_stream_get_password (TpBaseMediaCallStream *self)
{
  g_return_val_if_fail (TP_IS_BASE_MEDIA_CALL_STREAM (self), NULL);

  return self->priv->password;
}

static void
maybe_got_server_info (TpBaseMediaCallStream *self)
{
  if (self->priv->has_server_info ||
      self->priv->stun_servers == NULL ||
      self->priv->relay_info == NULL)
    return;

  DEBUG ("Got server info for stream %s",
      tp_base_call_stream_get_object_path ((TpBaseCallStream *) self));

  self->priv->has_server_info = TRUE;
  tp_svc_call_stream_interface_media_emit_server_info_retrieved (self);
}

/**
 * tp_base_media_call_stream_set_stun_servers:
 * @self: a #TpBaseMediaCallStream
 * @stun_servers: the new stun servers
 *
 * Set the STUN servers. The #GPtrArray should have a free_func defined such as
 * g_ptr_array_ref() is enough to keep the data and g_ptr_array_unref() is
 * enough to release it later.
 *
 * Note that this replaces the previously set STUN servers, it is not an
 * addition.
 *
 * Since: 0.UNRELEASED
 */
void
tp_base_media_call_stream_set_stun_servers (TpBaseMediaCallStream *self,
    GPtrArray *stun_servers)
{
  g_return_if_fail (TP_IS_BASE_MEDIA_CALL_STREAM (self));
  g_return_if_fail (stun_servers != NULL);

  tp_clear_pointer (&self->priv->stun_servers, g_ptr_array_unref);
  self->priv->stun_servers = g_ptr_array_ref (stun_servers);

  tp_svc_call_stream_interface_media_emit_stun_servers_changed (self,
      self->priv->stun_servers);

  maybe_got_server_info (self);
}

/**
 * tp_base_media_call_stream_set_relay_info:
 * @self: a #TpBaseMediaCallStream
 * @relays: the new relays info
 *
 * Set the relays info. The #GPtrArray should have a free_func defined such as
 * g_ptr_array_ref() is enough to keep the data and g_ptr_array_unref() is
 * enough to release it later.
 *
 * Note that this replaces the previously set relays, it is not an addition.
 *
 * Since: 0.UNRELEASED
 */
void
tp_base_media_call_stream_set_relay_info (TpBaseMediaCallStream *self,
    GPtrArray *relays)
{
  g_return_if_fail (TP_IS_BASE_MEDIA_CALL_STREAM (self));
  g_return_if_fail (relays != NULL);

  tp_clear_pointer (&self->priv->relay_info, g_ptr_array_unref);
  self->priv->relay_info = g_ptr_array_ref (relays);

  tp_svc_call_stream_interface_media_emit_relay_info_changed (self,
      self->priv->relay_info);

  maybe_got_server_info (self);
}

/**
 * tp_base_media_call_stream_add_endpoint:
 * @self: a #TpBaseMediaCallStream
 * @endpoint: a #TpCallStreamEndpoint
 *
 * Add @endpoint to #TpBaseMediaCallStream:endpoints list, and emits
 * EndpointsChanged DBus signal.
 *
 * Since: 0.UNRELEASED
 */
void
tp_base_media_call_stream_add_endpoint (TpBaseMediaCallStream *self,
    TpCallStreamEndpoint *endpoint)
{
  const gchar *object_path;
  GPtrArray *added;
  GPtrArray *removed;

  g_return_if_fail (TP_IS_BASE_MEDIA_CALL_STREAM (self));
  g_return_if_fail (TP_IS_CALL_STREAM_ENDPOINT (endpoint));

  _tp_call_stream_endpoint_set_stream (endpoint, self);

  object_path = tp_call_stream_endpoint_get_object_path (endpoint);
  DEBUG ("Add endpoint %s to stream %s", object_path,
      tp_base_call_stream_get_object_path ((TpBaseCallStream *) self));

  self->priv->endpoints = g_list_append (self->priv->endpoints,
      g_object_ref (endpoint));

  added = g_ptr_array_new ();
  removed = g_ptr_array_new ();
  g_ptr_array_add (added, (gpointer) object_path);

  tp_svc_call_stream_interface_media_emit_endpoints_changed (self,
      added, removed);

  g_ptr_array_unref (added);
  g_ptr_array_unref (removed);
}

/**
 * tp_base_media_call_stream_get_endpoints:
 * @self: a #TpBaseMediaCallStream
 *
 * Same as #TpBaseMediaCallStream:endpoints but as a #GList of
 * #TpCallStreamEndpoint.
 *
 * Returns: Borrowed #GList of #TpCallStreamEndpoint.
 * Since: 0.UNRELEASED
 */
GList *
tp_base_media_call_stream_get_endpoints (TpBaseMediaCallStream *self)
{
  g_return_val_if_fail (TP_IS_BASE_MEDIA_CALL_STREAM (self), NULL);

  return self->priv->endpoints;
}

/**
 * tp_base_media_call_stream_set_sending_state:
 * @self: a #TpBaseMediaCallStream
 * @state: a #TpStreamFlowState
 *
 * Request a change in the sending state. Only PENDING values are accepted,
 * state will change to the corresponding non-pending value once the stream
 * state effectively changed.
 *
 * Since: 0.UNRELEASED
 */
void
tp_base_media_call_stream_set_sending_state (TpBaseMediaCallStream *self,
    TpStreamFlowState state)
{
  g_return_if_fail (TP_IS_BASE_MEDIA_CALL_STREAM (self));
  g_return_if_fail (state == TP_STREAM_FLOW_STATE_PENDING_START ||
      state == TP_STREAM_FLOW_STATE_PENDING_STOP ||
      state == TP_STREAM_FLOW_STATE_PENDING_PAUSE);

  if (self->priv->sending_state == state)
    return;

  self->priv->sending_state = state;

  tp_svc_call_stream_interface_media_emit_sending_state_changed (self, state);
}

/**
 * tp_base_media_call_stream_set_receiving_state:
 * @self: a #TpBaseMediaCallStream
 * @state: a #TpStreamFlowState
 *
 * Request a change in the receiving state. Only PENDING values are accepted,
 * state will change to the corresponding non-pending value once the stream
 * state effectively changed.
 *
 * Since: 0.UNRELEASED
 */
void
tp_base_media_call_stream_set_receiving_state (TpBaseMediaCallStream *self,
    TpStreamFlowState state)
{
  g_return_if_fail (TP_IS_BASE_MEDIA_CALL_STREAM (self));
  g_return_if_fail (state == TP_STREAM_FLOW_STATE_PENDING_START ||
      state == TP_STREAM_FLOW_STATE_PENDING_STOP ||
      state == TP_STREAM_FLOW_STATE_PENDING_PAUSE);

  if (self->priv->receiving_state == state)
    return;

  self->priv->receiving_state = state;
  g_object_notify (G_OBJECT (self), "receiving-state");

  tp_svc_call_stream_interface_media_emit_receiving_state_changed (self, state);
}

static guint
find_handle_in_array (GArray *array, TpHandle handle)
{
  guint i;

  for (i = 0; i < array->len; i++)
    if (g_array_index (array, TpHandle, i) == handle)
      return i;

  return G_MAXUINT;
}

static gboolean
tp_base_media_call_stream_set_sending (TpBaseCallStream *bcs,
    gboolean sending, GError **error)
{

  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (bcs);
  TpBaseMediaCallStreamClass *klass =
      TP_BASE_MEDIA_CALL_STREAM_GET_CLASS (self);

  if (sending)
    {
      if (klass->set_sending != NULL &&
          !klass->set_sending (bcs, sending, error))
        return FALSE;

      if (self->priv->sending_state != TP_STREAM_FLOW_STATE_PENDING_START &&
          self->priv->sending_state != TP_STREAM_FLOW_STATE_STARTED)
        tp_base_media_call_stream_set_sending_state (self,
            TP_STREAM_FLOW_STATE_PENDING_START);
    }
  else
   {
      if (self->priv->sending_state == TP_STREAM_FLOW_STATE_STOPPED)
        {
          if (klass->set_sending != NULL)
            return klass->set_sending (bcs, sending, error);
        }

      /* Already waiting for the streaming implementation to stop sending */
      if (self->priv->sending_state == TP_STREAM_FLOW_STATE_PENDING_STOP)
        return TRUE;

      tp_base_media_call_stream_set_sending_state (self,
          TP_STREAM_FLOW_STATE_PENDING_STOP);
    }

  return TRUE;
}

TpStreamFlowState
tp_base_media_call_stream_get_sending_state (TpBaseMediaCallStream *self)
{
  g_return_val_if_fail (TP_IS_BASE_MEDIA_CALL_STREAM (self),
      TP_STREAM_FLOW_STATE_STOPPED);

  return self->priv->sending_state;
}

TpStreamFlowState
tp_base_media_call_stream_get_receiving_state (TpBaseMediaCallStream *self)
{
  g_return_val_if_fail (TP_IS_BASE_MEDIA_CALL_STREAM (self),
      TP_STREAM_FLOW_STATE_STOPPED);

  return self->priv->receiving_state;
}

void
_tp_base_media_call_stream_start_receiving (TpBaseMediaCallStream *self,
    guint contact)
{

  if (find_handle_in_array (self->priv->receiving_requests, contact) !=
      G_MAXUINT)
    g_array_append_val (self->priv->receiving_requests, contact);

  /* Already waiting for the streaming implementation to start receiving */
  if (self->priv->receiving_state != TP_STREAM_FLOW_STATE_PENDING_START)
    tp_base_media_call_stream_set_receiving_state (self,
        TP_STREAM_FLOW_STATE_PENDING_START);
}

static gboolean
tp_base_media_call_stream_request_receiving (TpBaseCallStream *bcs,
    TpHandle contact, gboolean receive, GError **error)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (bcs);
  TpBaseMediaCallStreamClass *klass =
      TP_BASE_MEDIA_CALL_STREAM_GET_CLASS (self);
  TpBaseCallChannel *channel = _tp_base_call_stream_get_channel (bcs);


  if (receive)
    {
      tp_base_call_stream_update_remote_sending_state (bcs, contact,
          TP_SENDING_STATE_PENDING_SEND,
          tp_base_channel_get_self_handle (TP_BASE_CHANNEL (channel)),
          TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "",
          "User asked the remote side to start sending");

      if (self->priv->receiving_state == TP_STREAM_FLOW_STATE_STARTED)
        {
          if (klass->request_receiving != NULL)
            {
              klass->request_receiving (self, contact, receive);
              return TRUE;
            }
        }

      _tp_base_media_call_stream_start_receiving (self, contact);
    }
  else
    {
      guint i;

      tp_base_call_stream_update_remote_sending_state (bcs, contact,
          TP_SENDING_STATE_PENDING_STOP_SENDING,
          tp_base_channel_get_self_handle (TP_BASE_CHANNEL (channel)),
          TP_CALL_STATE_CHANGE_REASON_USER_REQUESTED, "",
          "User asked the remote side to stop sending");

      i = find_handle_in_array (self->priv->receiving_requests, contact);
      if (i != G_MAXUINT)
        g_array_remove_index_fast (self->priv->receiving_requests, i);

      if (klass->request_receiving != NULL)
        klass->request_receiving (self, contact, receive);

      if (self->priv->receiving_state != TP_STREAM_FLOW_STATE_PENDING_STOP &&
          self->priv->receiving_state != TP_STREAM_FLOW_STATE_STOPPED)
        tp_base_media_call_stream_set_receiving_state (self,
            TP_STREAM_FLOW_STATE_PENDING_STOP);
    }

  return TRUE;
}

static gboolean
correct_state_transition (TpStreamFlowState old_state,
    TpStreamFlowState new_state)
{
  switch (new_state)
    {
      case TP_STREAM_FLOW_STATE_STARTED:
        return (old_state == TP_STREAM_FLOW_STATE_PENDING_START);
      case TP_STREAM_FLOW_STATE_STOPPED:
        return (old_state == TP_STREAM_FLOW_STATE_PENDING_STOP);
      case TP_STREAM_FLOW_STATE_PAUSED:
        return (old_state == TP_STREAM_FLOW_STATE_PENDING_PAUSE);
      default:
        return FALSE;
    }
}

static void
tp_base_media_call_stream_complete_sending_state_change (
    TpSvcCallStreamInterfaceMedia *iface,
    TpStreamFlowState state,
    DBusGMethodInvocation *context)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (iface);
  TpBaseMediaCallStreamClass *klass =
      TP_BASE_MEDIA_CALL_STREAM_GET_CLASS (self);

  if (!correct_state_transition (self->priv->sending_state, state))
    {
      GError e = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Invalid sending state transition" };
      dbus_g_method_return_error (context, &e);
      return;
    }

  self->priv->sending_state = state;

  if (state == TP_STREAM_FLOW_STATE_STOPPED)
    {
      if (klass->set_sending != NULL)
        klass->set_sending (TP_BASE_CALL_STREAM (self), FALSE, NULL);
    }

  tp_svc_call_stream_interface_media_emit_sending_state_changed (self, state);
  tp_svc_call_stream_interface_media_return_from_complete_sending_state_change
      (context);
}

static void
tp_base_media_call_stream_report_sending_failure (
    TpSvcCallStreamInterfaceMedia *iface,
    TpCallStateChangeReason reason,
    const gchar *dbus_reason,
    const gchar *message,
    DBusGMethodInvocation *context)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (iface);
  TpBaseMediaCallStreamClass *klass =
      TP_BASE_MEDIA_CALL_STREAM_GET_CLASS (self);
  TpStreamFlowState old_state = self->priv->sending_state;

  switch (self->priv->sending_state)
    {
    case TP_STREAM_FLOW_STATE_PENDING_START:
      self->priv->sending_state = TP_STREAM_FLOW_STATE_STOPPED;
      break;
    case TP_STREAM_FLOW_STATE_PENDING_STOP:
      self->priv->sending_state = TP_STREAM_FLOW_STATE_STARTED;
      break;
    default:
      {
        GError e = {TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                    "The Sending state was not in a pending state"};
        dbus_g_method_return_error (context, &e);
        return;
      }
    }

  if (klass->report_sending_failure != NULL)
    klass->report_sending_failure (self, old_state, reason, dbus_reason,
        message);

  tp_svc_call_stream_interface_media_return_from_report_sending_failure (
      context);
}

static void
tp_base_media_call_stream_complete_receiving_state_change (
    TpSvcCallStreamInterfaceMedia *iface,
    TpStreamFlowState state,
    DBusGMethodInvocation *context)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (iface);
  TpBaseMediaCallStreamClass *klass =
      TP_BASE_MEDIA_CALL_STREAM_GET_CLASS (self);

  if (!correct_state_transition (self->priv->receiving_state, state))
    {
      GError e = { TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "Invalid receiving state transition" };
      dbus_g_method_return_error (context, &e);
      return;
    }

  self->priv->receiving_state = state;
  g_object_notify (G_OBJECT (self), "receiving-state");

  if (state == TP_STREAM_FLOW_STATE_STARTED)
    {
      guint i;

      for (i = 0; i < self->priv->receiving_requests->len; i++)
        {
          TpHandle contact = g_array_index (self->priv->receiving_requests,
              TpHandle, i);

          if (klass->request_receiving != NULL)
            klass->request_receiving (self, contact, TRUE);
        }
      if (self->priv->receiving_requests->len > 0)
        g_array_remove_range (self->priv->receiving_requests, 0,
            self->priv->receiving_requests->len);
    }
  else if (state == TP_STREAM_FLOW_STATE_STOPPED)
    {
      /* FIXME: Update the Stream RemoteMembers state if possible */
    }

  tp_svc_call_stream_interface_media_emit_receiving_state_changed (self, state);
  tp_svc_call_stream_interface_media_return_from_complete_receiving_state_change
      (context);
}

static void
tp_base_media_call_stream_report_receiving_failure (
    TpSvcCallStreamInterfaceMedia *iface,
    TpCallStateChangeReason reason,
    const gchar *dbus_reason,
    const gchar *message,
    DBusGMethodInvocation *context)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (iface);
  TpBaseMediaCallStreamClass *klass =
      TP_BASE_MEDIA_CALL_STREAM_GET_CLASS (self);
  TpStreamFlowState old_state = self->priv->receiving_state;

  switch (self->priv->receiving_state)
    {
    case TP_STREAM_FLOW_STATE_PENDING_START:
      /* Clear all receving requests, we can't receive */
      if (self->priv->receiving_requests->len > 0)
        g_array_remove_range (self->priv->receiving_requests, 0,
            self->priv->receiving_requests->len);
      self->priv->receiving_state = TP_STREAM_FLOW_STATE_STOPPED;
      g_object_notify (G_OBJECT (self), "receiving-state");
      break;
    case TP_STREAM_FLOW_STATE_PENDING_STOP:
      self->priv->receiving_state = TP_STREAM_FLOW_STATE_STARTED;
      g_object_notify (G_OBJECT (self), "receiving-state");
      break;
    default:
      {
        GError e = {TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
                    "The Receiving state was not in a pending state"};
        dbus_g_method_return_error (context, &e);
        return;
      }
    }

  if (klass->report_receiving_failure != NULL)
    klass->report_receiving_failure (self, old_state,
        reason, dbus_reason, message);

  tp_svc_call_stream_interface_media_return_from_report_receiving_failure (
      context);
}

static void
tp_base_media_call_stream_set_credentials (TpSvcCallStreamInterfaceMedia *iface,
    const gchar *username,
    const gchar *password,
    DBusGMethodInvocation *context)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (iface);

  g_free (self->priv->username);
  g_free (self->priv->password);
  self->priv->username = g_strdup (username);
  self->priv->password = g_strdup (password);

  tp_clear_pointer (&self->priv->local_candidates, g_ptr_array_unref);
  self->priv->local_candidates = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_value_array_free);

  g_object_notify (G_OBJECT (self), "local-candidates");
  g_object_notify (G_OBJECT (self), "local-credentials");

  tp_svc_call_stream_interface_media_emit_local_credentials_changed (self,
      username, password);

  tp_svc_call_stream_interface_media_return_from_set_credentials (context);
}

static void
tp_base_media_call_stream_add_candidates (TpSvcCallStreamInterfaceMedia *iface,
    const GPtrArray *candidates,
    DBusGMethodInvocation *context)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (iface);
  TpBaseMediaCallStreamClass *klass =
      TP_BASE_MEDIA_CALL_STREAM_GET_CLASS (self);
  GPtrArray *accepted_candidates = NULL;
  guint i;
  GError *error = NULL;

  if (klass->add_local_candidates == NULL)
    {
      GError e = { TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
          "Connection Manager did not implement "
          "TpBaseMediaCallStream::add_local_candidates vmethod" };
      dbus_g_method_return_error (context, &e);
      return;
    }

  DEBUG ("Adding %d candidates to stream %s", candidates->len,
      tp_base_call_stream_get_object_path ((TpBaseCallStream *) self));

  accepted_candidates = klass->add_local_candidates (self, candidates, &error);
  if (accepted_candidates == NULL)
    {
      dbus_g_method_return_error (context, error);
      g_clear_error (&error);
      return;
    }

  for (i = 0; i < accepted_candidates->len; i++)
    {
      GValueArray *c = g_ptr_array_index (accepted_candidates, i);

      g_ptr_array_add (self->priv->local_candidates,
          g_value_array_copy (c));
    }

  tp_svc_call_stream_interface_media_emit_local_candidates_added (self,
      accepted_candidates);
  tp_svc_call_stream_interface_media_return_from_add_candidates (context);

  g_ptr_array_unref (accepted_candidates);
}

static void
tp_base_media_call_stream_finish_initial_candidates (
    TpSvcCallStreamInterfaceMedia *iface,
    DBusGMethodInvocation *context)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (iface);
  TpBaseMediaCallStreamClass *klass =
      TP_BASE_MEDIA_CALL_STREAM_GET_CLASS (self);

  if (klass->finish_initial_candidates != NULL)
    klass->finish_initial_candidates (self);

  tp_svc_call_stream_interface_media_return_from_finish_initial_candidates (
      context);
}

static void
tp_base_media_call_stream_fail (TpSvcCallStreamInterfaceMedia *iface,
    const GValueArray *reason_array,
    DBusGMethodInvocation *context)
{
  TpBaseMediaCallStream *self = TP_BASE_MEDIA_CALL_STREAM (iface);
  TpBaseCallStream *base = TP_BASE_CALL_STREAM (self);
  TpBaseCallChannel *channel;
  TpBaseCallContent *content;

  channel = _tp_base_call_stream_get_channel (base);
  content = _tp_base_call_stream_get_content (base);

  _tp_base_call_content_remove_stream_internal (content, base, reason_array);

  /* If it was the last stream, remove the content */
  if (tp_base_call_content_get_streams (content) == NULL)
    {
      _tp_base_call_channel_remove_content_internal (channel, content,
          reason_array);
    }

  tp_svc_call_stream_interface_media_return_from_fail (context);
}

static void
call_stream_media_iface_init (gpointer g_iface, gpointer iface_data)
{
  TpSvcCallStreamInterfaceMediaClass *klass =
      (TpSvcCallStreamInterfaceMediaClass *) g_iface;

#define IMPLEMENT(x) tp_svc_call_stream_interface_media_implement_##x (\
    klass, tp_base_media_call_stream_##x)
  IMPLEMENT(complete_sending_state_change);
  IMPLEMENT(report_sending_failure);
  IMPLEMENT(complete_receiving_state_change);
  IMPLEMENT(report_receiving_failure);
  IMPLEMENT(set_credentials);
  IMPLEMENT(add_candidates);
  IMPLEMENT(finish_initial_candidates);
  IMPLEMENT(fail);
#undef IMPLEMENT
}
