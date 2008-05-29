/*
 * channel.c - proxy for a Telepathy channel (Group interface)
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
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

#include "telepathy-glib/channel-internal.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_GROUPS
#include "telepathy-glib/debug-internal.h"


/**
 * TP_ERRORS_REMOVED_FROM_GROUP:
 *
 * #GError domain representing the local user being removed from a channel
 * with the Group interface. The @code in a #GError with this domain must
 * be a member of #TpChannelGroupChangeReason.
 *
 * This error may be raised on non-Group channels with certain reason codes
 * if there's no better error code to use (mainly
 * %TP_CHANNEL_GROUP_CHANGE_REASON_NONE).
 *
 * This macro expands to a function call returning a #GQuark.
 *
 * Since: 0.7.1
 */
GQuark
tp_errors_removed_from_group_quark (void)
{
  static GQuark q = 0;

  if (q == 0)
    q = g_quark_from_static_string ("tp_errors_removed_from_group_quark");

  return q;
}


static void
local_pending_info_free (LocalPendingInfo *info)
{
  g_free (info->message);
  g_slice_free (LocalPendingInfo, info);
}


/**
 * tp_channel_group_get_self_handle:
 * @self: a channel
 *
 * Return the #TpChannel:group-self-handle property (see the description
 * of that property for notes on validity).
 *
 * Returns: the handle representing the user, or 0
 */
TpHandle
tp_channel_group_get_self_handle (TpChannel *self)
{
  return self->group_self_handle;
}


/**
 * tp_channel_group_get_flags:
 * @self: a channel
 *
 * Return the #TpChannel:group-flags property (see the description
 * of that property for notes on validity).
 *
 * Returns: the group flags, or 0
 */
TpChannelGroupFlags
tp_channel_group_get_flags (TpChannel *self)
{
  return self->group_flags;
}


/**
 * tp_channel_group_get_members:
 * @self: a channel
 *
 * If @self is ready and is a group, return a #TpIntSet containing
 * its members.
 *
 * If @self is a group but is not ready, the result may either be a set
 * of members, or %NULL.
 *
 * If @self is not a group, return %NULL.
 *
 * Returns: the members, or %NULL
 */
const TpIntSet *
tp_channel_group_get_members (TpChannel *self)
{
  return self->priv->group_members;
}


/**
 * tp_channel_group_get_local_pending:
 * @self: a channel
 *
 * If @self is ready and is a group, return a #TpIntSet containing
 * its local-pending members.
 *
 * If @self is a group but is not ready, the result may either be a set
 * of local-pending members, or %NULL.
 *
 * If @self is not a group, return %NULL.
 *
 * Returns: the local-pending members, or %NULL
 */
const TpIntSet *
tp_channel_group_get_local_pending (TpChannel *self)
{
  return self->priv->group_local_pending;
}


/**
 * tp_channel_group_get_remote_pending:
 * @self: a channel
 *
 * If @self is ready and is a group, return a #TpIntSet containing
 * its remote-pending members.
 *
 * If @self is a group but is not ready, the result may either be a set
 * of remote-pending members, or %NULL.
 *
 * If @self is not a group, return %NULL.
 *
 * Returns: the remote-pending members, or %NULL
  */
const TpIntSet *
tp_channel_group_get_remote_pending (TpChannel *self)
{
  return self->priv->group_remote_pending;
}


/**
 * tp_channel_group_get_local_pending_info:
 * @self: a channel
 * @local_pending: the handle of a local-pending contact about whom more
 *  information is needed
 * @actor: either %NULL or a location to return the contact who requested
 *  the change
 * @reason: either %NULL or a location to return the reason for the change
 * @message: either %NULL or a location to return the user-supplied message
 *
 * If @local_pending is actually the handle of a local-pending contact,
 * write additional information into @actor, @reason and @message and return
 * %TRUE. The handle and message are not referenced or copied, and can only be
 * assumed to remain valid until the main loop is re-entered.
 *
 * If @local_pending is not the handle of a local-pending contact,
 * write 0 into @actor, %TP_CHANNEL_GROUP_CHANGE_REASON_NONE into @reason
 * and "" into @message, and return %FALSE.
 *
 * Returns: %TRUE if the contact is in fact local-pending
 */
gboolean
tp_channel_group_get_local_pending_info (TpChannel *self,
                                         TpHandle local_pending,
                                         TpHandle *actor,
                                         TpChannelGroupChangeReason *reason,
                                         const gchar **message)
{
  gboolean ret = FALSE;
  TpHandle a = 0;
  TpChannelGroupChangeReason r = TP_CHANNEL_GROUP_CHANGE_REASON_NONE;
  const gchar *m = "";

  if (self->priv->group_local_pending != NULL)
    {
      /* it could conceivably be someone who is local-pending */

      ret = tp_intset_is_member (self->priv->group_local_pending,
          local_pending);

      if (ret && self->priv->group_local_pending_info != NULL)
        {
          /* we might even have information about them */
          LocalPendingInfo *info = g_hash_table_lookup (
              self->priv->group_local_pending_info, GUINT_TO_POINTER (actor));

          if (info != NULL)
            {
              a = info->actor;
              r = info->reason;

              if (info->message != NULL)
                m = info->message;
            }
          /* else we have no info, which means (0, NONE, NULL) */
        }
    }

  if (actor != NULL)
    *actor = a;

  if (message != NULL)
    *message = m;

  if (reason != NULL)
    *reason = r;

  return ret;
}


/**
 * tp_channel_group_get_handle_owner:
 * @self: a channel
 * @handle: a handle which is a member of this channel
 *
 * Synopsis (see below for further explanation):
 *
 * - if @self is not a group or @handle is not a member of this channel,
 *   result is undefined;
 * - if @self does not have #TpChannel:ready = TRUE, result is undefined;
 * - if @self does not have flags that include %TP_CHANNEL_FLAG_PROPERTIES,
 *   result is undefined;
 * - if @handle is channel-specific and its globally valid "owner" is known,
 *   return that owner;
 * - if @handle is channel-specific and its globally valid "owner" is unknown,
 *   return zero;
 * - if @handle is globally valid, return @handle itself
 *
 * Some channels (those with flags that include
 * %TP_CHANNEL_FLAG_CHANNEL_SPECIFIC_HANDLES) have a concept of
 * "channel-specific handles". These are handles that only have meaning within
 * the context of the channel - for instance, in XMPP Multi-User Chat,
 * participants in a chatroom are identified by an in-room JID consisting
 * of the JID of the chatroom plus a local nickname.
 *
 * Depending on the protocol and configuration, it might be possible to find
 * out what globally valid handle (i.e. an identifier that you could add to
 * your contact list) "owns" a channel-specific handle. For instance, in
 * most XMPP MUC chatrooms, normal users cannot see what global JID
 * corresponds to an in-room JID, but moderators can.
 *
 * This is further complicated by the fact that channels with channel-specific
 * handles can sometimes have members with globally valid handles (for
 * instance, if you invite someone to an XMPP MUC using their globally valid
 * JID, you would expect to see the handle representing that JID in the
 * Group's remote-pending set).
 *
 * This function's result is undefined unless the channel is ready
 * and its flags include %TP_CHANNEL_FLAG_PROPERTIES (an implementation
 * without extra D-Bus round trips is not possible using the older API).
 *
 * Returns: the global handle that owns the given handle, or 0
 */
TpHandle
tp_channel_group_get_handle_owner (TpChannel *self,
                                   TpHandle handle)
{
  gpointer key, value;

  if (self->priv->group_handle_owners == NULL)
    {
      /* undefined result - pretending it's global is probably as good as
       * any other behaviour, since we can't know either way */
      return handle;
    }

  if (g_hash_table_lookup_extended (self->priv->group_handle_owners,
        GUINT_TO_POINTER (handle), &key, &value))
    {
      /* channel-specific, value is either owner or 0 if unknown */
      return GPOINTER_TO_UINT (value);
    }
  else
    {
      /* either already globally valid, or not a member */
      return handle;
    }
}


static void
tp_channel_got_group_flags_0_16_cb (TpChannel *self,
                                    guint flags,
                                    const GError *error,
                                    gpointer user_data G_GNUC_UNUSED,
                                    GObject *weak_object G_GNUC_UNUSED)
{
  g_assert (self->group_flags == 0);

  if (error != NULL)
    {
      DEBUG ("%p GetGroupFlags() failed, assuming initial flags 0: %s", self,
          error->message);
    }
  else
    {
      /* If we reach this point, GetAll has already failed... */
      if (flags & TP_CHANNEL_GROUP_FLAG_PROPERTIES)
        {
          DEBUG ("Treason uncloaked! The channel claims to support Group "
              "properties, but GetAll didn't work");
          flags &= ~TP_CHANNEL_GROUP_FLAG_PROPERTIES;
        }

      self->group_flags = flags;
      DEBUG ("Initial GroupFlags: %u", flags);

      if (flags != 0)
        g_object_notify ((GObject *) self, "group-flags");
    }

  self->priv->have_group_flags = 1;
  _tp_channel_continue_introspection (self);
}


static void
tp_channel_group_self_handle_changed_cb (TpChannel *self,
                                         guint self_handle,
                                         gpointer unused G_GNUC_UNUSED,
                                         GObject *unused_object G_GNUC_UNUSED)
{
  if (self_handle == self->group_self_handle)
    return;

  DEBUG ("%p SelfHandle changed to %u", self, self_handle);

  self->group_self_handle = self_handle;
  g_object_notify ((GObject *) self, "group-self-handle");
}


static void
tp_channel_got_self_handle_0_16_cb (TpChannel *self,
                                    guint self_handle,
                                    const GError *error,
                                    gpointer user_data G_GNUC_UNUSED,
                                    GObject *weak_object G_GNUC_UNUSED)
{
  if (error != NULL)
    {
      DEBUG ("%p Group.GetSelfHandle() failed, assuming 0: %s", self,
          error->message);
      tp_channel_group_self_handle_changed_cb (self, 0, NULL, NULL);
    }
  else
    {
      DEBUG ("Initial Group.SelfHandle: %u", self_handle);
      tp_channel_group_self_handle_changed_cb (self, self_handle, NULL, NULL);
    }

  _tp_channel_continue_introspection (self);
}


static void
_tp_channel_get_self_handle_0_16 (TpChannel *self)
{
  tp_cli_channel_interface_group_call_get_self_handle (self, -1,
      tp_channel_got_self_handle_0_16_cb, NULL, NULL, NULL);
}


static void
_tp_channel_get_group_flags_0_16 (TpChannel *self)
{
  tp_cli_channel_interface_group_call_get_group_flags (self, -1,
      tp_channel_got_group_flags_0_16_cb, NULL, NULL, NULL);
}


static void
_tp_channel_group_set_one_lp (TpChannel *self,
                              TpHandle handle,
                              TpHandle actor,
                              TpChannelGroupChangeReason reason,
                              const gchar *message)
{
  LocalPendingInfo *info = NULL;

  g_assert (self->priv->group_local_pending != NULL);

  tp_intset_add (self->priv->group_local_pending, handle);

  if (actor == 0 && reason == TP_CHANNEL_GROUP_CHANGE_REASON_NONE &&
      (message == NULL || message[0] == '\0'))
    {
      /* we just don't bother storing informationless local-pending */
      if (self->priv->group_local_pending_info != NULL)
        {
          g_hash_table_remove (self->priv->group_local_pending_info,
              GUINT_TO_POINTER (handle));
        }

      return;
    }

  if (self->priv->group_local_pending_info == NULL)
    {
      self->priv->group_local_pending_info = g_hash_table_new_full (
          g_direct_hash, g_direct_equal, NULL,
          (GDestroyNotify) local_pending_info_free);
    }
  else
    {
      info = g_hash_table_lookup (self->priv->group_local_pending_info,
          GUINT_TO_POINTER (handle));
    }

  if (info == NULL)
    {
      info = g_slice_new0 (LocalPendingInfo);
    }
  else
    {
      g_hash_table_steal (self->priv->group_local_pending_info,
          GUINT_TO_POINTER (handle));
    }

  info->actor = actor;
  info->reason = reason;
  g_free (info->message);

  if (message == NULL || message[0] == '\0')
    info->message = NULL;
  else
    info->message = g_strdup (message);

  g_hash_table_insert (self->priv->group_local_pending_info,
      GUINT_TO_POINTER (handle), info);
}


static void
_tp_channel_group_set_lp (TpChannel *self,
                          const GPtrArray *info)
{
  guint i;

  /* should only be called during initialization */
  g_assert (self->priv->group_local_pending != NULL);
  g_assert (self->priv->group_local_pending_info == NULL);

  tp_intset_clear (self->priv->group_local_pending);

  /* NULL-safe for ease of use with tp_asv_get_boxed */
  if (info == NULL)
    {
      return;
    }

  for (i = 0; i < info->len; i++)
    {
      GValueArray *item = g_ptr_array_index (info, i);
      TpHandle handle = g_value_get_uint (item->values + 0);
      TpHandle actor = g_value_get_uint (item->values + 1);
      TpChannelGroupChangeReason reason = g_value_get_uint (
          item->values + 2);
      const gchar *message = g_value_get_string (item->values + 3);

      DEBUG ("+L %u, actor=%u, reason=%u, message=%s", handle,
          actor, reason, message);
      _tp_channel_group_set_one_lp (self, handle, actor,
          reason, message);
    }
}


static void
tp_channel_got_all_members_0_16_cb (TpChannel *self,
                                    const GArray *members,
                                    const GArray *local_pending,
                                    const GArray *remote_pending,
                                    const GError *error,
                                    gpointer user_data G_GNUC_UNUSED,
                                    GObject *weak_object G_GNUC_UNUSED)
{
  g_assert (self->priv->group_local_pending == NULL);
  g_assert (self->priv->group_local_pending_info == NULL);
  g_assert (self->priv->group_members == NULL);
  g_assert (self->priv->group_remote_pending == NULL);

  if (error == NULL)
    {
      DEBUG ("%p GetAllMembers returned %u members + %u LP + %u RP",
          self, members->len, local_pending->len, remote_pending->len);

      self->priv->group_local_pending = tp_intset_from_array (local_pending);
      self->priv->group_members = tp_intset_from_array (members);
      self->priv->group_remote_pending = tp_intset_from_array (remote_pending);

      /* the local-pending info will be filled in with the result of
       * GetLocalPendingMembersWithInfo, if it succeeds */
    }
  else
    {
      DEBUG ("%p GetAllMembers failed, assuming empty: %s", self,
          error->message);

      self->priv->group_local_pending = tp_intset_new ();
      self->priv->group_members = tp_intset_new ();
      self->priv->group_remote_pending = tp_intset_new ();
    }

  g_assert (self->priv->group_local_pending != NULL);
  g_assert (self->priv->group_members != NULL);
  g_assert (self->priv->group_remote_pending != NULL);

  _tp_channel_continue_introspection (self);
}


static void
_tp_channel_get_all_members_0_16 (TpChannel *self)
{
  tp_cli_channel_interface_group_call_get_all_members (self, -1,
      tp_channel_got_all_members_0_16_cb, NULL, NULL, NULL);
}


static void
tp_channel_glpmwi_0_16_cb (TpChannel *self,
                           const GPtrArray *info,
                           const GError *error,
                           gpointer user_data G_GNUC_UNUSED,
                           GObject *object G_GNUC_UNUSED)
{
  /* this should always run after tp_channel_got_all_members_0_16 */
  g_assert (self->priv->group_local_pending != NULL);
  g_assert (self->priv->group_local_pending_info == NULL);

  if (error == NULL)
    {
      DEBUG ("%p GetLocalPendingMembersWithInfo returned %u records",
          self, info->len);
      _tp_channel_group_set_lp (self, info);
    }
  else
    {
      DEBUG ("%p GetLocalPendingMembersWithInfo failed, keeping result of "
          "GetAllMembers instead: %s", self, error->message);
    }

  _tp_channel_continue_introspection (self);
}


static void
_tp_channel_glpmwi_0_16 (TpChannel *self)
{
  tp_cli_channel_interface_group_call_get_local_pending_members_with_info (
      self, -1, tp_channel_glpmwi_0_16_cb, NULL, NULL, NULL);
}


static void
tp_channel_got_group_properties_cb (TpProxy *proxy,
                                    GHashTable *asv,
                                    const GError *error,
                                    gpointer unused G_GNUC_UNUSED,
                                    GObject *unused_object G_GNUC_UNUSED)
{
  TpChannel *self = TP_CHANNEL (proxy);
  static GType au_type = 0;

  if (G_UNLIKELY (au_type == 0))
    {
      au_type = dbus_g_type_get_collection ("GArray", G_TYPE_UINT);
    }

  if (error != NULL)
    {
      DEBUG ("Error getting group properties, falling back to 0.16 API: %s",
          error->message);
    }
  else if ((tp_asv_get_uint32 (asv, "GroupFlags", NULL)
      & TP_CHANNEL_GROUP_FLAG_PROPERTIES) == 0)
    {
      DEBUG ("Got group properties, but no Properties flag: assuming a "
          "broken implementation and falling back to 0.16 API");
    }
  else
    {
      GHashTable *handle_owners;
      GArray *arr;

      DEBUG ("Received %u group properties", g_hash_table_size (asv));

      self->group_flags = tp_asv_get_uint32 (asv, "GroupFlags", NULL);
      DEBUG ("Initial GroupFlags: %u", self->group_flags);
      self->priv->have_group_flags = 1;

      if (self->group_flags != 0)
        g_object_notify ((GObject *) self, "group-flags");

      tp_channel_group_self_handle_changed_cb (self,
          tp_asv_get_uint32 (asv, "SelfHandle", NULL), NULL, NULL);

      g_assert (self->priv->group_members == NULL);
      g_assert (self->priv->group_remote_pending == NULL);

      arr = tp_asv_get_boxed (asv, "Members", au_type);

      if (arr == NULL)
        self->priv->group_members = tp_intset_new ();
      else
        self->priv->group_members = tp_intset_from_array (arr);

      arr = tp_asv_get_boxed (asv, "RemotePendingMembers", au_type);

      if (arr == NULL)
        self->priv->group_remote_pending = tp_intset_new ();
      else
        self->priv->group_remote_pending = tp_intset_from_array (arr);

      g_assert (self->priv->group_local_pending == NULL);
      g_assert (self->priv->group_local_pending_info == NULL);

      self->priv->group_local_pending = tp_intset_new ();

      /* this is NULL-safe with respect to the array */
      _tp_channel_group_set_lp (self,
          tp_asv_get_boxed (asv, "LocalPendingMembers",
              TP_ARRAY_TYPE_LOCAL_PENDING_INFO_LIST));

      handle_owners = tp_asv_get_boxed (asv, "HandleOwners",
          TP_HASH_TYPE_HANDLE_OWNER_MAP);

      self->priv->group_handle_owners = g_hash_table_new (g_direct_hash,
          g_direct_equal);

      if (handle_owners != NULL)
        tp_g_hash_table_update (self->priv->group_handle_owners,
            handle_owners, NULL, NULL);

      _tp_channel_continue_introspection (self);
      return;
    }

  /* Failure case: fall back. This is quite annoying, as we need to combine:
   *
   * - GetGroupFlags
   * - GetAllMembers
   * - GetLocalPendingMembersWithInfo
   *
   * Channel-specific handles can't really have a sane client API (without
   * lots of silly round-trips) unless the CM implements the HandleOwners
   * property, so I intend to ignore this in the fallback case.
   */

  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_group_flags_0_16);

  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_self_handle_0_16);

  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_all_members_0_16);

  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_glpmwi_0_16);

  _tp_channel_continue_introspection (self);
}


static void
tp_channel_group_members_changed_cb (TpChannel *self,
                                     const gchar *message,
                                     const GArray *added,
                                     const GArray *removed,
                                     const GArray *local_pending,
                                     const GArray *remote_pending,
                                     guint actor,
                                     guint reason,
                                     gpointer unused G_GNUC_UNUSED,
                                     GObject *unused_object G_GNUC_UNUSED)
{
  guint i;

  DEBUG ("%p MembersChanged: added %u, removed %u, "
      "moved %u to LP and %u to RP, actor %u, reason %u, message %s",
      self, added->len, removed->len, local_pending->len, remote_pending->len,
      actor, reason, message);

  if (self->priv->group_members != NULL)
    {
      g_assert (self->priv->group_members != NULL);
      g_assert (self->priv->group_local_pending != NULL);
      g_assert (self->priv->group_remote_pending != NULL);

      for (i = 0; i < added->len; i++)
        {
          TpHandle handle = g_array_index (added, guint, i);

          DEBUG ("+++ contact#%u", handle);
          tp_intset_add (self->priv->group_members, handle);
        }

      for (i = 0; i < local_pending->len; i++)
        {
          TpHandle handle = g_array_index (local_pending, guint, i);

          DEBUG ("+LP contact#%u", handle);

          /* Special-case renaming a local-pending contact, if the
           * signal is spec-compliant. Keep the old actor/reason/message in
           * this case */
          if (reason == TP_CHANNEL_GROUP_CHANGE_REASON_RENAMED &&
              added->len == 0 &&
              local_pending->len == 1 &&
              remote_pending->len == 0 &&
              removed->len == 1 &&
              self->priv->group_local_pending_info != NULL)
            {
              TpHandle old = g_array_index (removed, guint, 0);
              LocalPendingInfo *info = g_hash_table_lookup (
                  self->priv->group_local_pending_info,
                  GUINT_TO_POINTER (old));

              if (info != NULL)
                {
                  _tp_channel_group_set_one_lp (self, handle,
                      info->actor, info->reason, info->message);
                  continue;
                }
            }

          /* not reached if the Renamed special case occurred */
          _tp_channel_group_set_one_lp (self, handle, actor,
              reason, message);
        }

      for (i = 0; i < remote_pending->len; i++)
        {
          TpHandle handle = g_array_index (remote_pending, guint, i);

          DEBUG ("+RP contact#%u", handle);
          tp_intset_add (self->priv->group_remote_pending, handle);
        }

      for (i = 0; i < removed->len; i++)
        {
          TpHandle handle = g_array_index (removed, guint, i);

          DEBUG ("--- contact#%u", handle);

          if (self->priv->group_local_pending_info != NULL)
            g_hash_table_remove (self->priv->group_local_pending_info,
                GUINT_TO_POINTER (handle));

          tp_intset_remove (self->priv->group_members, handle);
          tp_intset_remove (self->priv->group_local_pending, handle);
          tp_intset_remove (self->priv->group_remote_pending, handle);

          if (handle == self->group_self_handle)
            {
              self->priv->group_remove_reason = reason;
              g_free (self->priv->group_remove_message);
              self->priv->group_remove_message = g_strdup (message);
            }
          /* FIXME: should check against the Connection's self-handle too,
           * after I add that API */
        }

      /* FIXME: emit a signal or something */
    }
}


static void
tp_channel_handle_owners_changed_cb (TpChannel *self,
                                     GHashTable *added,
                                     const GArray *removed,
                                     gpointer unused G_GNUC_UNUSED,
                                     GObject *unused_object G_GNUC_UNUSED)
{
  guint i;

  /* ignore the signal if we don't have the initial set yet */
  if (self->priv->group_handle_owners == NULL)
    return;

  tp_g_hash_table_update (self->priv->group_handle_owners, added, NULL, NULL);

  for (i = 0; i < removed->len; i++)
    {
      g_hash_table_remove (self->priv->group_handle_owners,
          GUINT_TO_POINTER (g_array_index (removed, guint, i)));
    }
}


static void
tp_channel_group_flags_changed_cb (TpChannel *self,
                                   guint added,
                                   guint removed,
                                   gpointer unused G_GNUC_UNUSED,
                                   GObject *unused_object G_GNUC_UNUSED)
{
  if (self->priv->have_group_flags)
    {
      DEBUG ("%p GroupFlagsChanged: +%u -%u", self, added, removed);

      added &= ~(self->group_flags);
      removed &= self->group_flags;

      DEBUG ("%p GroupFlagsChanged (after filtering): +%u -%u",
          self, added, removed);

      self->group_flags |= added;
      self->group_flags &= ~removed;

      if (added != 0 || removed != 0)
        g_object_notify ((GObject *) self, "group-flags");
    }
}


void
_tp_channel_get_group_properties (TpChannel *self)
{
  tp_cli_channel_interface_group_connect_to_members_changed (self,
      tp_channel_group_members_changed_cb, NULL, NULL, NULL, NULL);

  tp_cli_channel_interface_group_connect_to_group_flags_changed (self,
      tp_channel_group_flags_changed_cb, NULL, NULL, NULL, NULL);

  tp_cli_channel_interface_group_connect_to_self_handle_changed (self,
      tp_channel_group_self_handle_changed_cb, NULL, NULL, NULL, NULL);

  tp_cli_channel_interface_group_connect_to_handle_owners_changed (self,
      tp_channel_handle_owners_changed_cb, NULL, NULL, NULL, NULL);

  /* First try the 0.17 API (properties). If this fails we'll fall back */
  tp_cli_dbus_properties_call_get_all (self, -1,
      TP_IFACE_CHANNEL_INTERFACE_GROUP, tp_channel_got_group_properties_cb,
      NULL, NULL, NULL);
}
