/*
 * base-channel.c - base class for Channel implementations
 *
 * Copyright © 2009-2010 Collabora Ltd.
 * Copyright © 2009-2010 Nokia Corporation
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
 * SECTION:base-channel
 * @title: TpBaseChannel
 * @short_description: base class for all channel implementations
 * @see_also: #TpSvcChannel
 *
 * This base class makes it easier to write channels
 * implementations by implementing some of its properties, and defining other
 * relevant properties.
 *
 * Subclasses should fill in #TpBaseChannelClass.channel_type and
 * #TpBaseChannelClass.target_entity_type; and implement the
 * #TpBaseChannelClass.get_interfaces and
 * #TpBaseChannelClass.close virtual functions.
 *
 * If the channel type and/or interfaces being implemented define immutable
 * D-Bus properties besides those on the Channel interface, the subclass should
 * implement the #TpBaseChannelClass.fill_immutable_properties virtual function.
 *
 * If the #TpBaseChannel:object-path property is not set at construct
 * time, the #TpBaseChannelClass.get_object_path_suffix virtual function will
 * be called to determine the channel's path, whose default implementation
 * simply generates a unique path based on the object's address in memory.
 *
 * #TpBaseChannel also has the ability to remove the channel from the
 * bus, but keep the object around. To close the channel and remove it
 * from the bus, subclasses should call
 * tp_base_channel_disappear(). To bring the channel back, subclasses
 * use tp_base_channel_reopened_with_requested() and the channel
 * should be re-announced with
 * tp_channel_manager_emit_new_channel(). Note that channels which can
 * disappear but can also reopen due to pending messages need special
 * casing by the channel manager:
 *
 * |[
 * static void
 * channel_closed_cb (TpBaseChannel *chan,
 *     TpChannelManager *manager)
 * {
 *   MyChannelManager *self = MY_CHANNEL_MANAGER (manager);
 *   TpHandle handle = tp_base_channel_get_target_handle (chan);
 *
 *   // first, emit ChannelClosed if the channel is registered (it
 *   // won't be registered if it is appearing from being hidden, so
 *   // let's not emit the signal in this case)
 *   if (tp_base_channel_is_registered (chan))
 *     {
 *       tp_channel_manager_emit_channel_closed (manager,
 *           TP_BASE_CHANNEL (chan));
 *     }
 *
 *   if (tp_base_channel_is_destroyed (chan))
 *     {
 *       // destroyed() must have been called; forget this channel
 *       g_hash_table_remove (self->priv->channels, handle);
 *     }
 *   else if (tp_base_channel_is_respawning (chan))
 *     {
 *       // reopened_with_requested() must have been called; re-announce the channel
 *       tp_channel_manager_emit_new_channel (manager, TP_BASE_CHANNEL (chan));
 *     }
 *   else
 *     {
 *       // disappear() must have been called, do nothing special
 *     }
 * }
 * ]|
 *
 * and the #TpChannelManagerIface.foreach_channel virtual function
 * should be updated to only include registered channels:
 *
 * |[
 * static void
 * foreach_channel (TpChannelManager *manager,
 *     TpChannelManagerChannelClassFunc func,
 *     gpointer user_data)
 * {
 *   MyChannelManager *self = MY_CHANNEL_MANAGER (manager);
 *   GHashTableIter iter;
 *   gpointer chan;
 *
 *   g_hash_table_iter_init (&iter, self->priv->channels);
 *   while (g_hash_table_iter_next (&iter, NULL, &chan))
 *     {
 *       if (tp_base_channel_is_registered (TP_BASE_CHANNEL (chan)))
 *         func (TP_BASE_CHANNEL (chan), user_data);
 *     }
 * }
 * ]|
 *
 * Since: 0.11.14
 */

/**
 * TpBaseChannel:
 *
 * A base class for channel implementations
 *
 * Since: 0.11.14
 */

/**
 * TpBaseChannelClass:
 * @channel_type: The type of channel that instances of this class represent
 * (e.g. #TP_IFACE_CHANNEL_TYPE_TEXT)
 * @target_entity_type: The type of handle that is the target of channels of
 * this type
 * @close: A virtual function called to close the channel, which will be called
 *  by tp_base_channel_close() and by the implementation of the Closed D-Bus
 *  method.
 * @fill_immutable_properties: A virtual function called to add custom
 * properties to the DBus properties hash.  Implementations must chain up to the
 * parent class implementation and call
 * tp_dbus_properties_mixin_fill_properties_hash() on the supplied hash table
 * @get_object_path_suffix: Returns a string that will be appended to the
 * Connection objects's object path to get the Channel's object path.  This
 * function will only be called as a fallback if the
 * #TpBaseChannel:object-path property is not set.  The default
 * implementation simply generates a unique path based on the object's address
 * in memory.  The returned string will be freed automatically.
 * @get_interfaces: Extra interfaces provided by this channel (this SHOULD NOT
 *  include the channel type and interface itself). Implementation must first
 *  chainup on parent class implementation and then add extra interfaces into
 *  the #GPtrArray.
 *
 * The class structure for #TpBaseChannel
 *
 * Since: 0.11.14
 */

/**
 * TpBaseChannelCloseFunc:
 * @chan: a channel
 *
 * Signature of an implementation of the #TpBaseChannelClass.close virtual
 * function. Implementations should eventually call either
 * tp_base_channel_destroyed() if the channel is really closed as a result, or
 * tp_base_channel_reopened() if the channel will be re-spawned (for instance,
 * due to unacknowledged messages on a text channel), but need not do so before
 * returning. Note that channels that support re-spawning must also implement
 * #TpSvcChannelInterfaceDestroyable.
 *
 * Implementations may assume that tp_base_channel_is_destroyed() is FALSE for
 * @chan when called.  Note that if this function is implemented
 * asynchronously, it may be called more than once. A subclass which needs to
 * perform some asynchronous clean-up in order to close might implement this
 * function as follows:
 *
 * |[
 * static void
 * my_channel_close (TpBaseChannel *chan)
 * {
 *   MyChannel *self = MY_CHANNEL (chan);
 *
 *   if (self->priv->closing)
 *     return;
 *
 *   self->priv->closing = TRUE;
 *
 *   // some hypothetical channel-specific clean-up function:
 *   clean_up (self, cleaned_up_cb);
 * }
 *
 * static void
 * cleaned_up_cb (MyChannel *self)
 * {
 *   // all done, we can finish closing now
 *   tp_base_channel_destroyed (TP_BASE_CHANNEL (self));
 * }
 * static void
 * my_channel_class_init (MyChannelClass *klass)
 * {
 *   TpBaseChannelClass *base_channel_class = TP_BASE_CHANNEL_CLASS (klass);
 *
 *   klass->close = my_channel_close;
 *   // ...
 * }
 * ]|
 *
 * If a subclass does not need to do anything to clean itself up, it may
 * implement #TpBaseChannelClass.close using tp_base_channel_destroyed()
 * directly:
 *
 * |[
 * static void
 * my_channel_class_init (MyChannelClass *klass)
 * {
 *   TpBaseChannelClass *base_channel_class = TP_BASE_CHANNEL_CLASS (klass);
 *
 *   klass->close = tp_base_channel_destroyed;
 *   // ...
 * }
 * ]|
 *
 * Since: 0.11.14
 */

/**
 * TpBaseChannelFillPropertiesFunc:
 * @chan: a channel
 * @properties: a dictionary of @chan's immutable properties, which the
 *  implementation may add to using
 *  tp_dbus_properties_mixin_fill_properties_hash()
 *
 * Signature of an implementation of the
 * #TpBaseChannelClass.fill_immutable_properties
 * virtual function. A typical implementation, for a channel implementing
 * #TpSvcChannelTypeContactSearch, would be:
 *
 * |[
 * static void
 * my_search_channel_fill_properties (
 *     TpBaseChannel *chan,
 *     GHashTable *properties)
 * {
 *   TpBaseChannelClass *klass = TP_BASE_CHANNEL_CLASS (my_search_channel_parent_class);
 *
 *   klass->fill_immutable_properties (chan, properties);
 *
 *   tp_dbus_properties_mixin_fill_properties_hash (
 *       G_OBJECT (chan), properties,
 *       TP_IFACE_CHANNEL_TYPE_CONTACT_SEARCH, "Limit",
 *       TP_IFACE_CHANNEL_TYPE_CONTACT_SEARCH, "AvailableSearchKeys",
 *       TP_IFACE_CHANNEL_TYPE_CONTACT_SEARCH, "Server",
 *       NULL);
 * }
 * ]|
 *
 * Note that the SearchState property is <emphasis>not</emphasis> added to
 * @properties, since only immutable properties (whose value cannot change over
 * the lifetime of @chan) should be included.
 *
 * Since: 0.11.14
 */

/**
 * TpBaseChannelGetPathFunc:
 * @chan: a channel
 *
 * Signature of an implementation of the
 * #TpBaseChannelClass.get_object_path_suffix virtual function.
 *
 * Returns: (transfer full): a string that will be appended to the Connection
 * objects's object path to get the Channel's object path.
 *
 * Since: 0.11.14
 */

/**
 * TpBaseChannelGetInterfacesFunc:
 * @chan: a channel
 *
 * Signature of an implementation of #TpBaseChannelClass.get_interfaces virtual
 * function.
 *
 * Implementation must first chainup on parent class implementation and then
 * add extra interfaces into the #GPtrArray.
 *
 * |[
 * static GPtrArray *
 * my_channel_get_interfaces (TpBaseChannel *self)
 * {
 *   GPtrArray *interfaces;
 *
 *   interfaces = TP_BASE_CHANNEL_CLASS (my_channel_parent_class)->get_interfaces (self);
 *
 *   g_ptr_array_add (interfaces, TP_IFACE_BADGERS);
 *
 *   return interfaces;
 * }
 * ]|
 *
 * Returns: (transfer container): a #GPtrArray of static strings for D-Bus
 *   interfaces implemented by this client.
 *
 * Since: 0.17.5
 */

/**
 * TpBaseChannelFunc:
 * @channel: A #TpBaseChannel
 * @user_data: Arbitrary user-supplied data
 *
 * A callback for functions which act on base channels.
 */


#include "config.h"

#include "base-channel.h"

#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/asv.h>
#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include "telepathy-glib/group-mixin.h"
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/debug-internal.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL

#include "debug-internal.h"

enum
{
  PROP_OBJECT_PATH = 1,
  PROP_CHANNEL_TYPE,
  PROP_ENTITY_TYPE,
  PROP_HANDLE,
  PROP_INITIATOR_HANDLE,
  PROP_INITIATOR_ID,
  PROP_TARGET_ID,
  PROP_REQUESTED,
  PROP_CONNECTION,
  PROP_INTERFACES,
  PROP_CHANNEL_DESTROYED,
  PROP_CHANNEL_PROPERTIES,
  LAST_PROPERTY
};

struct _TpBaseChannelPrivate
{
  TpBaseConnection *conn;

  char *object_path;

  TpHandle target;
  TpHandle initiator;

  gboolean requested;
  gboolean destroyed;
  gboolean registered;
  gboolean respawning;

  gboolean dispose_has_run;
};

static void channel_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (TpBaseChannel, tp_base_channel,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL, channel_iface_init);
    )

/**
 * tp_base_channel_register:
 * @chan: a channel
 *
 * Make the channel appear on the bus.  #TpBaseChannel:object-path must have been set
 * to a valid path, which must not already be in use as another object's path.
 *
 * Since: 0.11.14
 */
void
tp_base_channel_register (TpBaseChannel *chan)
{
  GDBusConnection *bus = tp_base_connection_get_dbus_connection (
      chan->priv->conn);

  g_assert (chan->priv->object_path != NULL);
  g_return_if_fail (!chan->priv->registered);

  tp_dbus_connection_register_object (bus, chan->priv->object_path, chan);
  chan->priv->registered = TRUE;
}

/**
 * tp_base_channel_destroyed:
 * @chan: a channel
 *
 * Called by subclasses to indicate that this channel was destroyed and can be
 * removed from the bus.  The "Closed" signal will be emitted and the
 * #TpBaseChannel:channel-destroyed property will be set.
 *
 * Since: 0.11.14
 */
void
tp_base_channel_destroyed (TpBaseChannel *chan)
{
  GDBusConnection *bus = tp_base_connection_get_dbus_connection (
      chan->priv->conn);

  /* Take a ref to ourself: the 'closed' handler might drop its reference on us.
   */
  g_object_ref (chan);

  chan->priv->destroyed = TRUE;
  chan->priv->respawning = FALSE;
  tp_svc_channel_emit_closed (chan);

  if (chan->priv->registered)
    {
      tp_dbus_connection_unregister_object (bus, chan);
      chan->priv->registered = FALSE;
    }

  g_object_unref (chan);
}

/**
 * tp_base_channel_reopened:
 * @chan: a channel
 * @initiator: the handle of the contact that re-opened the channel
 *
 * Called by subclasses to indicate that this channel was closed but was
 * re-opened due to pending messages.
 *
 * Calling this method is the same as calling
 * tp_base_channel_reopened_with_requested() with a requested value of
 * %FALSE.
 *
 * Since: 0.11.14
 */
void
tp_base_channel_reopened (TpBaseChannel *chan, TpHandle initiator)
{
  tp_base_channel_reopened_with_requested (chan, FALSE, initiator);
}

/**
 * tp_base_channel_disappear:
 * @chan: a channel
 *
 * Called by subclasses to indicate that this channel is closing and
 * should be unregistered from the bus, but the actual object
 * shouldn't be destroyed. The "Closed" signal will be emitted,
 * the #TpBaseChannel:channel-destroyed property will not be
 * set, and the channel will be unregistered from the bus.
 *
 * Since: 0.19.7
 */
void
tp_base_channel_disappear (TpBaseChannel *chan)
{
  TpBaseChannelPrivate *priv = chan->priv;
  GDBusConnection *bus = tp_base_connection_get_dbus_connection (priv->conn);

  /* Take a ref to ourself: the 'closed' handler might drop its reference on us.
   */
  g_object_ref (chan);

  priv->destroyed = FALSE;
  priv->respawning = FALSE;

  tp_svc_channel_emit_closed (chan);

  if (priv->registered)
    {
      tp_dbus_connection_unregister_object (bus, chan);
      priv->registered = FALSE;
    }

  g_object_unref (chan);
}

/**
 * tp_base_channel_reopened_with_requested:
 * @chan: a channel
 * @requested: %TRUE if the channel is requested, otherwise %FALSE
 * @initiator: the handle of the contact that re-opened the channel
 *
 * Called by subclasses to indicate that this channel was closed but
 * was re-opened, either due to pending messages or from having
 * disappeared (with tp_base_channel_disappear()). The "Closed" signal
 * will be emitted, but the #TpBaseChannel:channel-destroyed
 * property will not be set.  The channel's
 * #TpBaseChannel:initiator-handle property will be set to @initiator,
 * and the #TpBaseChannel:requested property will be set to
 * @requested.
 *
 * Since: 0.19.7
 */
void
tp_base_channel_reopened_with_requested (TpBaseChannel *chan,
    gboolean requested,
    TpHandle initiator)
{
  TpBaseChannelPrivate *priv = chan->priv;

  /* Take a ref to ourself: the 'closed' handler might drop its reference on us.
   */
  g_object_ref (chan);

  if (priv->initiator != initiator)
    priv->initiator = initiator;

  priv->requested = requested;
  priv->respawning = TRUE;

  tp_svc_channel_emit_closed (chan);

  if (!priv->registered)
    tp_base_channel_register (chan);

  g_object_unref (chan);
}

/**
 * tp_base_channel_close:
 * @chan: a channel
 *
 * Asks @chan to close, just as if the Close D-Bus method had been called. If
 * #TpBaseChannel:channel-destroyed is TRUE, this is a no-op.
 *
 * Note that, depending on the subclass's implementation of
 * #TpBaseChannelClass.close and internal behaviour, this may or may not be a
 * suitable method to use during connection teardown. For instance, if the
 * channel may respawn when Close is called, an equivalent of the Destroy D-Bus
 * method would be more appropriate during teardown, since the intention is to
 * forcibly terminate all channels.
 *
 * Since: 0.11.14
 */
void
tp_base_channel_close (TpBaseChannel *chan)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (chan);

  g_return_if_fail (klass->close != NULL);

  if (!tp_base_channel_is_destroyed (chan))
    klass->close (chan);
}

/**
 * tp_base_channel_get_object_path:
 * @chan: a channel
 *
 * Returns @chan's object path, as a shortcut for retrieving the
 * #TpBaseChannel:object-path property.
 *
 * Returns: (transfer none): @chan's object path
 *
 * Since: 0.11.14
 */
const gchar *
tp_base_channel_get_object_path (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), NULL);

  return chan->priv->object_path;
}

/**
 * tp_base_channel_get_connection:
 * @chan: a channel
 *
 * Returns the connection to which @chan is attached, as a shortcut for
 * retrieving the #TpBaseChannel:connection property.
 *
 * Returns: (transfer none): the connection to which @chan is attached.
 *
 * Since: 0.11.14
 */
TpBaseConnection *
tp_base_channel_get_connection (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), NULL);

  return chan->priv->conn;
}

/**
 * tp_base_channel_get_self_handle:
 * @chan: a channel
 *
 * If @chan has a #TpGroupMixin, returns the value of group's self handle.
 * Otherwise return the value of #TpBaseConnection:self-handle.
 *
 * Returns: the self handle of @chan
 *
 * Since: 0.17.5
 */
TpHandle
tp_base_channel_get_self_handle (TpBaseChannel *chan)
{
  if (TP_HAS_GROUP_MIXIN (chan))
    {
      guint ret = 0;

      tp_group_mixin_get_self_handle (G_OBJECT (chan), &ret, NULL);
      if (ret != 0)
        return ret;
    }

  return tp_base_connection_get_self_handle (chan->priv->conn);
}

/**
 * tp_base_channel_get_target_handle:
 * @chan: a channel
 *
 * Returns the target handle of @chan (without a reference), which will be 0
 * if #TpBaseChannelClass.target_entity_type is #TP_ENTITY_TYPE_NONE for this
 * class, and non-zero otherwise. This is a shortcut for retrieving the
 * #TpBaseChannel:handle property.
 *
 * Returns: the target handle of @chan
 *
 * Since: 0.11.14
 */
TpHandle
tp_base_channel_get_target_handle (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), 0);

  return chan->priv->target;
}

/**
 * tp_base_channel_get_initiator:
 * @chan: a channel
 *
 * Returns the initiator handle of @chan, as a shortcut for retrieving the
 * #TpBaseChannel:initiator-handle property.
 *
 * Returns: the initiator handle of @chan
 *
 * Since: 0.11.14
 */
TpHandle
tp_base_channel_get_initiator (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), 0);

  return chan->priv->initiator;
}

/**
 * tp_base_channel_is_requested:
 * @chan: a channel
 *
 * Returns whether or not @chan was requested, as a shortcut for retrieving the
 * #TpBaseChannel:requested property.
 *
 * Returns: whether or not @chan was requested.
 *
 * Since: 0.11.14
 */
gboolean
tp_base_channel_is_requested (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), FALSE);

  return chan->priv->requested;
}

/**
 * tp_base_channel_is_registered:
 * @chan: a channel
 *
 * Returns whether or not @chan is visible on the bus; that is, whether
 * tp_base_channel_register() has been called and tp_base_channel_destroyed()
 * has not been called.
 *
 * Returns: TRUE if @chan is visible on the bus
 *
 * Since: 0.11.14
 */
gboolean
tp_base_channel_is_registered (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), FALSE);

  return chan->priv->registered;
}

/**
 * tp_base_channel_is_destroyed:
 * @chan: a channel
 *
 * Returns the value of the #TpBaseChannel:channel-destroyed property,
 * which is TRUE if tp_base_channel_destroyed() has been called (and thus the
 * channel has been removed from the bus).
 *
 * Returns: TRUE if tp_base_channel_destroyed() has been called.
 *
 * Since: 0.11.14
 */
gboolean
tp_base_channel_is_destroyed (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), FALSE);

  return chan->priv->destroyed;
}

/**
 * tp_base_channel_is_respawning:
 * @chan: a channel
 *
 * Returns %TRUE if the channel has been reopened, either by a
 * subclass calling tp_base_channel_reopened() or
 * tp_base_channel_reopened_with_requested(). This is useful for
 * "closed" handlers to distinguish between channels really closing
 * and channels that have been reopened due to pending messages.
 *
 * Returns: %TRUE if tp_base_channel_reopened() or
 *   tp_base_channel_reopened_with_requested() have been called.
 *
 * Since: 0.19.7
 */
gboolean
tp_base_channel_is_respawning (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), FALSE);

  return chan->priv->respawning;
}

/*
 * tp_base_channel_fill_basic_immutable_properties:
 *
 * Specifies the immutable properties supported for this Channel object, by
 * using tp_dbus_properties_mixin_fill_properties_hash().
 */
static void
tp_base_channel_fill_basic_immutable_properties (TpBaseChannel *chan, GHashTable *properties)
{
  tp_dbus_properties_mixin_fill_properties_hash (G_OBJECT (chan),
      properties,
      TP_IFACE_CHANNEL, "ChannelType",
      TP_IFACE_CHANNEL, "TargetEntityType",
      TP_IFACE_CHANNEL, "TargetHandle",
      TP_IFACE_CHANNEL, "TargetID",
      TP_IFACE_CHANNEL, "InitiatorHandle",
      TP_IFACE_CHANNEL, "InitiatorID",
      TP_IFACE_CHANNEL, "Requested",
      TP_IFACE_CHANNEL, "Interfaces",
      NULL);
}

static gchar *
tp_base_channel_get_basic_object_path_suffix (TpBaseChannel *self)
{
  gchar *obj_path = g_strdup_printf ("channel%p", self);
  gchar *escaped = tp_escape_as_identifier (obj_path);

  g_free (obj_path);

  return escaped;
}

static GPtrArray *
tp_base_channel_get_basic_interfaces (TpBaseChannel *self)
{
  return g_ptr_array_new ();
}

static void
tp_base_channel_init (TpBaseChannel *self)
{
  TpBaseChannelPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CHANNEL, TpBaseChannelPrivate);

  self->priv = priv;

}

static void
tp_base_channel_constructed (GObject *object)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (object);
  GObjectClass *parent_class = tp_base_channel_parent_class;
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);
  TpBaseConnection *conn = chan->priv->conn;

  if (parent_class->constructed != NULL)
    parent_class->constructed (object);

  g_return_if_fail (conn != NULL);
  g_return_if_fail (TP_IS_BASE_CONNECTION (conn));

  if (chan->priv->object_path == NULL)
    {
      gchar *base_path = klass->get_object_path_suffix (chan);

      g_assert (base_path != NULL);
      g_assert (*base_path != '\0');

      chan->priv->object_path = g_strdup_printf ("%s/%s",
          tp_base_connection_get_object_path (conn), base_path);
      g_free (base_path);
    }
}

static void
tp_base_channel_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (chan);

  switch (property_id) {
    case PROP_OBJECT_PATH:
      g_value_set_string (value, chan->priv->object_path);
      break;
    case PROP_CHANNEL_TYPE:
      g_value_set_static_string (value, klass->channel_type);
      break;
    case PROP_ENTITY_TYPE:
      g_value_set_uint (value, klass->target_entity_type);
      break;
    case PROP_HANDLE:
      g_value_set_uint (value, chan->priv->target);
      break;
    case PROP_TARGET_ID:
      if (chan->priv->target != 0)
        {
          TpHandleRepoIface *repo = tp_base_connection_get_handles (
              chan->priv->conn, klass->target_entity_type);

          g_assert (klass->target_entity_type != TP_ENTITY_TYPE_NONE);
          g_assert (repo != NULL);
          g_value_set_string (value, tp_handle_inspect (repo, chan->priv->target));
        }
      else
        {
          g_value_set_static_string (value, "");
        }
      break;
    case PROP_INITIATOR_HANDLE:
      g_value_set_uint (value, chan->priv->initiator);
      break;
    case PROP_INITIATOR_ID:
      if (chan->priv->initiator != 0)
        {
          TpHandleRepoIface *repo = tp_base_connection_get_handles (
              chan->priv->conn, TP_ENTITY_TYPE_CONTACT);

          g_assert (repo != NULL);
          g_assert (chan->priv->initiator != 0);
          g_value_set_string (value, tp_handle_inspect (repo, chan->priv->initiator));
        }
      else
        {
          g_value_set_static_string (value, "");
        }
      break;
    case PROP_REQUESTED:
      g_value_set_boolean (value, (chan->priv->requested));
      break;
    case PROP_CONNECTION:
      g_value_set_object (value, chan->priv->conn);
      break;
    case PROP_INTERFACES:
      {
        GPtrArray *interfaces = klass->get_interfaces (chan);

        g_ptr_array_add (interfaces, NULL);
        g_value_set_boxed (value, interfaces->pdata);
        g_ptr_array_unref (interfaces);
        break;
      }
    case PROP_CHANNEL_DESTROYED:
      g_value_set_boolean (value, chan->priv->destroyed);
      break;
    case PROP_CHANNEL_PROPERTIES:
        {
          /* create an empty properties hash for subclasses to fill */
          GHashTable *properties =
            tp_dbus_properties_mixin_make_properties_hash (G_OBJECT (chan), NULL, NULL, NULL);

          if (klass->fill_immutable_properties)
            klass->fill_immutable_properties (chan, properties);

          g_value_set_variant (value, tp_asv_to_vardict (properties));
          g_hash_table_unref (properties);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_base_channel_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);

  switch (property_id) {
    case PROP_OBJECT_PATH:
      g_assert (chan->priv->object_path == NULL);
      chan->priv->object_path = g_value_dup_string (value);
      break;
    case PROP_HANDLE:
      /* we don't ref it here because we don't necessarily have access to the
       * contact repo yet - instead we ref it in constructed.
       */
      chan->priv->target = g_value_get_uint (value);
      break;
    case PROP_INITIATOR_HANDLE:
      /* similarly we can't ref this yet */
      chan->priv->initiator = g_value_get_uint (value);
      break;
    case PROP_ENTITY_TYPE:
    case PROP_CHANNEL_TYPE:
      /* these properties are writable in the interface, but not actually
       * meaningfully changeable on this channel, so we do nothing */
      break;
    case PROP_CONNECTION:
      chan->priv->conn = g_value_dup_object (value);
      break;
    case PROP_REQUESTED:
      chan->priv->requested = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_base_channel_dispose (GObject *object)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);
  TpBaseChannelPrivate *priv = chan->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (!priv->destroyed)
    {
      tp_base_channel_destroyed (chan);
    }

  tp_clear_object (&priv->conn);

  if (G_OBJECT_CLASS (tp_base_channel_parent_class)->dispose)
    G_OBJECT_CLASS (tp_base_channel_parent_class)->dispose (object);
}

static void
tp_base_channel_finalize (GObject *object)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);

  g_free (chan->priv->object_path);

  G_OBJECT_CLASS (tp_base_channel_parent_class)->finalize (object);
}

static void
tp_base_channel_class_init (TpBaseChannelClass *tp_base_channel_class)
{
  static TpDBusPropertiesMixinPropImpl channel_props[] = {
      { "TargetEntityType", "entity-type", NULL },
      { "TargetHandle", "handle", NULL },
      { "TargetID", "target-id", NULL },
      { "ChannelType", "channel-type", NULL },
      { "Interfaces", "interfaces", NULL },
      { "Requested", "requested", NULL },
      { "InitiatorHandle", "initiator-handle", NULL },
      { "InitiatorID", "initiator-id", NULL },
      { NULL }
  };
  GObjectClass *object_class = G_OBJECT_CLASS (tp_base_channel_class);
  GParamSpec *param_spec;

  g_type_class_add_private (tp_base_channel_class,
      sizeof (TpBaseChannelPrivate));

  object_class->constructed = tp_base_channel_constructed;

  object_class->get_property = tp_base_channel_get_property;
  object_class->set_property = tp_base_channel_set_property;

  object_class->dispose = tp_base_channel_dispose;
  object_class->finalize = tp_base_channel_finalize;

  /**
   * TpBaseChannel:object-path:
   *
   * The D-Bus object path used for this object on the bus. Read-only
   * except during construction.
   */
  param_spec = g_param_spec_string ("object-path", "D-Bus object path",
      "The D-Bus object path used for this object on the bus.", NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH, param_spec);

  /**
   * TpBaseChannel:channel-type:
   *
   * The D-Bus interface representing the type of this channel. Read-only
   * except during construction.
   *
   * In connection manager implementations, attempts to set this property
   * during construction will usually be ignored or treated as an
   * error.
   */
  param_spec = g_param_spec_string ("channel-type", "Telepathy channel type",
      "The D-Bus interface representing the type of this channel.",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_TYPE, param_spec);

  /**
   * TpBaseChannel:entity-type:
   *
   * The #TpEntityType of this channel's associated handle, or
   * %TP_ENTITY_TYPE_NONE (which is numerically 0) if no handle.
   *
   * In connection manager implementations, attempts to set this during
   * construction might be ignored.
   */
  param_spec = g_param_spec_uint ("entity-type", "Entity type",
      "The TpEntityType of this channel's associated handle.",
      0, G_MAXUINT32, TP_UNKNOWN_ENTITY_TYPE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ENTITY_TYPE, param_spec);

  /**
   * TpBaseChannel:handle:
   *
   * This channel's associated handle, or 0 if no handle or unknown.
   * Read-only except during construction.
   *
   * In connection manager implementations, attempts to set this during
   * construction might be ignored, depending on the channel type.
   */
  param_spec = g_param_spec_uint ("handle", "Handle",
      "The TpHandle representing the contact, group, etc. with which "
      "this channel communicates, whose type is given by the entity-type "
      "property.",
      0, G_MAXUINT32, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_HANDLE, param_spec);

  /**
   * TpBaseChannel:channel-properties:
   *
   * The D-Bus properties to be announced in the NewChannels signal
   * and in the Channels property, as a map from
   * interface.name.propertyname to variant.
   *
   * A channel's immutable properties are constant for its lifetime on the
   * bus, so this property should only change when the closed signal is
   * emitted (so that respawned channels can reappear on the bus with
   * different properties).  All of the D-Bus properties mentioned here
   * should be exposed through the D-Bus properties interface; additional
   * (possibly mutable) properties not included here may also be exposed
   * via the D-Bus properties interface.
   *
   * If the channel implementation uses
   * <link linkend="telepathy-glib-dbus-properties-mixin">TpDBusPropertiesMixin</link>,
   * this property can implemented using
   * tp_dbus_properties_mixin_make_properties_hash() as follows:
   *
   * <informalexample><programlisting>
   *  case PROP_CHANNEL_PROPERTIES:
   *    {
   *      GHashTable *hash = tp_dbus_properties_mixin_make_properties_hash (object,
   *          // The spec says these properties MUST be included:
   *          TP_IFACE_CHANNEL, "TargetHandle",
   *          TP_IFACE_CHANNEL, "TargetEntityType",
   *          TP_IFACE_CHANNEL, "ChannelType",
   *          TP_IFACE_CHANNEL, "TargetID",
   *          TP_IFACE_CHANNEL, "Requested",
   *          TP_IFACE_CHANNEL, "InitiatorHandle",
   *          TP_IFACE_CHANNEL, "InitiatorID",
   *          TP_IFACE_CHANNEL, "Interfaces",
   *          // Perhaps your channel has some other immutable properties:
   *          TP_IFACE_CHANNEL_INTERFACE_MESSAGES, "SupportedContentTypes",
   *          // etc.
   *          NULL));
   *
   *      g_value_set_variant (value, tp_asv_to_vardict (hash));
   *      g_hash_table_unref (hash);
   *    }
   *    break;
   * </programlisting></informalexample>
   */
  param_spec = g_param_spec_variant ("channel-properties",
      "Channel properties",
      "The channel's immutable properties",
      G_VARIANT_TYPE_VARDICT, NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_PROPERTIES,
      param_spec);

  /**
   * TpBaseChannel:channel-destroyed:
   *
   * If true, the closed signal on the Channel interface indicates that
   * the channel can go away.
   *
   * If false, the closed signal indicates to the channel manager that the
   * channel should appear to go away and be re-created, by emitting Closed
   * followed by NewChannel. (This is to support the "respawning" of  Text
   * channels which are closed with unacknowledged messages.)
   */
  param_spec = g_param_spec_boolean ("channel-destroyed",
      "Destroyed?",
      "If true, the channel has *really* closed, rather than just "
      "appearing to do so",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_DESTROYED,
      param_spec);

  param_spec = g_param_spec_object ("connection", "TpBaseConnection object",
      "Connection object that owns this channel.",
      TP_TYPE_BASE_CONNECTION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION, param_spec);

  param_spec = g_param_spec_boxed ("interfaces", "Extra D-Bus interfaces",
      "Additional Channel.Interface.* interfaces",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INTERFACES, param_spec);

  param_spec = g_param_spec_string ("target-id", "Target's identifier",
      "The string obtained by inspecting the target handle",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TARGET_ID, param_spec);

  param_spec = g_param_spec_boolean ("requested", "Requested?",
      "True if this channel was requested by the local user",
      FALSE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REQUESTED, param_spec);

  param_spec = g_param_spec_uint ("initiator-handle", "Initiator's handle",
      "The contact who initiated the channel",
      0, G_MAXUINT32, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INITIATOR_HANDLE,
      param_spec);

  param_spec = g_param_spec_string ("initiator-id", "Initiator's bare JID",
      "The string obtained by inspecting the initiator-handle",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INITIATOR_ID,
      param_spec);

  tp_dbus_properties_mixin_class_init (object_class, 0);
  tp_dbus_properties_mixin_implement_interface (object_class,
      TP_IFACE_QUARK_CHANNEL,
      tp_dbus_properties_mixin_getter_gobject_properties,
      NULL,
      channel_props);

  tp_base_channel_class->fill_immutable_properties =
      tp_base_channel_fill_basic_immutable_properties;
  tp_base_channel_class->get_object_path_suffix =
      tp_base_channel_get_basic_object_path_suffix;
  tp_base_channel_class->get_interfaces =
      tp_base_channel_get_basic_interfaces;
}

static void
tp_base_channel_close_dbus (
    TpSvcChannel *iface,
    GDBusMethodInvocation *context)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (iface);

  if (DEBUGGING)
    {
      const gchar *caller = g_dbus_method_invocation_get_sender (context);

      DEBUG ("called by %s", caller);
    }

  tp_base_channel_close (chan);
  tp_svc_channel_return_from_close (context);
}

static void
channel_iface_init (gpointer g_iface,
                    gpointer iface_data)
{
  TpSvcChannelClass *klass = (TpSvcChannelClass *) g_iface;

  tp_svc_channel_implement_close (klass, tp_base_channel_close_dbus);
}
