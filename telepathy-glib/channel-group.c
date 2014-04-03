/*
 * channel-group.c - proxy for a Telepathy channel (GROUP feature)
 *
 * Copyright (C) 2011 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include "telepathy-glib/channel-internal.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/asv.h>
#include <telepathy-glib/cli-channel.h>
#include <telepathy-glib/cli-misc.h>
#include <telepathy-glib/client-factory.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/value-array.h>

#define DEBUG_FLAG TP_DEBUG_GROUPS
#include "telepathy-glib/connection-internal.h"
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/util-internal.h"

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

static GArray *
dup_handle_array (const GArray *source)
{
  GArray *target;

  target = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), source->len);
  g_array_append_vals (target, source->data, source->len);

  return target;
}

static TpContact *
dup_contact (TpChannel *self,
    TpHandle handle,
    GHashTable *identifiers)
{
  const gchar *id;

  if (handle == 0)
    return NULL;

  id = g_hash_table_lookup (identifiers, GUINT_TO_POINTER (handle));
  if (id == NULL)
    {
      DEBUG ("Missing identifier for handle %u - broken CM", handle);
      return NULL;
    }

  return tp_client_factory_ensure_contact (
      tp_proxy_get_factory (self->priv->connection), self->priv->connection,
      handle, id);
}

static GPtrArray *
dup_contact_array (TpChannel *self,
    const GArray *handles,
    GHashTable *identifiers)
{
  GPtrArray *array;
  guint i;

  array = g_ptr_array_new_full (handles->len, g_object_unref);
  if (handles == NULL)
    return array;

  for (i = 0; i < handles->len; i++)
    {
      TpHandle handle = g_array_index (handles, TpHandle, i);
      TpContact *contact = dup_contact (self, handle, identifiers);

      if (contact != NULL)
        g_ptr_array_add (array, contact);
    }

  return array;
}

static GHashTable *
dup_contacts_table (TpChannel *self,
    const GArray *handles,
    GHashTable *identifiers)
{
  GHashTable *target;
  guint i;

  target = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  if (handles == NULL)
    return target;

  for (i = 0; i < handles->len; i++)
    {
      TpHandle handle = g_array_index (handles, TpHandle, i);
      TpContact *contact = dup_contact (self, handle, identifiers);

      if (contact != NULL)
        g_hash_table_insert (target, GUINT_TO_POINTER (handle), contact);
    }

  return target;
}

/* self->priv->group_contact_owners may contain NULL TpContact and
 * g_object_unref isn't NULL safe */
static void
safe_g_object_unref (gpointer data)
{
  if (data == NULL)
    return;

  g_object_unref (data);
}

static gpointer
safe_g_object_ref (gpointer data)
{
  if (data == NULL)
    return NULL;

  return g_object_ref (data);
}

static GHashTable *
dup_owners_table (TpChannel *self,
    GHashTable *source,
    GHashTable *identifiers)
{
  GHashTable *target;
  GHashTableIter iter;
  gpointer key, value;

  target = g_hash_table_new_full (NULL, NULL, NULL, safe_g_object_unref);
  if (source == NULL)
    return target;

  g_hash_table_iter_init (&iter, source);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      TpHandle owner_handle = GPOINTER_TO_UINT (value);
      TpContact *contact = dup_contact (self, owner_handle, identifiers);

      g_hash_table_insert (target, key, contact);
    }

  return target;
}

static void process_contacts_queue (TpChannel *self);

static void
contacts_queue_upgraded_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannel *self = user_data;
  TpClientFactory *factory = (TpClientFactory *) object;
  GSimpleAsyncResult *my_result = self->priv->current_contacts_queue_result;
  GError *error = NULL;

  if (!tp_client_factory_upgrade_contacts_finish (factory, result, NULL,
          &error))
    {
      DEBUG ("Error preparing channel contacts: %s", error->message);
      g_simple_async_result_take_error (my_result, error);
    }

  g_simple_async_result_complete (my_result);

  self->priv->current_contacts_queue_result = NULL;
  process_contacts_queue (self);

  g_object_unref (my_result);
}

static void
process_contacts_queue (TpChannel *self)
{
  GSimpleAsyncResult *result;
  GPtrArray *contacts;
  const GError *error = NULL;

  if (self->priv->current_contacts_queue_result != NULL)
    return;

  /* self can't die while there are queued results because GSimpleAsyncResult
   * keep a ref to it. But it could have been invalidated. */
  error = tp_proxy_get_invalidated (self);
  if (error != NULL)
    {
      g_object_ref (self);
      while ((result = g_queue_pop_head (self->priv->contacts_queue)) != NULL)
        {
          g_simple_async_result_set_from_error (result, error);
          g_simple_async_result_complete (result);
          g_object_unref (result);
        }
      g_object_unref (self);

      return;
    }

next:
  result = g_queue_pop_head (self->priv->contacts_queue);
  if (result == NULL)
    return;

  contacts = g_simple_async_result_get_op_res_gpointer (result);
  if (contacts == NULL || contacts->len == 0)
    {
      /* It can happen there is no contact to prepare, and can still be useful
       * in order to not reorder some events.
       * We have to use an idle though, to guarantee callback is never called
       * without reentering mainloop first. */
      g_simple_async_result_complete_in_idle (result);
      g_object_unref (result);
      goto next;
    }

  self->priv->current_contacts_queue_result = result;
  tp_client_factory_upgrade_contacts_async (
      tp_proxy_get_factory (self->priv->connection),
      self->priv->connection,
      contacts->len, (TpContact **) contacts->pdata,
      contacts_queue_upgraded_cb,
      self);
}

void
_tp_channel_contacts_queue_prepare_async (TpChannel *self,
    GPtrArray *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new ((GObject *) self, callback, user_data,
      _tp_channel_contacts_queue_prepare_async);

  if (contacts != NULL)
    {
      g_simple_async_result_set_op_res_gpointer (result,
          g_ptr_array_ref (contacts), (GDestroyNotify) g_ptr_array_unref);
    }

  g_queue_push_tail (self->priv->contacts_queue, result);
  process_contacts_queue (self);
}

static gpointer
safe_g_ptr_array_ref (GPtrArray *array)
{
  return (array != NULL) ? g_ptr_array_ref (array) : NULL;
}

gboolean
_tp_channel_contacts_queue_prepare_finish (TpChannel *self,
    GAsyncResult *result,
    GPtrArray **contacts,
    GError **error)
{
  _tp_implement_finish_copy_pointer (self,
      _tp_channel_contacts_queue_prepare_async,
      safe_g_ptr_array_ref, contacts);
}

static void
local_pending_info_free (LocalPendingInfo *info)
{
  g_free (info->message);
  g_clear_object (&info->actor_contact);
  g_slice_free (LocalPendingInfo, info);
}

static void
set_local_pending_info (TpChannel *self,
    TpContact *contact,
    TpContact *actor,
    TpChannelGroupChangeReason reason,
    const gchar *message)
{
  LocalPendingInfo *info;

  if (tp_str_empty (message))
    message = NULL;

  if (actor == NULL && message == NULL &&
      reason == TP_CHANNEL_GROUP_CHANGE_REASON_NONE)
    {
      /* we just don't bother storing informationless local-pending */
      g_hash_table_remove (self->priv->group_local_pending_info,
          GUINT_TO_POINTER (tp_contact_get_handle (contact)));
      return;
    }

  info = g_slice_new0 (LocalPendingInfo);
  info->actor_contact = safe_g_object_ref (actor);
  info->reason = reason;
  info->message = g_strdup (message);

  g_hash_table_insert (self->priv->group_local_pending_info,
      GUINT_TO_POINTER (tp_contact_get_handle (contact)), info);
}

/*
 * If the @group_remove_error is derived from a TpChannelGroupChangeReason,
 * attempt to rewrite it into a TpError.
 */
static void
_tp_channel_group_improve_remove_error (TpChannel *self,
    TpContact *actor)
{
  GError *error = self->priv->group_remove_error;

  if (error == NULL || error->domain != TP_ERRORS_REMOVED_FROM_GROUP)
    return;

  switch (error->code)
    {
    case TP_CHANNEL_GROUP_CHANGE_REASON_NONE:
      if (actor == self->priv->group_self_contact ||
          actor == tp_connection_get_self_contact (self->priv->connection))
        {
          error->code = TP_ERROR_CANCELLED;
        }
      else
        {
          error->code = TP_ERROR_TERMINATED;
        }
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_OFFLINE:
      error->code = TP_ERROR_OFFLINE;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_KICKED:
      error->code = TP_ERROR_CHANNEL_KICKED;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_BUSY:
      error->code = TP_ERROR_BUSY;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_INVITED:
      DEBUG ("%s: Channel_Group_Change_Reason_Invited makes no sense as a "
          "removal reason!", tp_proxy_get_object_path (self));
      error->domain = TP_DBUS_ERRORS;
      error->code = TP_DBUS_ERROR_INCONSISTENT;
      return;

    case TP_CHANNEL_GROUP_CHANGE_REASON_BANNED:
      error->code = TP_ERROR_CHANNEL_BANNED;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_ERROR:
      /* hopefully all CMs that use this will also give us an error detail,
       * but if they didn't, or gave us one we didn't understand... */
      error->code = TP_ERROR_NOT_AVAILABLE;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_INVALID_CONTACT:
      error->code = TP_ERROR_DOES_NOT_EXIST;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_NO_ANSWER:
      error->code = TP_ERROR_NO_ANSWER;
      break;

    /* TP_CHANNEL_GROUP_CHANGE_REASON_RENAMED shouldn't be the last error
     * seen in the channel - we'll get removed again with a real reason,
     * later, so there's no point in doing anything special with this one */

    case TP_CHANNEL_GROUP_CHANGE_REASON_PERMISSION_DENIED:
      error->code = TP_ERROR_PERMISSION_DENIED;
      break;

    case TP_CHANNEL_GROUP_CHANGE_REASON_SEPARATED:
      DEBUG ("%s: Channel_Group_Change_Reason_Separated makes no sense as a "
          "removal reason!", tp_proxy_get_object_path (self));
      error->domain = TP_DBUS_ERRORS;
      error->code = TP_DBUS_ERROR_INCONSISTENT;
      return;

    /* all values up to and including Separated have been checked */

    default:
      /* We don't understand this reason code, so keeping the domain and code
       * the same (i.e. using TP_ERRORS_REMOVED_FROM_GROUP) is no worse than
       * anything else we could do. */
      return;
    }

  /* If we changed the code we also need to change the domain; if not, we did
   * an early return, so we'll never reach this */
  error->domain = TP_ERROR;
}

typedef struct
{
  GPtrArray *added;
  GArray *removed;
  GPtrArray *local_pending;
  GPtrArray *remote_pending;
  TpContact *actor;
  GHashTable *details;
} MembersChangedData;

static void
members_changed_data_free (MembersChangedData *data)
{
  tp_clear_pointer (&data->added, g_ptr_array_unref);
  tp_clear_pointer (&data->removed, g_array_unref);
  tp_clear_pointer (&data->local_pending, g_ptr_array_unref);
  tp_clear_pointer (&data->remote_pending, g_ptr_array_unref);
  g_clear_object (&data->actor);
  tp_clear_pointer (&data->details, g_hash_table_unref);

  g_slice_free (MembersChangedData, data);
}

static void
members_changed_prepared_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannel *self = (TpChannel *) object;
  MembersChangedData *data = user_data;
  TpChannelGroupChangeReason reason;
  const gchar *message;
  GPtrArray *removed;
  guint i;

  _tp_channel_contacts_queue_prepare_finish (self, result, NULL, NULL);

  reason = tp_asv_get_uint32 (data->details, "change-reason", NULL);
  message = tp_asv_get_string (data->details, "message");

  for (i = 0; i < data->added->len; i++)
    {
      TpContact *contact = g_ptr_array_index (data->added, i);
      gpointer key = GUINT_TO_POINTER (tp_contact_get_handle (contact));

      g_hash_table_insert (self->priv->group_members, key,
          g_object_ref (contact));
      g_hash_table_remove (self->priv->group_local_pending, key);
      g_hash_table_remove (self->priv->group_local_pending_info, key);
      g_hash_table_remove (self->priv->group_remote_pending, key);
    }

  for (i = 0; i < data->local_pending->len; i++)
    {
      TpContact *contact = g_ptr_array_index (data->local_pending, i);
      gpointer key = GUINT_TO_POINTER (tp_contact_get_handle (contact));

      g_hash_table_remove (self->priv->group_members, key);
      g_hash_table_insert (self->priv->group_local_pending, key,
          g_object_ref (contact));
      g_hash_table_remove (self->priv->group_remote_pending, key);

      /* Special-case renaming a local-pending contact, if the
       * signal is spec-compliant. Keep the old actor/reason/message in
       * this case */
      if (reason == TP_CHANNEL_GROUP_CHANGE_REASON_RENAMED &&
          data->added->len == 0 &&
          data->local_pending->len == 1 &&
          data->remote_pending->len == 0 &&
          data->removed->len == 1)
        {
          TpHandle old = g_array_index (data->removed, TpHandle, 0);
          LocalPendingInfo *info = g_hash_table_lookup (
              self->priv->group_local_pending_info,
              GUINT_TO_POINTER (old));

          if (info != NULL)
            {
              set_local_pending_info (self, contact,
                  info->actor_contact, info->reason, info->message);
              continue;
            }
        }

      /* not reached if the Renamed special case occurred */
      set_local_pending_info (self, contact, data->actor, reason, message);
    }

  for (i = 0; i < data->remote_pending->len; i++)
    {
      TpContact *contact = g_ptr_array_index (data->remote_pending, i);
      gpointer key = GUINT_TO_POINTER (tp_contact_get_handle (contact));

      g_hash_table_remove (self->priv->group_members, key);
      g_hash_table_remove (self->priv->group_local_pending, key);
      g_hash_table_remove (self->priv->group_local_pending_info, key);
      g_hash_table_insert (self->priv->group_remote_pending, key,
          g_object_ref (contact));
    }

  /* For removed contacts, we have only handles because we are supposed to
   * already know them. So we have to search them in our tables, construct an
   * array of removed contacts and then remove them from our tables */
  removed = g_ptr_array_new_full (data->removed->len, g_object_unref);
  for (i = 0; i < data->removed->len; i++)
    {
      TpHandle handle = g_array_index (data->removed, TpHandle, i);
      gpointer key = GUINT_TO_POINTER (handle);
      TpContact *contact;

      contact = g_hash_table_lookup (self->priv->group_members, key);
      if (contact == NULL)
          contact = g_hash_table_lookup (
              self->priv->group_local_pending, key);
      if (contact == NULL)
          contact = g_hash_table_lookup (
              self->priv->group_remote_pending, key);

      if (contact == NULL)
        {
          DEBUG ("Handle %u removed but not found in our tables - broken CM",
              handle);
          continue;
        }

      g_ptr_array_add (removed, g_object_ref (contact));

      g_hash_table_remove (self->priv->group_members, key);
      g_hash_table_remove (self->priv->group_local_pending, key);
      g_hash_table_remove (self->priv->group_local_pending_info, key);
      g_hash_table_remove (self->priv->group_remote_pending, key);

      if (contact == self->priv->group_self_contact ||
          contact == tp_connection_get_self_contact (self->priv->connection))
        {
          const gchar *error_detail = tp_asv_get_string (data->details,
              "error");
          const gchar *debug_message = tp_asv_get_string (data->details,
              "debug-message");

          if (debug_message == NULL && !tp_str_empty (message))
            debug_message = message;

          if (debug_message == NULL && error_detail != NULL)
            debug_message = error_detail;

          if (debug_message == NULL)
            debug_message = "(no message provided)";

          if (self->priv->group_remove_error != NULL)
            g_clear_error (&self->priv->group_remove_error);

          if (error_detail != NULL)
            {
              DEBUG ("detailed error: %s", error_detail);

              /* CM specified a D-Bus error name */
              tp_proxy_dbus_error_to_gerror (self, error_detail,
                  debug_message, &self->priv->group_remove_error);

              DEBUG ("-> %s #%d: %s",
                  g_quark_to_string (self->priv->group_remove_error->domain),
                  self->priv->group_remove_error->code,
                  self->priv->group_remove_error->message);

              /* ... but if we don't know anything about that D-Bus error
               * name, we can still do better by using RemovedFromGroup */
              if (g_error_matches (self->priv->group_remove_error,
                    G_IO_ERROR, G_IO_ERROR_DBUS_ERROR))
                {
                  self->priv->group_remove_error->domain =
                    TP_ERRORS_REMOVED_FROM_GROUP;
                  self->priv->group_remove_error->code = reason;

                  _tp_channel_group_improve_remove_error (self, data->actor);
                }
            }
          else
            {
              DEBUG ("no detailed error");

              /* Use our separate error domain */
              g_set_error_literal (&self->priv->group_remove_error,
                  TP_ERRORS_REMOVED_FROM_GROUP, reason, debug_message);

              _tp_channel_group_improve_remove_error (self, data->actor);
            }
        }
    }

  g_signal_emit_by_name (self, "group-members-changed", data->added,
      removed, data->local_pending, data->remote_pending, data->actor,
      data->details);

  g_ptr_array_unref (removed);
  members_changed_data_free (data);
}

static void
members_changed_cb (TpChannel *self,
    const GArray *added,
    const GArray *removed,
    const GArray *local_pending,
    const GArray *remote_pending,
    GHashTable *details,
    gpointer user_data,
    GObject *weak_object)
{
  MembersChangedData *data;
  GPtrArray *contacts;
  GHashTable *ids;
  TpHandle actor;

  if (!self->priv->group_properties_retrieved)
    return;

  actor = tp_asv_get_uint32 (details, "actor", NULL);

  ids = tp_asv_get_boxed (details, "contact-ids",
      TP_HASH_TYPE_HANDLE_IDENTIFIER_MAP);
  if (ids == NULL && (added->len > 0 || local_pending->len > 0 ||
      remote_pending->len > 0 || actor != 0 ))
    {
      DEBUG ("CM did not give identifiers, can't create TpContact");
      return;
    }

  /* Ensure all TpContact, and push to a queue. This is to ensure that signals
   * does not get reordered while we prepare them. */
  data = g_slice_new (MembersChangedData);
  data->added = dup_contact_array (self, added, ids);
  data->removed = dup_handle_array (removed);
  data->local_pending = dup_contact_array (self, local_pending, ids);
  data->remote_pending = dup_contact_array (self, remote_pending, ids);
  data->actor = dup_contact (self, actor, ids);
  data->details = g_hash_table_ref (details);

  contacts = g_ptr_array_new ();
  tp_g_ptr_array_extend (contacts, data->added);
  tp_g_ptr_array_extend (contacts, data->local_pending);
  tp_g_ptr_array_extend (contacts, data->remote_pending);
  if (data->actor != NULL)
    g_ptr_array_add (contacts, data->actor);

  _tp_channel_contacts_queue_prepare_async (self, contacts,
      members_changed_prepared_cb, data);

  g_ptr_array_unref (contacts);
}

typedef struct
{
  GHashTable *added;
  GArray *removed;
} HandleOwnersChangedData;

static void
handle_owners_changed_data_free (HandleOwnersChangedData *data)
{
  tp_clear_pointer (&data->added, g_hash_table_unref);
  tp_clear_pointer (&data->removed, g_array_unref);

  g_slice_free (HandleOwnersChangedData, data);
}

static void
handle_owners_changed_prepared_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannel *self = (TpChannel *) object;
  HandleOwnersChangedData *data = user_data;
  guint i;

  _tp_channel_contacts_queue_prepare_finish (self, result, NULL, NULL);

  for (i = 0; i < data->removed->len; i++)
    {
      g_hash_table_remove (self->priv->group_contact_owners,
          GUINT_TO_POINTER (g_array_index (data->removed, TpHandle, i)));
    }

  tp_g_hash_table_update (self->priv->group_contact_owners, data->added, NULL,
      safe_g_object_ref);

  handle_owners_changed_data_free (data);
}

static void
handle_owners_changed_cb (TpChannel *self,
    GHashTable *added,
    const GArray *removed,
    GHashTable *identifiers,
    gpointer user_data,
    GObject *weak_object)
{
  HandleOwnersChangedData *data;
  GPtrArray *contacts;

  if (!self->priv->group_properties_retrieved)
    return;

  data = g_slice_new (HandleOwnersChangedData);
  data->added = dup_owners_table (self, added, identifiers);
  data->removed = dup_handle_array (removed);

  contacts = _tp_contacts_from_values (data->added);

  _tp_channel_contacts_queue_prepare_async (self, contacts,
      handle_owners_changed_prepared_cb, data);

  g_ptr_array_unref (contacts);
}

static void
self_contact_changed_prepared_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannel *self = (TpChannel *) object;
  TpContact *contact = user_data;

  _tp_channel_contacts_queue_prepare_finish (self, result, NULL, NULL);

  g_clear_object (&self->priv->group_self_contact);
  self->priv->group_self_contact = contact;

  g_object_notify ((GObject *) self, "group-self-contact");
}

static void
self_contact_changed_cb (TpChannel *self,
    guint self_handle,
    const gchar *identifier,
    gpointer user_data,
    GObject *weak_object)
{
  TpContact *contact;
  GPtrArray *contacts;

  if (!self->priv->group_properties_retrieved)
    return;

  contacts = g_ptr_array_new_with_free_func (g_object_unref);
  contact = tp_client_factory_ensure_contact (
      tp_proxy_get_factory (self->priv->connection), self->priv->connection,
      self_handle, identifier);
  g_ptr_array_add (contacts, g_object_ref (contact));

  _tp_channel_contacts_queue_prepare_async (self, contacts,
      self_contact_changed_prepared_cb, contact);

  g_ptr_array_unref (contacts);
}

static void
group_flags_changed_cb (TpChannel *self,
    guint added,
    guint removed,
    gpointer user_data,
    GObject *weak_object)
{
  if (!self->priv->group_properties_retrieved)
    return;

  DEBUG ("%p GroupFlagsChanged: +%u -%u", self, added, removed);

  added &= ~(self->priv->group_flags);
  removed &= self->priv->group_flags;

  DEBUG ("%p GroupFlagsChanged (after filtering): +%u -%u",
      self, added, removed);

  self->priv->group_flags |= added;
  self->priv->group_flags &= ~removed;

  if (added != 0 || removed != 0)
    {
      g_object_notify ((GObject *) self, "group-flags");
      g_signal_emit_by_name (self, "group-flags-changed", added, removed);
    }
}

static void
contacts_prepared_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpChannel *self = (TpChannel *) object;
  GSimpleAsyncResult *result = user_data;
  GError *error = NULL;

  if (!_tp_channel_contacts_queue_prepare_finish (self, res, NULL, &error))
    g_simple_async_result_take_error (result, error);

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

static void
append_contacts (GPtrArray *contacts,
    GHashTable *table)
{
  GHashTableIter iter;
  gpointer value;

  if (table == NULL)
      return;

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      if (value == NULL)
        continue;

      g_ptr_array_add (contacts, value);
    }
}

static void
set_local_pending (TpChannel *self,
    const GPtrArray *info,
    GHashTable *identifiers)
{
  guint i;

  self->priv->group_local_pending = g_hash_table_new_full (NULL, NULL,
      NULL, g_object_unref);
  self->priv->group_local_pending_info = g_hash_table_new_full (NULL, NULL,
      NULL, (GDestroyNotify) local_pending_info_free);

  if (info == NULL)
    return;

  for (i = 0; i < info->len; i++)
    {
      GValueArray *va = g_ptr_array_index (info, i);
      TpHandle handle;
      TpHandle actor;
      TpChannelGroupChangeReason reason;
      const gchar *message;
      TpContact *contact;
      TpContact *actor_contact;

      tp_value_array_unpack (va, 4, &handle, &actor, &reason, &message);

      contact = dup_contact (self, handle, identifiers);
      if (contact == NULL)
        continue;

      g_hash_table_insert (self->priv->group_local_pending,
          GUINT_TO_POINTER (handle), contact);

      actor_contact = dup_contact (self, actor, identifiers);
      set_local_pending_info (self, contact, actor_contact, reason, message);
      g_clear_object (&actor_contact);
    }
}

static void
got_group_properties_cb (TpProxy *proxy,
    GHashTable *asv,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  static GType au_type = 0;
  TpChannel *self = TP_CHANNEL (proxy);
  GSimpleAsyncResult *result = user_data;
  GHashTable *identifiers;
  GHashTableIter iter;
  gpointer value;
  GPtrArray *contacts;

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (result, error);
      g_simple_async_result_complete (result);
      return;
    }

  if (G_UNLIKELY (au_type == 0))
    {
      au_type = dbus_g_type_get_collection ("GArray", G_TYPE_UINT);
    }

  DEBUG ("Received %u group properties", g_hash_table_size (asv));
  self->priv->group_properties_retrieved = TRUE;

  self->priv->group_flags = tp_asv_get_uint32 (asv, "GroupFlags", NULL);

  identifiers = tp_asv_get_boxed (asv, "MemberIdentifiers",
      TP_HASH_TYPE_HANDLE_IDENTIFIER_MAP);

  if (identifiers == NULL)
    {
      g_simple_async_result_set_error (result, TP_ERROR,
          TP_ERROR_INVALID_ARGUMENT,
          "Failed to get MemberIdentifiers property from Group interface");
      g_simple_async_result_complete (result);
      return;
    }

  self->priv->group_self_contact = dup_contact (self,
      tp_asv_get_uint32 (asv, "SelfHandle", NULL),
      identifiers);

  self->priv->group_members = dup_contacts_table (self,
      tp_asv_get_boxed (asv, "Members", au_type),
      identifiers);

  set_local_pending (self,
      tp_asv_get_boxed (asv, "LocalPendingMembers",
          TP_ARRAY_TYPE_LOCAL_PENDING_INFO_LIST),
      identifiers);

  self->priv->group_remote_pending = dup_contacts_table (self,
      tp_asv_get_boxed (asv, "RemotePendingMembers", au_type),
      identifiers);

  self->priv->group_contact_owners = dup_owners_table (self,
      tp_asv_get_boxed (asv, "HandleOwners", TP_HASH_TYPE_HANDLE_OWNER_MAP),
      identifiers);

  contacts = g_ptr_array_new ();

  /* Collect all the TpContacts we have for this channel */
  if (self->priv->group_self_contact != NULL)
    g_ptr_array_add (contacts, self->priv->group_self_contact);

  append_contacts (contacts, self->priv->group_members);
  append_contacts (contacts, self->priv->group_local_pending);
  append_contacts (contacts, self->priv->group_remote_pending);
  append_contacts (contacts, self->priv->group_contact_owners);

  g_hash_table_iter_init (&iter, self->priv->group_local_pending_info);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      LocalPendingInfo *info = value;

      if (info->actor_contact != NULL)
        g_ptr_array_add (contacts, info->actor_contact);
    }

  _tp_channel_contacts_queue_prepare_async (self, contacts,
      contacts_prepared_cb, g_object_ref (result));

  g_ptr_array_unref (contacts);
}

void
_tp_channel_group_prepare_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpChannel *self = (TpChannel *) proxy;
  GSimpleAsyncResult *result;
  GError *error = NULL;

  if (!tp_proxy_has_interface_by_id (self,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_GROUP1))
    {
      g_simple_async_report_error_in_idle ((GObject *) self, callback,
          user_data, TP_ERROR, TP_ERROR_NOT_CAPABLE,
          "Channel has no GROUP interface");
      return;
    }

  tp_cli_channel_interface_group1_connect_to_group_flags_changed (self,
      group_flags_changed_cb, NULL, NULL, NULL, &error);
  g_assert_no_error (error);

  tp_cli_channel_interface_group1_connect_to_self_contact_changed (self,
      self_contact_changed_cb, NULL, NULL, NULL, &error);
  g_assert_no_error (error);

  tp_cli_channel_interface_group1_connect_to_members_changed (self,
      members_changed_cb, NULL, NULL, NULL, &error);
  g_assert_no_error (error);

  tp_cli_channel_interface_group1_connect_to_handle_owners_changed (self,
      handle_owners_changed_cb, NULL, NULL, NULL, &error);
  g_assert_no_error (error);

  result = g_simple_async_result_new ((GObject *) self, callback, user_data,
      _tp_channel_group_prepare_async);

  tp_cli_dbus_properties_call_get_all (self, -1,
      TP_IFACE_CHANNEL_INTERFACE_GROUP1, got_group_properties_cb,
      result, g_object_unref, NULL);
}

/**
 * tp_channel_group_get_flags:
 * @self: a channel
 *
 * <!-- -->
 *
 * Returns: the value of #TpChannel:group-flags
 * Since: 0.99.1
 */
TpChannelGroupFlags
tp_channel_group_get_flags (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), 0);

  return self->priv->group_flags;
}

/**
 * tp_channel_group_get_self_contact:
 * @self: a channel
 *
 * <!-- -->
 *
 * Returns: (transfer none): the value of #TpChannel:group-self-contact
 * Since: 0.99.1
 */
TpContact *
tp_channel_group_get_self_contact (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return self->priv->group_self_contact;
}

/**
 * tp_channel_group_dup_members:
 * @self: a channel
 *
 * If @self is a group and the %TP_CHANNEL_FEATURE_GROUP feature has been
 * prepared, return a #GPtrArray containing its members.
 *
 * If @self is a group but %TP_CHANNEL_FEATURE_GROUP has not been prepared,
 * the result may either be a set of members, or %NULL.
 *
 * If @self is not a group, return %NULL.
 *
 * Returns: (transfer container) (type GLib.PtrArray) (element-type TelepathyGLib.Contact):
 *  a new #GPtrArray of #TpContact, free it with g_ptr_array_unref(), or %NULL.
 *
 * Since: 0.99.1
 */
GPtrArray *
tp_channel_group_dup_members (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return _tp_contacts_from_values (self->priv->group_members);
}

/**
 * tp_channel_group_dup_local_pending:
 * @self: a channel
 *
 * If @self is a group and the %TP_CHANNEL_FEATURE_GROUP feature has been
 * prepared, return a #GPtrArray containing its local-pending members.
 *
 * If @self is a group but %TP_CHANNEL_FEATURE_GROUP has not been prepared,
 * the result may either be a set of local-pending members, or %NULL.
 *
 * If @self is not a group, return %NULL.
 *
 * Returns: (transfer container) (type GLib.PtrArray) (element-type TelepathyGLib.Contact):
 *  a new #GPtrArray of #TpContact, free it with g_ptr_array_unref(), or %NULL.
 *
 * Since: 0.99.1
 */
GPtrArray *
tp_channel_group_dup_local_pending (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return _tp_contacts_from_values (self->priv->group_local_pending);
}

/**
 * tp_channel_group_dup_remote_pending:
 * @self: a channel
 *
 * If @self is a group and the %TP_CHANNEL_FEATURE_GROUP feature has been
 * prepared, return a #GPtrArray containing its remote-pending members.
 *
 * If @self is a group but %TP_CHANNEL_FEATURE_GROUP has not been prepared,
 * the result may either be a set of remote-pending members, or %NULL.
 *
 * If @self is not a group, return %NULL.
 *
 * Returns: (transfer container) (type GLib.PtrArray) (element-type TelepathyGLib.Contact):
 *  a new #GPtrArray of #TpContact, free it with g_ptr_array_unref(), or %NULL.
 *
 * Since: 0.99.1
 */
GPtrArray *
tp_channel_group_dup_remote_pending (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return _tp_contacts_from_values (self->priv->group_remote_pending);
}

/**
 * tp_channel_group_get_local_pending_info:
 * @self: a channel
 * @local_pending: the #TpContact of a local-pending contact about whom more
 *  information is needed
 * @actor: (out) (allow-none) (transfer none): either %NULL or a location to
 *  return the contact who requested the change
 * @reason: (out) (allow-none): either %NULL or a location to return the reason
 *  for the change
 * @message: (out) (allow-none) (transfer none): either %NULL or a location to
 *  return the user-supplied message
 *
 * If @local_pending is actually a local-pending contact,
 * write additional information into @actor, @reason and @message and return
 * %TRUE. The contact and message are not referenced or copied, and can only be
 * assumed to remain valid until the main loop is re-entered.
 *
 * If @local_pending is not the handle of a local-pending contact,
 * write %NULL into @actor, %TP_CHANNEL_GROUP_CHANGE_REASON_NONE into @reason
 * and "" into @message, and return %FALSE.
 *
 * Returns: %TRUE if the contact is in fact local-pending
 * Since: 0.99.1
 */
gboolean
tp_channel_group_get_local_pending_info (TpChannel *self,
    TpContact *local_pending,
    TpContact **actor,
    TpChannelGroupChangeReason *reason,
    const gchar **message)
{
  gboolean ret = FALSE;
  TpContact *a = NULL;
  TpChannelGroupChangeReason r = TP_CHANNEL_GROUP_CHANGE_REASON_NONE;
  const gchar *m = "";

  g_return_val_if_fail (TP_IS_CHANNEL (self), FALSE);
  g_return_val_if_fail (TP_IS_CONTACT (local_pending), FALSE);
  g_return_val_if_fail (tp_contact_get_connection (local_pending) ==
      self->priv->connection, FALSE);

  if (self->priv->group_properties_retrieved)
    {
      gpointer key = GUINT_TO_POINTER (tp_contact_get_handle (local_pending));

      /* it could conceivably be someone who is local-pending */
      ret = g_hash_table_contains (self->priv->group_local_pending,
          key);

      if (ret)
        {
          /* we might even have information about them */
          LocalPendingInfo *info = g_hash_table_lookup (
              self->priv->group_local_pending_info, key);

          if (info != NULL)
            {
              a = info->actor_contact;
              r = info->reason;

              if (info->message != NULL)
                m = info->message;
            }
          /* else we have no info, which means (NULL, NONE, NULL) */
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
 * tp_channel_group_get_contact_owner:
 * @self: a channel
 * @contact: a contact which is a member of this channel
 *
 * Synopsis (see below for further explanation):
 *
 * - if @self is not a group or @contact is not a member of this channel,
 *   result is undefined;
 * - if %TP_CHANNEL_FEATURE_GROUP has not yet been prepared, result is
 *   undefined;
 * - if @self does not have flags that include
 *   %TP_CHANNEL_GROUP_FLAG_PROPERTIES,
 *   result is undefined;
 * - if @contact is channel-specific and its globally valid "owner" is known,
 *   return that owner;
 * - if @contact is channel-specific and its globally valid "owner" is unknown,
 *   return %NULL;
 * - if @contact is globally valid, return @contact itself
 *
 * Some channels (those with flags that include
 * %TP_CHANNEL_GROUP_FLAG_CHANNEL_SPECIFIC_HANDLES) have a concept of
 * "channel-specific contacts". These are contacts that only have meaning within
 * the context of the channel - for instance, in XMPP Multi-User Chat,
 * participants in a chatroom are identified by an in-room JID consisting
 * of the JID of the chatroom plus a local nickname.
 *
 * Depending on the protocol and configuration, it might be possible to find
 * out what globally valid contact (i.e. a contact that you could add to
 * your contact list) "owns" a channel-specific contact. For instance, in
 * most XMPP MUC chatrooms, normal users cannot see what global JID
 * corresponds to an in-room JID, but moderators can.
 *
 * This is further complicated by the fact that channels with channel-specific
 * contacts can sometimes have members with globally valid contacts (for
 * instance, if you invite someone to an XMPP MUC using their globally valid
 * JID, you would expect to see the contact representing that JID in the
 * Group's remote-pending set).
 *
 * Returns: (transfer none): the global contact that owns the given contact,
 *  or %NULL.
 * Since: 0.99.1
 */
TpContact *
tp_channel_group_get_contact_owner (TpChannel *self,
    TpContact *contact)
{
  TpHandle handle;
  gpointer value;

  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);
  g_return_val_if_fail (TP_IS_CONTACT (contact), NULL);
  g_return_val_if_fail (tp_contact_get_connection (contact) ==
      self->priv->connection, NULL);

  if (self->priv->group_contact_owners == NULL)
    {
      /* undefined result - pretending it's global is probably as good as
       * any other behaviour, since we can't know either way */
      return contact;
    }

  handle = tp_contact_get_handle (contact);

  if (g_hash_table_lookup_extended (self->priv->group_contact_owners,
        GUINT_TO_POINTER (handle), NULL, &value))
    {
      /* channel-specific, value is either owner or NULL if unknown */
      return value;
    }
  else
    {
      /* either already globally valid, or not a member */
      return contact;
    }
}
