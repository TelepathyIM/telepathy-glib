/*
 * A filter matching certain channels.
 *
 * Copyright © 2010-2014 Collabora Ltd.
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

#include "config.h"
#include <telepathy-glib/channel-filter.h>
#include <telepathy-glib/channel-filter-internal.h>

#define DEBUG_FLAG TP_DEBUG_MISC
#include "debug-internal.h"

#include <dbus/dbus-glib.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/variant-util.h>

/**
 * SECTION:channel-filter
 * @title: TpChannelFilter
 * @short_description: a filter matching certain channels
 * @see_also: #TpBaseClient, #TpSimpleApprover, #TpSimpleHandler,
 *  #TpSimpleObserver
 *
 * Telepathy clients are notified about "interesting" #TpChannels
 * by the Channel Dispatcher. To do this efficiently, the clients have
 * lists of "channel filters", describing the channels that each client
 * considers to be "interesting".
 *
 * In telepathy-glib, these lists take the form of lists of #TpChannelFilter
 * objects. Each #TpChannelFilter matches certain properties of the channel,
 * and the channel dispatcher dispatches a channel to a client if that
 * channel matches any of the filters in the client's list:
 *
 * |[
 * channel is interesting to this client = (
 *     ((channel matches property A from filter 1) &&
 *      (channel matches property B from filter 1) && ...)
 *      ||
 *     ((channel matches property P from filter 2) &&
 *      (channel matches property Q from filter 2) && ...)
 *      || ...)
 * ]|
 *
 * An empty list of filters does not match any channels, but a list
 * containing an empty filter matches every channel.
 *
 * To construct a filter, either create an empty filter with
 * tp_channel_filter_new(), or create a pre-populated filter with
 * certain properties using one of the convenience constructors like
 * tp_channel_filter_new_for_text_chats().
 *
 * After creating a filter, you can make it more specific by using
 * methods like tp_channel_filter_require_locally_requested(), if
 * required.
 *
 * Finally, add it to a #TpBaseClient using
 * tp_base_client_add_observer_filter(),
 * tp_base_client_add_approver_filter() and/or
 * tp_base_client_add_handler_filter() (depending on the type
 * of client required), and release the filter object with g_object_unref().
 *
 * If you would like the #TpBaseClient to act on particular channels in
 * more than one role - for instance, an Approver for Text channels which
 * is also a Handler for Text channels - you can add the same
 * #TpChannelFilter object via more than one method.
 *
 * Once you have added a filter object to a #TpBaseClient, you may not
 * modify it further.
 */

/**
 * TpChannelFilterClass:
 *
 * The class of a #TpChannelFilter.
 */

struct _TpChannelFilterClass {
    /*<private>*/
    GObjectClass parent_class;
};

/**
 * TpChannelFilter:
 *
 * A filter matching certain channels.
 */

struct _TpChannelFilter {
    /*<private>*/
    GObject parent;
    TpChannelFilterPrivate *priv;
};

struct _TpChannelFilterPrivate {
  GVariantDict dict;

  gboolean already_used;
};

G_DEFINE_TYPE (TpChannelFilter, tp_channel_filter, G_TYPE_OBJECT)

static void
tp_channel_filter_init (TpChannelFilter *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CHANNEL_FILTER,
      TpChannelFilterPrivate);

  g_variant_dict_init (&self->priv->dict, NULL);

  self->priv->already_used = FALSE;
}

static void
tp_channel_filter_finalize (GObject *object)
{
  TpChannelFilter *self = TP_CHANNEL_FILTER (object);

  g_variant_dict_clear (&self->priv->dict);

  G_OBJECT_CLASS (tp_channel_filter_parent_class)->finalize (object);
}

static void
tp_channel_filter_class_init (TpChannelFilterClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);

  g_type_class_add_private (cls, sizeof (TpChannelFilterPrivate));

  object_class->finalize = tp_channel_filter_finalize;
}

/**
 * tp_channel_filter_new_for_all_types:
 *
 * Return a channel filter that matches every channel.
 *
 * You can make the filter more restrictive by setting properties.
 */
TpChannelFilter *
tp_channel_filter_new_for_all_types (void)
{
  return g_object_new (TP_TYPE_CHANNEL_FILTER,
      NULL);
}

/*
 * tp_channel_filter_require_channel_type:
 * @self: a channel filter
 * @channel_type: the desired value for #TpChannel:channel-type
 *
 * Narrow @self to require a particular channel type, given as a D-Bus
 * interface name.
 *
 * It is an error to call this method if the channel filter has already
 * been passed to a #TpBaseClient.
 */
static void
tp_channel_filter_require_channel_type (TpChannelFilter *self,
    const gchar *channel_type)
{
  g_return_if_fail (TP_IS_CHANNEL_FILTER (self));
  g_return_if_fail (g_dbus_is_interface_name (channel_type));

  g_variant_dict_insert (&self->priv->dict, TP_PROP_CHANNEL_CHANNEL_TYPE, "s",
      channel_type);
}

/**
 * tp_channel_filter_new_for_text_chats:
 *
 * Return a channel filter that matches 1-1 text chats,
 * such as #TpTextChannels carrying private messages or SMSs.
 *
 * It is not necessary to call tp_channel_filter_require_target_is_contact()
 * on the returned filter.
 */
TpChannelFilter *
tp_channel_filter_new_for_text_chats (void)
{
  TpChannelFilter *self = tp_channel_filter_new_for_all_types ();

  tp_channel_filter_require_target_is_contact (self);
  tp_channel_filter_require_channel_type (self, TP_IFACE_CHANNEL_TYPE_TEXT);
  return self;
}

/**
 * tp_channel_filter_new_for_text_chatrooms:
 *
 * Return a channel filter that matches participation in named text
 * chatrooms, such as #TpTextChannels communicating with
 * an XMPP Multi-User Chat room or an IRC channel.
 *
 * It is not necessary to call tp_channel_filter_require_target_is_room()
 * on the returned filter.
 */
TpChannelFilter *
tp_channel_filter_new_for_text_chatrooms (void)
{
  TpChannelFilter *self = tp_channel_filter_new_for_all_types ();

  tp_channel_filter_require_target_is_room (self);
  tp_channel_filter_require_channel_type (self, TP_IFACE_CHANNEL_TYPE_TEXT);
  return self;
}

/**
 * tp_channel_filter_require_target_is_contact:
 * @self: a channel filter
 *
 * Narrow @self to require that the channel communicates with a single
 * #TpContact.
 *
 * For instance, the filter would match #TpTextChannels carrying private
 * messages or SMSs, #TpCallChannels for ordinary 1-1 audio and/or video calls,
 * #TpFileTransferChannels for file transfers to or from a contact, and so on.
 *
 * It would not match channels communicating with a chatroom, ad-hoc
 * chatrooms with no name, or conference-calls (in protocols that can tell
 * the difference between a conference call and an ordinary 1-1 call).
 *
 * It is an error to call this method if the channel filter has already
 * been passed to a #TpBaseClient.
 */
void
tp_channel_filter_require_target_is_contact (TpChannelFilter *self)
{
  tp_channel_filter_require_target_type (self, TP_ENTITY_TYPE_CONTACT);
}

/**
 * tp_channel_filter_require_target_is_room:
 * @self: a channel filter
 *
 * Narrow @self to require that the channel communicates with a named
 * chatroom.
 *
 * For instance, this filter would match #TpTextChannels communicating with
 * an XMPP Multi-User Chat room or an IRC channel. It would also match
 * #TpDBusTubeChannels or #TpStreamTubeChannels that communicate through
 * a chatroom, and multi-user audio and/or video calls that use a named,
 * chatroom-like object on the server.
 *
 * It is an error to call this method if the channel filter has already
 * been passed to a #TpBaseClient.
 */
void
tp_channel_filter_require_target_is_room (TpChannelFilter *self)
{
  tp_channel_filter_require_target_type (self, TP_ENTITY_TYPE_ROOM);
}

/**
 * tp_channel_filter_require_no_target:
 * @self: a channel filter
 *
 * Narrow @self to require that the channel communicates with an
 * ad-hoc, unnamed group of contacts.
 *
 * For instance, among other things, this filter would match #TpCallChannels
 * for conference calls in cellular telephony.
 *
 * This is equivalent to tp_channel_filter_require_target_type()
 * with argument %TP_ENTITY_TYPE_NONE.
 *
 * It is an error to call this method if the channel filter has already
 * been passed to a #TpBaseClient.
 */
void
tp_channel_filter_require_no_target (TpChannelFilter *self)
{
  tp_channel_filter_require_target_type (self, TP_ENTITY_TYPE_NONE);
}

/**
 * tp_channel_filter_require_target_type:
 * @self: a channel filter
 * @entity_type: the desired value for #TpChannel:entity-type
 *
 * Narrow @self to require a particular target entity type.
 *
 * For instance, setting @entity_type to %TP_ENTITY_TYPE_CONTACT
 * is equivalent to tp_channel_filter_require_target_is_contact().
 *
 * It is an error to call this method if the channel filter has already
 * been passed to a #TpBaseClient.
 */
void
tp_channel_filter_require_target_type (TpChannelFilter *self,
    TpEntityType entity_type)
{
  g_return_if_fail (TP_IS_CHANNEL_FILTER (self));
  g_return_if_fail (((guint) entity_type) < TP_NUM_ENTITY_TYPES);

  g_variant_dict_insert (&self->priv->dict,
      TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, "u", entity_type);
}

/**
 * tp_channel_filter_new_for_calls:
 * @entity_type: the desired entity type
 *
 * Return a channel filter that matches audio and video calls,
 * including VoIP and telephony.
 *
 * @entity_type is passed to tp_channel_filter_require_target_type().
 * Use %TP_ENTITY_TYPE_CONTACT for ordinary 1-1 calls.
 */
TpChannelFilter *
tp_channel_filter_new_for_calls (TpEntityType entity_type)
{
  TpChannelFilter *self = tp_channel_filter_new_for_all_types ();

  tp_channel_filter_require_target_type (self, entity_type);
  tp_channel_filter_require_channel_type (self, TP_IFACE_CHANNEL_TYPE_CALL1);
  return self;
}

/**
 * tp_channel_filter_new_for_stream_tubes:
 * @service: (allow-none): the desired value of #TpStreamTubeChannel:service,
 *  or %NULL to match any service
 *
 * Return a channel filter that matches stream tube channels, as used by
 * #TpStreamTubeChannel, and optionally also match a particular service.
 * This filter can be narrowed further via other methods.
 *
 * For instance, to match RFB display-sharing being offered by another
 * participant in a chatroom:
 *
 * |[
 * filter = tp_channel_filter_new_for_stream_tubes ("rfb");
 * tp_channel_filter_require_target_is_room (filter);
 * tp_channel_filter_require_locally_requested (filter, FALSE);
 * ]|
 */
TpChannelFilter *
tp_channel_filter_new_for_stream_tubes (const gchar *service)
{
  TpChannelFilter *self = tp_channel_filter_new_for_all_types ();

  tp_channel_filter_require_channel_type (self,
      TP_IFACE_CHANNEL_TYPE_STREAM_TUBE1);

  if (service != NULL)
    g_variant_dict_insert (&self->priv->dict,
        TP_PROP_CHANNEL_TYPE_STREAM_TUBE1_SERVICE, "s", service);

  return self;
}

/**
 * tp_channel_filter_new_for_dbus_tubes:
 * @service: (allow-none): the desired value of
 *  #TpDBusTubeChannel:service-name, or %NULL to match any service
 *
 * Return a channel filter that matches D-Bus tube channels, as used by
 * #TpDBusTubeChannel, and optionally also match a particular service.
 * This filter can be narrowed further via other methods.
 *
 * For instance, to match a "com.example.Chess" tube being offered by
 * the local user to a peer:
 *
 * |[
 * filter = tp_channel_filter_new_for_dbus_tube ("com.example.Chess");
 * tp_channel_filter_require_target_is_contact (filter);
 * tp_channel_filter_require_locally_requested (filter, TRUE);
 * ]|
 */
TpChannelFilter *
tp_channel_filter_new_for_dbus_tubes (const gchar *service)
{
  TpChannelFilter *self = tp_channel_filter_new_for_all_types ();

  tp_channel_filter_require_channel_type (self,
      TP_IFACE_CHANNEL_TYPE_DBUS_TUBE1);

  if (service != NULL)
    g_variant_dict_insert (&self->priv->dict,
        TP_PROP_CHANNEL_TYPE_DBUS_TUBE1_SERVICE_NAME, "s", service);

  return self;
}

/**
 * tp_channel_filter_new_for_file_transfers:
 * @service: (allow-none): a service name, or %NULL
 *
 * Return a channel filter that matches file transfer channels with
 * a #TpContact, as used by #TpFileTransferChannel.
 *
 * At the time of writing, file transfers with other types of target
 * (like chatrooms) have not been implemented. If they are, they will
 * use a different filter.
 *
 * Using this method will match both incoming and outgoing file transfers.
 * If you only want to match one direction, use
 * tp_channel_filter_require_locally_requested() to select it.
 *
 * For instance, to match outgoing file transfers (sending a file to
 * a contact), you can use:
 *
 * |[
 * filter = tp_channel_filter_new_for_file_transfer (NULL);
 * tp_channel_filter_require_locally_requested (filter, TRUE);
 * ]|
 *
 * @service can be used by collaborative applications to match a particular
 * #TpFileTransferChannel:service-name. For instance, if an application
 * wants to be the handler for incoming file transfers that are marked
 * as belonging to that application, it could use a filter like this:
 *
 * |[
 * filter = tp_channel_filter_new_for_file_transfer ("com.example.MyApp");
 * tp_channel_filter_require_locally_requested (filter, FALSE);
 * tp_base_client_take_handler_filter (client, filter);
 * ]|
 */
TpChannelFilter *
tp_channel_filter_new_for_file_transfers (const gchar *service)
{
  TpChannelFilter *self = tp_channel_filter_new_for_all_types ();

  tp_channel_filter_require_target_is_contact (self);
  tp_channel_filter_require_channel_type (self,
      TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER1);

  if (service != NULL)
    g_variant_dict_insert (&self->priv->dict,
        TP_PROP_CHANNEL_INTERFACE_FILE_TRANSFER_METADATA1_SERVICE_NAME, "s",
        service);

  return self;
}

/**
 * tp_channel_filter_require_locally_requested:
 * @self: a channel filter
 * @requested: the desired value for tp_channel_get_requested()
 *
 * Narrow @self to require that the channel was requested by the local
 * user, or to require that the channel was <emphasis>not</emphasis>
 * requested by the local user, depending on the value of @requested.
 *
 * For instance, to match an outgoing (locally-requested) 1-1 call:
 *
 * |[
 * filter = tp_channel_filter_new_for_calls (TP_ENTITY_TYPE_CONTACT);
 * tp_channel_filter_require_locally_requested (filter, TRUE);
 * ]|
 *
 * or to match an incoming (not locally-requested) file transfer:
 *
 * |[
 * filter = tp_channel_filter_new_for_file_transfer ();
 * tp_channel_filter_require_locally_requested (filter, FALSE);
 * ]|
 *
 * It is an error to call this method if the channel filter has already
 * been passed to a #TpBaseClient.
 */
void
tp_channel_filter_require_locally_requested (TpChannelFilter *self,
    gboolean requested)
{
  g_return_if_fail (TP_IS_CHANNEL_FILTER (self));
  g_return_if_fail (!self->priv->already_used);

  /* Do not use tp_asv_set_uint32 or similar - the key is dup'd */
  g_variant_dict_insert (&self->priv->dict,
      TP_PROP_CHANNEL_REQUESTED, "b", requested);
}

/**
 * tp_channel_filter_require_property:
 * @self: a channel filter
 * @name: a fully-qualified D-Bus property name (in the format
 *  "interface.name.propertyname") as described by the Telepathy
 *  D-Bus API Specification
 * @value: the value required for @name
 *
 * Narrow @self to require that the immutable channel property @name
 * has the given value.
 *
 * If @value is a floating reference, this method will take ownership
 * of it.
 *
 * @value must not contain any GVariant extensions not supported by
 * dbus-glib, such as %G_VARIANT_TYPE_UNIT or %G_VARIANT_TYPE_HANDLE.
 *
 * For instance, tp_channel_filter_require_target_is_contact() is equivalent
 * to:
 *
 * |[
 * tp_channel_filter_require_property (filter,
 *     TP_PROP_CHANNEL_TARGET_HANDLE_TYPE,
 *     g_variant_new_uint32 (TP_ENTITY_TYPE_CONTACT));
 * ]|
 *
 * It is an error to call this method if the channel filter has already
 * been passed to a #TpBaseClient.
 */
void
tp_channel_filter_require_property (TpChannelFilter *self,
    const gchar *name,
    GVariant *value)
{
  g_return_if_fail (TP_IS_CHANNEL_FILTER (self));
  g_return_if_fail (!self->priv->already_used);
  g_return_if_fail (name != NULL);

  g_variant_dict_insert_value (&self->priv->dict, name, value);
}

GVariant *
_tp_channel_filter_use (TpChannelFilter *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL_FILTER (self), NULL);

  self->priv->already_used = TRUE;
  return g_variant_dict_end (&self->priv->dict);
  /* self->priv->dict is now invalid but self->priv->already_used prevents us
   * from trying to re-use it. */
}
