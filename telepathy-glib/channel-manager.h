/*
 * channel-manager.h - factory and manager for channels relating to a
 *  particular protocol feature
 *
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008 Nokia Corporation
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

#ifndef TP_CHANNEL_MANAGER_H
#define TP_CHANNEL_MANAGER_H

#include <glib-object.h>

#include <telepathy-glib/exportable-channel.h>

G_BEGIN_DECLS

#define TP_TYPE_CHANNEL_MANAGER (tp_channel_manager_get_type ())

#define TP_CHANNEL_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TP_TYPE_CHANNEL_MANAGER, TpChannelManager))

#define TP_IS_CHANNEL_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_TYPE_CHANNEL_MANAGER))

#define TP_CHANNEL_MANAGER_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
  TP_TYPE_CHANNEL_MANAGER, TpChannelManagerIface))

/**
 * TpChannelManager:
 *
 * Opaque typedef representing any channel manager implementation.
 */
typedef struct _TpChannelManager TpChannelManager;

/* documented below */
typedef struct _TpChannelManagerIface TpChannelManagerIface;


/* virtual methods */

/**
 * TpChannelManagerForeachChannelFunc:
 * @manager: an object implementing #TpChannelManager
 * @func: A function
 * @user_data: Arbitrary data to be passed as the second argument of @func
 *
 * Signature of an implementation of foreach_channel, which must call
 * func(channel, user_data) for each channel managed by this channel manager.
 */
typedef void (*TpChannelManagerForeachChannelFunc) (
    TpChannelManager *manager, TpExportableChannelFunc func,
    gpointer user_data);

void tp_channel_manager_foreach_channel (TpChannelManager *manager,
    TpExportableChannelFunc func, gpointer user_data);


/**
 * TpChannelManagerChannelClassFunc:
 * @manager: An object implementing #TpChannelManager
 * @fixed_properties: A table mapping (const gchar *) property names to
 *  GValues, representing the values those properties must take to request
 *  channels of a particular class.
 * @allowed_properties: A %NULL-terminated array of property names which may
 *  appear in requests for a particular channel class.
 * @user_data: Arbitrary user-supplied data.
 *
 * Signature of callbacks which act on each channel class supported by @manager.
 */
typedef void (*TpChannelManagerChannelClassFunc) (
    TpChannelManager *manager,
    GHashTable *fixed_properties,
    const gchar * const *allowed_properties,
    gpointer user_data);

/**
 * TpChannelManagerForeachChannelClassFunc:
 * @manager: An object implementing #TpChannelManager
 * @func: A function
 * @user_data: Arbitrary data to be passed as the final argument of @func
 *
 * Signature of an implementation of foreach_channel_class, which must call
 * func(manager, fixed, allowed, user_data) for each channel class understood
 * by @manager.
 */
typedef void (*TpChannelManagerForeachChannelClassFunc) (
    TpChannelManager *manager, TpChannelManagerChannelClassFunc func,
    gpointer user_data);

void tp_channel_manager_foreach_channel_class (
    TpChannelManager *manager,
    TpChannelManagerChannelClassFunc func, gpointer user_data);


/**
 * TpChannelManagerRequestFunc:
 * @manager: An object implementing #TpChannelManager
 * @request_token: An opaque pointer representing this pending request.
 * @request_properties: A table mapping (const gchar *) property names to
 *  GValue, representing the desired properties of a channel requested by a
 *  Telepathy client.
 *
 * Signature of an implementation of #TpChannelManagerIface::create_channel and
 * #TpChannelManagerIface::request_channel.
 *
 * Implementations should inspect the contents of @request_properties to see if
 * it matches a channel class handled by this manager.  If so, they should
 * return %TRUE to accept responsibility for the request, and ultimately emit
 * exactly one of the #TpChannelManagerIface::new-channels,
 * #TpChannelManagerIface::already-satisfied and
 * #TpChannelManagerIface::request-failed signals (including @request_token in
 * the appropriate argument).
 *
 * If the implementation does not want to handle the request, it should return
 * %FALSE to allow the request to be offered to another channel manager.
 *
 * Returns: %TRUE if @manager will handle this request, else %FALSE.
 */
typedef gboolean (*TpChannelManagerRequestFunc) (
    TpChannelManager *manager, gpointer request_token,
    GHashTable *request_properties);

gboolean tp_channel_manager_create_channel (TpChannelManager *manager,
    gpointer request_token, GHashTable *request_properties);

gboolean tp_channel_manager_request_channel (TpChannelManager *manager,
    gpointer request_token, GHashTable *request_properties);


/**
 * TpChannelManagerIface:
 * @parent: Fields shared with GTypeInterface.
 * @foreach_channel: Call func(channel, user_data) for each channel managed by
 *  this manager. If not implemented, the manager is assumed to manage no
 *  channels.
 * @foreach_channel_class: Call func(manager, fixed, allowed, user_data) for
 *  each class of channel that this manager can create. If not implemented, the
 *  manager is assumed to be able to create no classes of channels.
 * @create_channel: Respond to a request for a new channel made with the
 *  Connection.Interface.Requests.CreateChannel method. See
 *  #TpChannelManagerRequestFunc for details.
 * @request_channel: Respond to a request for a (new or existing) channel made
 *  with the Connection.RequestChannel method. See #TpChannelManagerRequestFunc
 *  for details.
 *
 * The vtable for a channel manager implementation.
 *
 * In addition to the fields documented here there are thirteen GCallback
 * fields which must currently be %NULL.
 */
struct _TpChannelManagerIface {
    GTypeInterface parent;

    TpChannelManagerForeachChannelFunc foreach_channel;

    TpChannelManagerForeachChannelClassFunc foreach_channel_class;

    TpChannelManagerRequestFunc create_channel;
    TpChannelManagerRequestFunc request_channel;
    /* in principle we could have EnsureChannel here too */

    /*<private>*/
    /* extra spaces left for ensure_channel and two caps-related methods, which
     * will be added in the near future.
     */
    GCallback _near_future[3];

    GCallback _future[8];
};


GType tp_channel_manager_get_type (void);


/* signal emission */

void tp_channel_manager_emit_new_channel (gpointer instance,
    TpExportableChannel *channel, GSList *request_tokens);
void tp_channel_manager_emit_new_channels (gpointer instance,
    GHashTable *channels);

void tp_channel_manager_emit_channel_closed (gpointer instance,
    const gchar *path);
void tp_channel_manager_emit_channel_closed_for_object (gpointer instance,
    TpExportableChannel *channel);

void tp_channel_manager_emit_request_already_satisfied (
    gpointer instance, gpointer request_token,
    TpExportableChannel *channel);

void tp_channel_manager_emit_request_failed (gpointer instance,
    gpointer request_token, GQuark domain, gint code, const gchar *message);
void tp_channel_manager_emit_request_failed_printf (gpointer instance,
    gpointer request_token, GQuark domain, gint code, const gchar *format,
    ...) G_GNUC_PRINTF (5, 6);


/* helper functions */

gboolean tp_channel_manager_asv_has_unknown_properties (GHashTable *properties,
    const gchar * const *fixed, const gchar * const *allowed, GError **error);

G_END_DECLS

#endif
