/*
 * channel.c - proxy for a Telepathy channel
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

#include "telepathy-glib/channel.h"

#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/handle.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "telepathy-glib/dbus-internal.h"
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/_gen/signals-marshal.h"

#include "_gen/tp-cli-channel-body.h"

/**
 * SECTION:channel
 * @title: TpChannel
 * @short_description: proxy object for a Telepathy channel
 * @see_also: #TpConnection, channel-group, channel-text, channel-media
 *
 * #TpChannel objects provide convenient access to Telepathy channels.
 *
 * Compared with a simple proxy for method calls, they add the following
 * features:
 *
 * * calling GetChannelType(), GetInterfaces(), GetHandles() automatically
 *
 * This section also documents the auto-generated C wrappers for the
 * Channel D-Bus interface. Of these, in general, only
 * tp_cli_channel_call_close() and tp_cli_channel_run_close() are useful (the
 * #TpChannel object provides a more convenient API for the rest).
 *
 * Since: 0.7.1
 */

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

/**
 * TpChannelClass:
 * @parent_class: parent class
 * @priv: pointer to opaque private data
 *
 * The class of a #TpChannel.
 *
 * Since: 0.7.1
 */
struct _TpChannelClass {
    TpProxyClass parent_class;
    gpointer priv;
};

/**
 * TpChannel:
 * @parent: parent class instance
 * @ready: the same as #TpChannel:channel-ready; should be considered
 *  read-only
 * @_reserved_flags: (private, reserved for future use)
 * @channel_type: quark representing the channel type; should be considered
 *  read-only
 * @handle_type: the handle type (%TP_UNKNOWN_HANDLE_TYPE if not yet known);
 *  should be considered read-only
 * @handle: the handle with which this channel communicates (0 if
 *  not yet known or if @handle_type is %TP_HANDLE_TYPE_NONE); should be
 *  considered read-only
 * @priv: pointer to opaque private data
 *
 * A proxy object for a Telepathy channel.
 *
 * Since: 0.7.1
 */
struct _TpChannel {
    TpProxy parent;

    TpConnection *connection;

    gboolean ready:1;
    gboolean _reserved_flags:31;
    GQuark channel_type;
    TpHandleType handle_type;
    TpHandle handle;

    TpHandle group_self_handle;
    TpChannelGroupFlags group_flags;

    TpChannelPrivate *priv;
};

typedef void (*TpChannelProc) (TpChannel *self);


/* These have char-like values so we can use them in debug messages */
typedef enum {
    GROUP_NONE = '\0',
    GROUP_LOCAL_PENDING = 'L',
    GROUP_REMOTE_PENDING = 'R',
    GROUP_MEMBER = 'M'
} GroupMembership;


typedef struct {
    GroupMembership state;
    /* these three are only populated for local-pending members - we don't
     * expose them in our API for full or remote-pending members anyway */
    TpHandle actor;
    TpChannelGroupChangeReason reason;
    gchar *message;
} GroupMemberInfo;


static void
group_member_info_free (GroupMemberInfo *gmi)
{
  g_free (gmi->message);
  g_slice_free (GroupMemberInfo, gmi);
}


struct _TpChannelPrivate {
    gulong conn_invalidated_id;

    /* GQueue of TpChannelProc */
    GQueue *introspect_needed;

    /* (TpHandle => GroupMemberInfo), or NULL if members not discovered yet */
    GHashTable *group;
    /* reason the self-handle left, message == NULL if not removed */
    gchar *group_remove_message;
    TpChannelGroupChangeReason group_remove_reason;
    /* guint => guint, NULL if not discovered yet */
    GHashTable *group_handle_owners;

    gboolean have_group_flags:1;
};

enum
{
  PROP_CONNECTION = 1,
  PROP_CHANNEL_TYPE,
  PROP_HANDLE_TYPE,
  PROP_HANDLE,
  PROP_CHANNEL_READY,
  N_PROPS
};

G_DEFINE_TYPE_WITH_CODE (TpChannel,
    tp_channel,
    TP_TYPE_PROXY,
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_IFACE, NULL));


/* Convenient property accessors for C (these duplicate the properties) */


/**
 * tp_channel_get_channel_type:
 * @self: a channel
 *
 * Get the D-Bus interface name representing this channel's type,
 * if it has been discovered.
 *
 * This is the same as the #TpChannel:channel-type property.
 *
 * Returns: the channel type, if the channel is ready; either the channel
 *  type or %NULL, if the channel is not yet ready.
 */
const gchar *
tp_channel_get_channel_type (TpChannel *self)
{
  return g_quark_to_string (self->channel_type);
}


/**
 * tp_channel_get_channel_type_id:
 * @self: a channel
 *
 * Get the D-Bus interface name representing this channel's type, as a GQuark,
 * if it has been discovered.
 *
 * This is the same as the #TpChannel:channel-type property, except that it
 * is a GQuark rather than a string.
 *
 * Returns: the channel type, if the channel is ready; either the channel
 *  type or 0, if the channel is not yet ready.
 */
GQuark
tp_channel_get_channel_type_id (TpChannel *self)
{
  return self->channel_type;
}


/**
 * tp_channel_get_handle:
 * @self: a channel
 * @handle_type: if not %NULL, used to return the type of this handle
 *
 * Get the handle representing the contact, chatroom, etc. with which this
 * channel communicates for its whole lifetime, or 0 if there is no such
 * handle or it has not yet been discovered.
 *
 * This is the same as the #TpChannel:handle property.
 *
 * If %handle_type is not %NULL, the type of handle is written into it.
 * This will be %TP_UNKNOWN_HANDLE_TYPE if the handle has not yet been
 * discovered, or %TP_HANDLE_TYPE_NONE if there is no handle with which this
 * channel will always communicate. This is the same as the
 * #TpChannel:handle-type property.
 *
 * Returns: the handle
 */
TpHandle
tp_channel_get_handle (TpChannel *self,
                       TpHandleType *handle_type)
{
  if (handle_type != NULL)
    {
      *handle_type = self->handle_type;
    }

  return self->handle;
}


/**
 * tp_channel_is_ready:
 * @self: a channel
 *
 * Returns the same thing as the #TpChannel:channel-ready property.
 *
 * Returns: %TRUE if introspection has completed
 */
gboolean
tp_channel_is_ready (TpChannel *self)
{
  return self->ready;
}


/**
 * tp_channel_borrow_connection:
 * @self: a channel
 *
 * Returns the connection for this channel. The returned pointer is only valid
 * while this channel is valid - reference it with g_object_ref() if needed.
 *
 * Returns: a #TpConnection representing the connection to which this channel
 *  is attached.
 */
TpConnection *
tp_channel_borrow_connection (TpChannel *self)
{
  return self->connection;
}


static void
tp_channel_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
  TpChannel *self = TP_CHANNEL (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;
    case PROP_CHANNEL_READY:
      g_value_set_boolean (value, self->ready);
      break;
    case PROP_CHANNEL_TYPE:
      g_value_set_static_string (value,
          g_quark_to_string (self->channel_type));
      break;
    case PROP_HANDLE_TYPE:
      g_value_set_uint (value, self->handle_type);
      break;
    case PROP_HANDLE:
      g_value_set_uint (value, self->handle);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_channel_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  TpChannel *self = TP_CHANNEL (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      self->connection = TP_CONNECTION (g_value_dup_object (value));
      break;
    case PROP_CHANNEL_TYPE:
      /* can only be set in constructor */
      g_assert (self->channel_type == 0);
      self->channel_type = g_quark_from_string (g_value_get_string (value));
      break;
    case PROP_HANDLE_TYPE:
      self->handle_type = g_value_get_uint (value);
      break;
    case PROP_HANDLE:
      self->handle = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


/* Introspection etc. */


static void
_tp_channel_continue_introspection (TpChannel *self)
{
  g_assert (self->priv->introspect_needed != NULL);

  if (g_queue_peek_head (self->priv->introspect_needed) == NULL)
    {
      g_queue_free (self->priv->introspect_needed);
      self->priv->introspect_needed = NULL;

      DEBUG ("%p: channel ready", self);
      self->ready = TRUE;
      g_object_notify ((GObject *) self, "channel-ready");
    }
  else
    {
      TpChannelProc next = g_queue_pop_head (self->priv->introspect_needed);

      next (self);
    }
}


static void
tp_channel_got_group_flags_0_16_cb (TpChannel *self,
                                    guint flags,
                                    const GError *error,
                                    gpointer user_data G_GNUC_UNUSED,
                                    GObject *weak_object G_GNUC_UNUSED)
{
  if (error != NULL)
    {
      DEBUG ("%p GetGroupFlags() failed, assuming initial flags 0: %s", self,
          error->message);
      self->group_flags = 0;
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
  DEBUG ("%p SelfHandle changed to %u", self, self_handle);

  self->group_self_handle = self_handle;
  /* FIXME: emit a signal or something */
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
_tp_channel_group_set (TpChannel *self,
                       TpHandle handle,
                       GroupMembership state,
                       TpHandle actor,
                       TpChannelGroupChangeReason reason,
                       const gchar *message)
{
  GroupMemberInfo *info;

  g_return_if_fail (self->priv->group != NULL);

  info = g_hash_table_lookup (self->priv->group, GUINT_TO_POINTER (handle));

  if (info == NULL)
    info = g_slice_new0 (GroupMemberInfo);
  else
    g_hash_table_steal (self->priv->group, GUINT_TO_POINTER (handle));

  info->state = state;
  info->actor = actor;
  info->reason = reason;
  g_free (info->message);

  if (message == NULL || message[0] == '\0')
    info->message = NULL;
  else
    info->message = g_strdup (message);

  g_hash_table_insert (self->priv->group, GUINT_TO_POINTER (handle), info);
}


static void
_tp_channel_group_set_many (TpChannel *self,
                            const GArray *handles,
                            GroupMembership state,
                            TpHandle actor,
                            TpChannelGroupChangeReason reason,
                            const gchar *message)
{
  guint i;

  /* must be NULL-safe for ease of use with tp_asv_get_boxed */
  if (handles == NULL)
    return;

  for (i = 0; i < handles->len; i++)
    {
      TpHandle handle = g_array_index (handles, guint, i);

      DEBUG ("+%c %u", state, handle);
      _tp_channel_group_set (self, handle, GROUP_MEMBER, 0, 0, NULL);
    }
}


static void
_tp_channel_group_set_lp (TpChannel *self,
                          const GPtrArray *info)
{
  guint i;

  /* must be NULL-safe for ease of use with tp_asv_get_boxed */
  if (info == NULL)
    return;

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
      _tp_channel_group_set (self, handle, GROUP_LOCAL_PENDING, actor,
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
  g_assert (self->priv->group == NULL);

  self->priv->group = g_hash_table_new_full (g_direct_hash,
      g_direct_equal, NULL, (GDestroyNotify) group_member_info_free);

  if (error == NULL)
    {
      DEBUG ("%p GetAllMembers returned %u members + %u LP + %u RP",
          self, members->len, local_pending->len, remote_pending->len);

      _tp_channel_group_set_many (self, members, GROUP_MEMBER, 0,
          TP_CHANNEL_GROUP_CHANGE_REASON_NONE, NULL);
      /* the local-pending members will be overwritten by the result of
       * GetLocalPendingMembersWithInfo, if it succeeds */
      _tp_channel_group_set_many (self, local_pending, GROUP_LOCAL_PENDING, 0,
          TP_CHANNEL_GROUP_CHANGE_REASON_NONE, NULL);
      _tp_channel_group_set_many (self, remote_pending, GROUP_REMOTE_PENDING,
          0, TP_CHANNEL_GROUP_CHANGE_REASON_NONE, NULL);
    }
  else
    {
      DEBUG ("%p GetAllMembers failed, assuming empty: %s", self,
          error->message);
    }

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
  g_assert (self->priv->group != NULL);

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

      DEBUG ("Received %u group properties", g_hash_table_size (asv));

      self->group_flags = tp_asv_get_uint32 (asv, "GroupFlags", NULL);
      DEBUG ("Initial GroupFlags: %u", self->group_flags);
      self->priv->have_group_flags = 1;

      tp_channel_group_self_handle_changed_cb (self,
          tp_asv_get_uint32 (asv, "SelfHandle", NULL), NULL, NULL);

      g_assert (self->priv->group == NULL);

      self->priv->group = g_hash_table_new_full (g_direct_hash,
          g_direct_equal, NULL, (GDestroyNotify) group_member_info_free);

      /* all of these are NULL-safe for the handles array */
      _tp_channel_group_set_many (self,
          tp_asv_get_boxed (asv, "Members", au_type),
          GROUP_MEMBER, 0, TP_CHANNEL_GROUP_CHANGE_REASON_NONE, NULL);
      _tp_channel_group_set_many (self,
          tp_asv_get_boxed (asv, "RemotePendingMembers", au_type),
          GROUP_REMOTE_PENDING, 0, TP_CHANNEL_GROUP_CHANGE_REASON_NONE, NULL);
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

  if (self->priv->group != NULL)
    {
      for (i = 0; i < added->len; i++)
        {
          TpHandle handle = g_array_index (added, guint, i);

          DEBUG ("+++ contact#%u", handle);
          _tp_channel_group_set (self, handle, GROUP_MEMBER, 0, 0,
              NULL);
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
              removed->len == 1)
            {
              TpHandle old = g_array_index (removed, guint, 0);
              GroupMemberInfo *info = g_hash_table_lookup (self->priv->group,
                  GUINT_TO_POINTER (old));

              if (info != NULL && info->state == GROUP_LOCAL_PENDING)
                {
                  _tp_channel_group_set (self, handle, GROUP_LOCAL_PENDING,
                      info->actor, info->reason, info->message);
                  continue;
                }
            }

          _tp_channel_group_set (self, handle, GROUP_LOCAL_PENDING, actor,
              reason, message);
        }

      for (i = 0; i < remote_pending->len; i++)
        {
          TpHandle handle = g_array_index (remote_pending, guint, i);

          DEBUG ("+RP contact#%u", handle);
          _tp_channel_group_set (self, handle, GROUP_REMOTE_PENDING, 0,
              0, NULL);
        }

      for (i = 0; i < removed->len; i++)
        {
          TpHandle handle = g_array_index (removed, guint, i);

          DEBUG ("--- contact#%u", handle);
          g_hash_table_remove (self->priv->group, GUINT_TO_POINTER (handle));

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
  DEBUG ("%p GroupFlagsChanged: +%u -%u", self, added, removed);

  if (self->priv->have_group_flags)
    {
      self->group_flags |= added;
      self->group_flags &= ~removed;

      /* FIXME: emit a signal or something */
    }
}


static void
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


static void
tp_channel_got_interfaces_cb (TpChannel *self,
                              const gchar **interfaces,
                              const GError *error,
                              gpointer unused,
                              GObject *unused2)
{
  if (error != NULL)
    {
      DEBUG ("%p: GetInterfaces() failed: %s", self, error->message);
      interfaces = NULL;
    }

  if (interfaces != NULL)
    {
      const gchar **iter;

      for (iter = interfaces; *iter != NULL; iter++)
        {
          DEBUG ("- %s", *iter);

          if (tp_dbus_check_valid_interface_name (*iter, NULL))
            {
              GQuark q = g_quark_from_string (*iter);
              tp_proxy_add_interface_by_id ((TpProxy *) self, q);

              if (q == TP_IFACE_QUARK_CHANNEL_INTERFACE_GROUP)
                {
                  g_queue_push_tail (self->priv->introspect_needed,
                      _tp_channel_get_group_properties);
                }
            }
          else
            {
              DEBUG ("\tInterface %s not valid", *iter);
            }
        }
    }

  _tp_channel_continue_introspection (self);
}


static void
_tp_channel_get_interfaces (TpChannel *self)
{
  tp_cli_channel_call_get_interfaces (self, -1,
      tp_channel_got_interfaces_cb, NULL, NULL, NULL);
}


static void
tp_channel_got_channel_type_cb (TpChannel *self,
                                const gchar *channel_type,
                                const GError *error,
                                gpointer unused,
                                GObject *unused2)
{
  GError *err2 = NULL;

  if (error != NULL)
    {
      DEBUG ("%p: GetChannelType() failed: %s", self, error->message);
    }
  else if (tp_dbus_check_valid_interface_name (channel_type, &err2))
    {
      DEBUG ("%p: Introspected channel type %s", self, channel_type);
      self->channel_type = g_quark_from_string (channel_type);
      g_object_notify ((GObject *) self, "channel-type");

      tp_proxy_add_interface_by_id ((TpProxy *) self, self->channel_type);

    }
  else
    {
      DEBUG ("%p: channel type %s not valid: %s", self, channel_type,
          err2->message);
      g_error_free (err2);
    }

  _tp_channel_continue_introspection (self);
}


static void
_tp_channel_get_channel_type (TpChannel *self)
{
  if (self->channel_type == 0)
    {
      tp_cli_channel_call_get_channel_type (self, -1,
          tp_channel_got_channel_type_cb, NULL, NULL, NULL);
    }
  else
    {
      tp_proxy_add_interface_by_id ((TpProxy *) self, self->channel_type);
      _tp_channel_continue_introspection (self);
    }
}


static void
tp_channel_got_handle_cb (TpChannel *self,
                          guint handle_type,
                          guint handle,
                          const GError *error,
                          gpointer unused,
                          GObject *unused2)
{
  if (error == NULL)
    {
      DEBUG ("%p: Introspected handle #%d of type %d", self, handle,
          handle_type);
      self->handle_type = handle_type;
      self->handle = handle;
      g_object_notify ((GObject *) self, "handle-type");
      g_object_notify ((GObject *) self, "handle");
    }
  else
    {
      DEBUG ("%p: GetHandle() failed: %s", self, error->message);
    }

  _tp_channel_continue_introspection (self);
}


static void
_tp_channel_get_handle (TpChannel *self)
{
  if (self->handle_type == TP_UNKNOWN_HANDLE_TYPE
      || (self->handle == 0 && self->handle_type != TP_HANDLE_TYPE_NONE))
    {
      tp_cli_channel_call_get_handle (self, -1,
          tp_channel_got_handle_cb, NULL, NULL, NULL);
    }
  else
    {
      _tp_channel_continue_introspection (self);
    }
}


static void
tp_channel_closed_cb (TpChannel *self,
                      gpointer user_data,
                      GObject *weak_object)
{
  GError e = { TP_DBUS_ERRORS, TP_DBUS_ERROR_OBJECT_REMOVED,
      "Channel was closed" };

  if (self->priv->group_remove_message != NULL)
    {
      e.domain = TP_ERRORS_REMOVED_FROM_GROUP;
      e.code = self->priv->group_remove_reason;
      e.message = self->priv->group_remove_message;
    }

  tp_proxy_invalidate ((TpProxy *) self, &e);
}

static void
tp_channel_connection_invalidated_cb (TpConnection *conn,
                                      guint domain,
                                      guint code,
                                      gchar *message,
                                      TpChannel *self)
{
  const GError e = { domain, code, message };

  g_signal_handler_disconnect (conn, self->priv->conn_invalidated_id);
  self->priv->conn_invalidated_id = 0;

  /* tp_proxy_invalidate and g_object_notify call out to user code - add a
   * temporary ref to ensure that we don't become finalized while doing so */
  g_object_ref (self);

  tp_proxy_invalidate ((TpProxy *) self, &e);

  /* this channel's handle is now meaningless */
  if (self->handle != 0)
    {
      self->handle = 0;
      g_object_notify ((GObject *) self, "handle");
    }

  g_object_unref (self);
}

static GObject *
tp_channel_constructor (GType type,
                        guint n_params,
                        GObjectConstructParam *params)
{
  GObjectClass *object_class = (GObjectClass *) tp_channel_parent_class;
  TpChannel *self = TP_CHANNEL (object_class->constructor (type,
        n_params, params));

  /* If our TpConnection dies, so do we. */
  self->priv->conn_invalidated_id = g_signal_connect (self->connection,
      "invalidated", G_CALLBACK (tp_channel_connection_invalidated_cb),
      self);

  /* Connect to my own Closed signal and self-destruct when it arrives.
   * The channel hasn't had a chance to become invalid yet, so we can
   * assume that this signal connection will work */
  tp_cli_channel_connect_to_closed (self, tp_channel_closed_cb, NULL, NULL,
      NULL, NULL);

  DEBUG ("%p: constructed with channel type \"%s\", handle #%d of type %d",
      self, (self->channel_type != 0) ? g_quark_to_string (self->channel_type)
                                      : "(null)",
      self->handle, self->handle_type);

  self->priv->introspect_needed = g_queue_new ();

  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_channel_type);

  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_interfaces);

  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_handle);

  _tp_channel_continue_introspection (self);

  return (GObject *) self;
}

static void
tp_channel_init (TpChannel *self)
{
  DEBUG ("%p", self);

  self->channel_type = 0;
  self->handle_type = TP_UNKNOWN_HANDLE_TYPE;
  self->handle = 0;
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CHANNEL,
      TpChannelPrivate);
}

static void
tp_channel_dispose (GObject *object)
{
  TpChannel *self = (TpChannel *) object;

  DEBUG ("%p", self);

  if (self->priv->conn_invalidated_id != 0)
    g_signal_handler_disconnect (self->connection,
        self->priv->conn_invalidated_id);

  self->priv->conn_invalidated_id = 0;

  g_object_unref (self->connection);
  self->connection = NULL;

  ((GObjectClass *) tp_channel_parent_class)->dispose (object);
}

static void
tp_channel_finalize (GObject *object)
{
  TpChannel *self = (TpChannel *) object;

  DEBUG ("%p", self);

  g_free (self->priv->group_remove_message);
  self->priv->group_remove_message = NULL;

  if (self->priv->group != NULL)
    {
      g_hash_table_destroy (self->priv->group);
      self->priv->group = NULL;
    }

  if (self->priv->introspect_needed != NULL)
    {
      g_queue_free (self->priv->introspect_needed);
      self->priv->introspect_needed = NULL;
    }

  ((GObjectClass *) tp_channel_parent_class)->finalize (object);
}

static void
tp_channel_class_init (TpChannelClass *klass)
{
  GParamSpec *param_spec;
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  tp_channel_init_known_interfaces ();

  g_type_class_add_private (klass, sizeof (TpChannelPrivate));

  object_class->constructor = tp_channel_constructor;
  object_class->get_property = tp_channel_get_property;
  object_class->set_property = tp_channel_set_property;
  object_class->dispose = tp_channel_dispose;
  object_class->finalize = tp_channel_finalize;

  proxy_class->interface = TP_IFACE_QUARK_CHANNEL;
  proxy_class->must_have_unique_name = TRUE;

  /**
   * TpChannel:channel-type:
   *
   * The D-Bus interface representing the type of this channel.
   *
   * Read-only except during construction. If %NULL during construction
   * (default), we ask the remote D-Bus object what its channel type is;
   * reading this property will yield %NULL until we get the reply, or if
   * GetChannelType() fails.
   */
  g_object_class_override_property (object_class, PROP_CHANNEL_TYPE,
      "channel-type");

  /**
   * TpChannel:handle-type:
   *
   * The #TpHandleType of this channel's associated handle, or 0 if no
   * handle, or TP_UNKNOWN_HANDLE_TYPE if unknown.
   *
   * Read-only except during construction. If this is TP_UNKNOWN_HANDLE_TYPE
   * during construction (default), we ask the remote D-Bus object what its
   * handle type is; reading this property will yield TP_UNKNOWN_HANDLE_TYPE
   * until we get the reply.
   */
  g_object_class_override_property (object_class, PROP_HANDLE_TYPE,
      "handle-type");

  /**
   * TpChannel:handle:
   *
   * This channel's associated handle, or 0 if no handle or unknown.
   *
   * Read-only except during construction. If this is 0
   * during construction, and handle-type is not TP_HANDLE_TYPE_NONE (== 0),
   * we ask the remote D-Bus object what its handle type is; reading this
   * property will yield 0 until we get the reply, or if GetHandle()
   * fails.
   */
  g_object_class_override_property (object_class, PROP_HANDLE,
      "handle");

  /**
   * TpChannel:channel-ready:
   *
   * Initially %FALSE; changes to %TRUE when introspection of the channel
   * has finished and it's ready for use.
   *
   * By the time this property becomes %TRUE, the #TpChannel:channel-type,
   * #TpChannel:handle-type and #TpChannel:handle properties will have been
   * set (if introspection did not fail), and any extra interfaces will
   * have been set up.
   */
  param_spec = g_param_spec_boolean ("channel-ready", "Channel ready?",
      "Initially FALSE; changes to TRUE when introspection finishes", FALSE,
      G_PARAM_READABLE
      | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_CHANNEL_READY,
      param_spec);

  /**
   * TpChannel:connection:
   *
   * The #TpConnection to which this #TpChannel belongs. Used for e.g.
   * handle manipulation.
   */
  param_spec = g_param_spec_object ("connection", "TpConnection",
      "The connection to which this object belongs.", TP_TYPE_CONNECTION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_CONNECTION,
      param_spec);
}

/**
 * tp_channel_new:
 * @conn: a connection; may not be %NULL
 * @object_path: the object path of the channel; may not be %NULL
 * @optional_channel_type: the channel type if already known, or %NULL if not
 * @optional_handle_type: the handle type if already known, or
 *  %TP_UNKNOWN_HANDLE_TYPE if not
 * @optional_handle: the handle if already known, or 0 if not
 *  (if @optional_handle_type is %TP_UNKNOWN_HANDLE_TYPE or
 *  %TP_HANDLE_TYPE_NONE, this must be 0)
 * @error: used to indicate the error if %NULL is returned
 *
 * <!-- -->
 *
 * Returns: a new channel proxy, or %NULL on invalid arguments.
 *
 * Since: 0.7.1
 */
TpChannel *
tp_channel_new (TpConnection *conn,
                const gchar *object_path,
                const gchar *optional_channel_type,
                TpHandleType optional_handle_type,
                TpHandle optional_handle,
                GError **error)
{
  TpChannel *ret = NULL;
  TpProxy *conn_proxy = (TpProxy *) conn;
  gchar *dup = NULL;

  g_return_val_if_fail (conn != NULL, NULL);
  g_return_val_if_fail (object_path != NULL, NULL);

  /* TpConnection always has a unique name, so we can assert this */
  g_assert (tp_dbus_check_valid_bus_name (conn_proxy->bus_name,
        TP_DBUS_NAME_TYPE_UNIQUE, NULL));

  if (!tp_dbus_check_valid_object_path (object_path, error))
    goto finally;

  if (optional_channel_type != NULL &&
      !tp_dbus_check_valid_interface_name (optional_channel_type, error))
    goto finally;

  if (optional_handle_type == TP_UNKNOWN_HANDLE_TYPE ||
      optional_handle_type == TP_HANDLE_TYPE_NONE)
    {
      if (optional_handle != 0)
        {
          /* in the properties, we do actually allow the user to give us an
           * assumed-valid handle of unknown type - but that'd be silly */
          g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
              "Nonzero handle of type NONE or unknown makes no sense");
          goto finally;
        }
    }
  else if (!tp_handle_type_is_valid (optional_handle_type, error))
    {
      goto finally;
    }

  ret = TP_CHANNEL (g_object_new (TP_TYPE_CHANNEL,
        "connection", conn,
        "dbus-daemon", conn_proxy->dbus_daemon,
        "bus-name", conn_proxy->bus_name,
        "object-path", object_path,
        "channel-type", optional_channel_type,
        "handle-type", optional_handle_type,
        "handle", optional_handle,
        NULL));

finally:
  g_free (dup);

  return ret;
}

/**
 * tp_channel_run_until_ready:
 * @self: a channel
 * @error: if not %NULL and %FALSE is returned, used to raise an error
 * @loop: if not %NULL, a #GMainLoop is placed here while it is being run
 *  (so calling code can call g_main_loop_quit() to abort), and %NULL is
 *  placed here after the loop has been run
 *
 * If @self is ready for use (introspection has finished, etc.), return
 * immediately. Otherwise, re-enter the main loop until the channel either
 * becomes invalid or becomes ready for use, or until the main loop stored
 * via @loop is cancelled.
 *
 * Returns: %TRUE if the channel has been introspected and is ready for use,
 *  %FALSE if the channel has become invalid.
 *
 * Since: 0.7.1
 */
gboolean
tp_channel_run_until_ready (TpChannel *self,
                            GError **error,
                            GMainLoop **loop)
{
  TpProxy *as_proxy = (TpProxy *) self;
  GMainLoop *my_loop;
  gulong invalidated_id, ready_id;

  if (as_proxy->invalidated)
    goto raise_invalidated;

  if (self->ready)
    return TRUE;

  my_loop = g_main_loop_new (NULL, FALSE);
  invalidated_id = g_signal_connect_swapped (self, "invalidated",
      G_CALLBACK (g_main_loop_quit), my_loop);
  ready_id = g_signal_connect_swapped (self, "notify::channel-ready",
      G_CALLBACK (g_main_loop_quit), my_loop);

  if (loop != NULL)
    *loop = my_loop;

  g_main_loop_run (my_loop);

  if (loop != NULL)
    *loop = NULL;

  g_signal_handler_disconnect (self, invalidated_id);
  g_signal_handler_disconnect (self, ready_id);
  g_main_loop_unref (my_loop);

  if (as_proxy->invalidated)
    goto raise_invalidated;

  g_assert (self->ready);
  return TRUE;

raise_invalidated:
  if (error != NULL)
    {
      g_return_val_if_fail (*error == NULL, FALSE);
      *error = g_error_copy (as_proxy->invalidated);
    }

  return FALSE;
}

typedef struct {
    TpChannelWhenReadyCb callback;
    gpointer user_data;
    gulong invalidated_id;
    gulong ready_id;
} CallWhenReadyContext;

static void
cwr_invalidated (TpChannel *self,
                 guint domain,
                 gint code,
                 gchar *message,
                 gpointer user_data)
{
  CallWhenReadyContext *ctx = user_data;
  GError e = { domain, code, message };

  DEBUG ("enter");

  g_assert (ctx->callback != NULL);

  ctx->callback (self, &e, ctx->user_data);

  g_signal_handler_disconnect (self, ctx->invalidated_id);
  g_signal_handler_disconnect (self, ctx->ready_id);

  ctx->callback = NULL;   /* poison it to detect errors */
  g_slice_free (CallWhenReadyContext, ctx);
}

static void
cwr_ready (TpChannel *self,
           GParamSpec *unused G_GNUC_UNUSED,
           gpointer user_data)
{
  CallWhenReadyContext *ctx = user_data;

  DEBUG ("enter");

  g_assert (ctx->callback != NULL);

  ctx->callback (self, NULL, ctx->user_data);

  g_signal_handler_disconnect (self, ctx->invalidated_id);
  g_signal_handler_disconnect (self, ctx->ready_id);

  ctx->callback = NULL;   /* poison it to detect errors */
  g_slice_free (CallWhenReadyContext, ctx);
}

/**
 * TpChannelWhenReadyCb:
 * @channel: the channel (which may be in the middle of being disposed,
 *  if error is non-%NULL, error->domain is TP_DBUS_ERRORS and error->code is
 *  TP_DBUS_ERROR_PROXY_UNREFERENCED)
 * @error: %NULL if the channel is ready for use, or the error with which
 *  it was invalidated if it is now invalid
 * @user_data: whatever was passed to tp_channel_call_when_ready()
 *
 * Signature of a callback passed to tp_channel_call_when_ready(), which
 * will be called exactly once, when the channel becomes ready or
 * invalid (whichever happens first)
 */

/**
 * tp_channel_call_when_ready:
 * @self: a channel
 * @callback: called when the channel becomes ready or invalidated, whichever
 *  happens first
 * @user_data: arbitrary user-supplied data passed to the callback
 *
 * If @self is ready for use or has been invalidated, call @callback
 * immediately, then return. Otherwise, arrange
 * for @callback to be called when @self either becomes ready for use
 * or becomes invalid.
 *
 * Since: 0.7.7
 */
void
tp_channel_call_when_ready (TpChannel *self,
                            TpChannelWhenReadyCb callback,
                            gpointer user_data)
{
  TpProxy *as_proxy = (TpProxy *) self;

  g_return_if_fail (callback != NULL);

  if (self->ready || as_proxy->invalidated != NULL)
    {
      DEBUG ("already ready or invalidated");
      callback (self, as_proxy->invalidated, user_data);
    }
  else
    {
      CallWhenReadyContext *ctx = g_slice_new (CallWhenReadyContext);

      DEBUG ("arranging callback later");

      ctx->callback = callback;
      ctx->user_data = user_data;
      ctx->invalidated_id = g_signal_connect (self, "invalidated",
          G_CALLBACK (cwr_invalidated), ctx);
      ctx->ready_id = g_signal_connect (self, "notify::channel-ready",
          G_CALLBACK (cwr_ready), ctx);
    }
}

static gpointer
tp_channel_once (gpointer data G_GNUC_UNUSED)
{
  GType type = TP_TYPE_CHANNEL;

  tp_proxy_init_known_interfaces ();

  tp_proxy_or_subclass_hook_on_interface_add (type,
      tp_cli_channel_add_signals);
  tp_proxy_subclass_add_error_mapping (type,
      TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

  return NULL;
}

/**
 * tp_channel_init_known_interfaces:
 *
 * Ensure that the known interfaces for TpChannel have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_CHANNEL.
 *
 * Since: 0.7.6
 */
void
tp_channel_init_known_interfaces (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, tp_channel_once, NULL);
}
