/*
 * Example channel manager for contact lists
 *
 * Copyright © 2007-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "contact-list-manager.h"

#include <string.h>

#include <dbus/dbus-glib.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/telepathy-glib.h>

/* this array must be kept in sync with the enum
 * ExampleContactListPresence in contact-list-manager.h */
static const TpPresenceStatusSpec _statuses[] = {
      { "offline", TP_CONNECTION_PRESENCE_TYPE_OFFLINE, FALSE, NULL },
      { "unknown", TP_CONNECTION_PRESENCE_TYPE_UNKNOWN, FALSE, NULL },
      { "error", TP_CONNECTION_PRESENCE_TYPE_ERROR, FALSE, NULL },
      { "away", TP_CONNECTION_PRESENCE_TYPE_AWAY, TRUE, NULL },
      { "available", TP_CONNECTION_PRESENCE_TYPE_AVAILABLE, TRUE, NULL },
      { NULL }
};

const TpPresenceStatusSpec *
example_contact_list_presence_statuses (void)
{
  return _statuses;
}

typedef struct {
    gchar *alias;

    guint subscribe:1;
    guint publish:1;
    guint pre_approved:1;
    guint subscribe_requested:1;
    guint publish_requested:1;
    gchar *publish_request;

    /* string borrowed from priv->all_tags => the same pointer */
    GHashTable *tags;
} ExampleContactDetails;

static ExampleContactDetails *
example_contact_details_new (void)
{
  return g_slice_new0 (ExampleContactDetails);
}

static void
example_contact_details_destroy (gpointer p)
{
  ExampleContactDetails *d = p;

  tp_clear_pointer (&d->tags, g_hash_table_unref);

  g_free (d->alias);
  g_free (d->publish_request);
  g_slice_free (ExampleContactDetails, d);
}

static void mutable_contact_list_iface_init (TpMutableContactListInterface *);
static void blockable_contact_list_iface_init (
    TpBlockableContactListInterface *);
static void contact_group_list_iface_init (TpContactGroupListInterface *);
static void mutable_contact_group_list_iface_init (
    TpMutableContactGroupListInterface *);

G_DEFINE_TYPE_WITH_CODE (ExampleContactListManager,
    example_contact_list_manager,
    TP_TYPE_BASE_CONTACT_LIST,
    G_IMPLEMENT_INTERFACE (TP_TYPE_BLOCKABLE_CONTACT_LIST,
      blockable_contact_list_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_CONTACT_GROUP_LIST,
      contact_group_list_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_MUTABLE_CONTACT_GROUP_LIST,
      mutable_contact_group_list_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_MUTABLE_CONTACT_LIST,
      mutable_contact_list_iface_init))

enum
{
  ALIAS_UPDATED,
  PRESENCE_UPDATED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

enum
{
  PROP_SIMULATION_DELAY = 1,
  N_PROPS
};

struct _ExampleContactListManagerPrivate
{
  TpBaseConnection *conn;
  guint simulation_delay;
  TpHandleRepoIface *contact_repo;

  /* g_strdup (group name) => the same pointer */
  GHashTable *all_tags;

  TpHandleSet *contacts;
  /* GUINT_TO_POINTER (handle borrowed from contacts)
   *    => ExampleContactDetails */
  GHashTable *contact_details;

  TpHandleSet *blocked_contacts;

  gulong status_changed_id;
};

static void
example_contact_list_manager_init (ExampleContactListManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EXAMPLE_TYPE_CONTACT_LIST_MANAGER, ExampleContactListManagerPrivate);

  self->priv->contact_details = g_hash_table_new_full (g_direct_hash,
      g_direct_equal, NULL, example_contact_details_destroy);
  self->priv->all_tags = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);

  /* initialized properly in constructed() */
  self->priv->contact_repo = NULL;
  self->priv->contacts = NULL;
  self->priv->blocked_contacts = NULL;
}

static void
example_contact_list_manager_close_all (ExampleContactListManager *self)
{
  tp_clear_pointer (&self->priv->contacts, tp_handle_set_destroy);
  tp_clear_pointer (&self->priv->blocked_contacts, tp_handle_set_destroy);
  tp_clear_pointer (&self->priv->contact_details, g_hash_table_unref);
  /* this must come after freeing contact_details, because the strings are
   * borrowed */
  tp_clear_pointer (&self->priv->all_tags, g_hash_table_unref);

  if (self->priv->status_changed_id != 0)
    {
      g_signal_handler_disconnect (self->priv->conn,
          self->priv->status_changed_id);
      self->priv->status_changed_id = 0;
    }
}

static void
dispose (GObject *object)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (object);

  example_contact_list_manager_close_all (self);

  ((GObjectClass *) example_contact_list_manager_parent_class)->dispose (
    object);
}

static void
get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *pspec)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (object);

  switch (property_id)
    {
    case PROP_SIMULATION_DELAY:
      g_value_set_uint (value, self->priv->simulation_delay);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
set_property (GObject *object,
              guint property_id,
              const GValue *value,
              GParamSpec *pspec)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (object);

  switch (property_id)
    {
    case PROP_SIMULATION_DELAY:
      self->priv->simulation_delay = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static ExampleContactDetails *
lookup_contact (ExampleContactListManager *self,
                TpHandle contact)
{
  return g_hash_table_lookup (self->priv->contact_details,
      GUINT_TO_POINTER (contact));
}

static ExampleContactDetails *
ensure_contact (ExampleContactListManager *self,
                TpHandle contact,
                gboolean *created)
{
  ExampleContactDetails *ret = lookup_contact (self, contact);

  if (ret == NULL)
    {
      tp_handle_set_add (self->priv->contacts, contact);

      ret = example_contact_details_new ();
      ret->alias = g_strdup (tp_handle_inspect (self->priv->contact_repo,
            contact));

      g_hash_table_insert (self->priv->contact_details,
          GUINT_TO_POINTER (contact), ret);

      if (created != NULL)
        *created = TRUE;
    }
  else if (created != NULL)
    {
      *created = FALSE;
    }

  return ret;
}

static gchar *
ensure_tag (ExampleContactListManager *self,
    const gchar *s,
    gboolean emit_signal)
{
  gchar *r = g_hash_table_lookup (self->priv->all_tags, s);

  if (r == NULL)
    {
      g_message ("creating group %s", s);
      r = g_strdup (s);
      g_hash_table_insert (self->priv->all_tags, r, r);

      if (emit_signal)
        tp_base_contact_list_groups_created ((TpBaseContactList *) self,
            &s, 1);
    }

  return r;
}

static void
example_contact_list_manager_create_groups_async (TpBaseContactList *manager,
    const gchar * const *names,
    gsize n,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  gsize i;

  for (i = 0; i < n; i++)
    ensure_tag (self, names[i], FALSE);

  tp_base_contact_list_groups_created (manager, names, n);
  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_create_groups_async);
}

static void
example_contact_list_manager_set_contact_groups_async (
    TpBaseContactList *manager,
    TpHandle contact,
    const gchar * const *names,
    gsize n,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpHandleSet *set = tp_handle_set_new (self->priv->contact_repo);
  gboolean created;
  gsize i;
  ExampleContactDetails *d;
  GPtrArray *old_names, *new_names;
  GHashTableIter iter;
  gpointer k;

  for (i = 0; i < n; i++)
    ensure_tag (self, names[i], FALSE);

  tp_base_contact_list_groups_created (manager, names, n);

  tp_handle_set_add (set, contact);

  d = ensure_contact (self, contact, &created);

  if (created)
    tp_base_contact_list_contacts_changed (manager, set, NULL);

  if (d->tags == NULL)
    d->tags = g_hash_table_new (g_str_hash, g_str_equal);

  old_names = g_ptr_array_sized_new (g_hash_table_size (d->tags));
  new_names = g_ptr_array_sized_new (n);

  for (i = 0; i < n; i++)
    {
      if (g_hash_table_lookup (d->tags, names[i]) == NULL)
        {
          gchar *tag = g_hash_table_lookup (self->priv->all_tags, names[i]);

          g_assert (tag != NULL);   /* already ensured to exist, above */
          g_hash_table_insert (d->tags, tag, tag);
          g_ptr_array_add (new_names, tag);
        }
    }

  g_hash_table_iter_init (&iter, d->tags);

  while (g_hash_table_iter_next (&iter, &k, NULL))
    {
      for (i = 0; i < n; i++)
        {
          if (!tp_strdiff (names[i], k))
            goto next_hash_element;
        }

      /* not found in @names, so remove it */
      g_ptr_array_add (old_names, k);
      g_hash_table_iter_remove (&iter);

next_hash_element:
      continue;
    }

  tp_base_contact_list_groups_changed (manager, set,
      (const gchar * const *) new_names->pdata, new_names->len,
      (const gchar * const *) old_names->pdata, old_names->len);
  g_ptr_array_unref (old_names);
  g_ptr_array_unref (new_names);
  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_set_contact_groups_async);
}

static gboolean
receive_contact_lists (gpointer p)
{
  TpBaseContactList *manager = p;
  ExampleContactListManager *self = p;
  TpHandle handle;
  gchar *cambridge, *montreal, *francophones;
  ExampleContactDetails *d;
  GHashTableIter iter;
  gpointer handle_p;

  if (self->priv->all_tags == NULL)
    {
      /* connection already disconnected, so don't process the
       * "data from the server" */
      return FALSE;
    }

  /* In a real CM we'd have received a contact list from the server at this
   * point. But this isn't a real CM, so we have to make one up... */

  g_message ("Receiving roster from server");

  cambridge = ensure_tag (self, "Cambridge", FALSE);
  montreal = ensure_tag (self, "Montreal", FALSE);
  francophones = ensure_tag (self, "Francophones", FALSE);

  /* Add various people who are already subscribing and publishing */

  handle = tp_handle_ensure (self->priv->contact_repo, "sjoerd@example.com",
      NULL, NULL);
  d = ensure_contact (self, handle, NULL);
  g_free (d->alias);
  d->alias = g_strdup ("Sjoerd");
  d->subscribe = TRUE;
  d->publish = TRUE;
  d->tags = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (d->tags, cambridge, cambridge);
  tp_handle_unref (self->priv->contact_repo, handle);

  handle = tp_handle_ensure (self->priv->contact_repo, "guillaume@example.com",
      NULL, NULL);
  d = ensure_contact (self, handle, NULL);
  g_free (d->alias);
  d->alias = g_strdup ("Guillaume");
  d->subscribe = TRUE;
  d->publish = TRUE;
  d->tags = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (d->tags, cambridge, cambridge);
  g_hash_table_insert (d->tags, francophones, francophones);
  tp_handle_unref (self->priv->contact_repo, handle);

  handle = tp_handle_ensure (self->priv->contact_repo, "olivier@example.com",
      NULL, NULL);
  d = ensure_contact (self, handle, NULL);
  g_free (d->alias);
  d->alias = g_strdup ("Olivier");
  d->subscribe = TRUE;
  d->publish = TRUE;
  d->tags = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (d->tags, montreal, montreal);
  g_hash_table_insert (d->tags, francophones, francophones);
  tp_handle_unref (self->priv->contact_repo, handle);

  handle = tp_handle_ensure (self->priv->contact_repo, "travis@example.com",
      NULL, NULL);
  d = ensure_contact (self, handle, NULL);
  g_free (d->alias);
  d->alias = g_strdup ("Travis");
  d->subscribe = TRUE;
  d->publish = TRUE;
  tp_handle_unref (self->priv->contact_repo, handle);

  /* Add a couple of people whose presence we've requested. They are
   * remote-pending in subscribe */

  handle = tp_handle_ensure (self->priv->contact_repo, "geraldine@example.com",
      NULL, NULL);
  d = ensure_contact (self, handle, NULL);
  g_free (d->alias);
  d->alias = g_strdup ("Géraldine");
  d->subscribe_requested = TRUE;
  d->tags = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (d->tags, cambridge, cambridge);
  g_hash_table_insert (d->tags, francophones, francophones);
  tp_handle_unref (self->priv->contact_repo, handle);

  handle = tp_handle_ensure (self->priv->contact_repo, "helen@example.com",
      NULL, NULL);
  d = ensure_contact (self, handle, NULL);
  g_free (d->alias);
  d->alias = g_strdup ("Helen");
  d->subscribe_requested = TRUE;
  d->tags = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (d->tags, cambridge, cambridge);
  tp_handle_unref (self->priv->contact_repo, handle);

  /* Receive a couple of authorization requests too. These people are
   * local-pending in publish */

  handle = tp_handle_ensure (self->priv->contact_repo, "wim@example.com",
      NULL, NULL);
  d = ensure_contact (self, handle, NULL);
  g_free (d->alias);
  d->alias = g_strdup ("Wim");
  d->publish_requested = TRUE;
  d->publish_request = g_strdup ("I'm more metal than you!");
  tp_handle_unref (self->priv->contact_repo, handle);

  handle = tp_handle_ensure (self->priv->contact_repo, "christian@example.com",
      NULL, NULL);
  d = ensure_contact (self, handle, NULL);
  g_free (d->alias);
  d->alias = g_strdup ("Christian");
  d->publish_requested = TRUE;
  d->publish_request = g_strdup ("I have some fermented herring for you");
  tp_handle_unref (self->priv->contact_repo, handle);

  /* Add a couple of blocked contacts. */
  handle = tp_handle_ensure (self->priv->contact_repo, "bill@example.com",
      NULL, NULL);
  tp_handle_set_add (self->priv->blocked_contacts, handle);
  tp_handle_unref (self->priv->contact_repo, handle);
  handle = tp_handle_ensure (self->priv->contact_repo, "steve@example.com",
      NULL, NULL);
  tp_handle_set_add (self->priv->blocked_contacts, handle);
  tp_handle_unref (self->priv->contact_repo, handle);

  g_hash_table_iter_init (&iter, self->priv->contact_details);

  /* emit initial aliases, presences */
  while (g_hash_table_iter_next (&iter, &handle_p, NULL))
    {
      handle = GPOINTER_TO_UINT (handle_p);

      g_signal_emit (self, signals[ALIAS_UPDATED], 0, handle);
      g_signal_emit (self, signals[PRESENCE_UPDATED], 0, handle);
    }

  /* ... and off we go */
  tp_base_contact_list_set_list_received (manager);

  return FALSE;
}

static void
status_changed_cb (TpBaseConnection *conn,
                   guint status,
                   guint reason,
                   ExampleContactListManager *self)
{
  switch (status)
    {
    case TP_CONNECTION_STATUS_CONNECTED:
        {
          /* Do network I/O to get the contact list. This connection manager
           * doesn't really have a server, so simulate a small network delay
           * then invent a contact list */
          g_timeout_add_full (G_PRIORITY_DEFAULT,
              2 * self->priv->simulation_delay, receive_contact_lists,
              g_object_ref (self), g_object_unref);
        }
      break;

    case TP_CONNECTION_STATUS_DISCONNECTED:
        {
          example_contact_list_manager_close_all (self);

          tp_clear_object (&self->priv->conn);
        }
      break;
    }
}

static void
constructed (GObject *object)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) example_contact_list_manager_parent_class)->constructed;

  if (chain_up != NULL)
    {
      chain_up (object);
    }

  g_object_get (self,
      "connection", &self->priv->conn,
      NULL);
  g_assert (self->priv->conn != NULL);

  self->priv->contact_repo = tp_base_connection_get_handles (self->priv->conn,
      TP_HANDLE_TYPE_CONTACT);
  self->priv->contacts = tp_handle_set_new (self->priv->contact_repo);
  self->priv->blocked_contacts = tp_handle_set_new (self->priv->contact_repo);

  self->priv->status_changed_id = g_signal_connect (self->priv->conn,
      "status-changed", (GCallback) status_changed_cb, self);
}

static void
send_updated_roster (ExampleContactListManager *self,
                     TpHandle contact)
{
  ExampleContactDetails *d = g_hash_table_lookup (self->priv->contact_details,
      GUINT_TO_POINTER (contact));
  const gchar *identifier = tp_handle_inspect (self->priv->contact_repo,
      contact);

  /* In a real connection manager, we'd transmit these new details to the
   * server, rather than just printing messages. */

  if (d == NULL)
    {
      g_message ("Deleting contact %s from server", identifier);
    }
  else
    {
      g_message ("Transmitting new state of contact %s to server", identifier);
      g_message ("\talias = %s", d->alias);
      g_message ("\tcan see our presence = %s",
          d->publish ? "yes" :
          (d->publish_requested ? "no, but has requested it" : "no"));
      g_message ("\tsends us presence = %s",
          d->subscribe ? "yes" :
          (d->subscribe_requested ? "no, but we have requested it" : "no"));

      if (d->tags == NULL || g_hash_table_size (d->tags) == 0)
        {
          g_message ("\tnot in any groups");
        }
      else
        {
          GHashTableIter iter;
          gpointer k;

          g_hash_table_iter_init (&iter, d->tags);

          while (g_hash_table_iter_next (&iter, &k, NULL))
            {
              g_message ("\tin group: %s", (gchar *) k);
            }
        }
    }
}

static void
example_contact_list_manager_set_group_members_async (
    TpBaseContactList *manager,
    const gchar *group,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpHandleSet *new_contacts = tp_handle_set_copy (contacts);
  TpHandleSet *added = tp_handle_set_copy (contacts);
  TpHandleSet *removed = tp_handle_set_new (self->priv->contact_repo);
  TpIntSetFastIter iter;
  TpHandle member;

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      gboolean created = FALSE, updated = FALSE;
      ExampleContactDetails *d = ensure_contact (self, member, &created);
      gchar *tag = ensure_tag (self, group, TRUE);

      if (!created)
        tp_handle_set_remove (new_contacts, member);

      if (d->tags == NULL)
        d->tags = g_hash_table_new (g_str_hash, g_str_equal);

      if (g_hash_table_lookup (d->tags, tag) == NULL)
        {
          g_hash_table_insert (d->tags, tag, tag);
          updated = TRUE;
        }

      if (created || updated)
        send_updated_roster (self, member);
      else
        tp_handle_set_remove (added, member);
    }

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (self->priv->contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      ExampleContactDetails *d;

      if (tp_handle_set_is_member (contacts, member))
        continue;

      d = lookup_contact (self, member);

      if (d != NULL && d->tags != NULL && g_hash_table_remove (d->tags, group))
        tp_handle_set_add (removed, member);
    }

  if (!tp_handle_set_is_empty (new_contacts))
    tp_base_contact_list_contacts_changed (manager, new_contacts, NULL);

  if (!tp_handle_set_is_empty (added))
    tp_base_contact_list_groups_changed (manager, added, &group, 1, NULL, 0);

  if (!tp_handle_set_is_empty (removed))
    tp_base_contact_list_groups_changed (manager, removed, NULL, 0, &group, 1);

  tp_handle_set_destroy (added);
  tp_handle_set_destroy (removed);
  tp_handle_set_destroy (new_contacts);
  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_set_group_members_async);
}

static void
example_contact_list_manager_add_to_group_async (TpBaseContactList *manager,
    const gchar *group,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpHandleSet *new_contacts = tp_handle_set_copy (contacts);
  TpHandleSet *new_to_group = tp_handle_set_copy (contacts);
  TpIntSetFastIter iter;
  TpHandle member;

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      gboolean created = FALSE, updated = FALSE;
      ExampleContactDetails *d = ensure_contact (self, member, &created);
      gchar *tag = ensure_tag (self, group, TRUE);

      if (!created)
        tp_handle_set_remove (new_contacts, member);

      if (d->tags == NULL)
        d->tags = g_hash_table_new (g_str_hash, g_str_equal);

      if (g_hash_table_lookup (d->tags, tag) == NULL)
        {
          g_hash_table_insert (d->tags, tag, tag);
          updated = TRUE;
        }

      if (created || updated)
        send_updated_roster (self, member);
      else
        tp_handle_set_remove (new_to_group, member);
    }

  if (!tp_handle_set_is_empty (new_contacts))
    tp_base_contact_list_contacts_changed (manager, new_contacts, NULL);

  if (!tp_handle_set_is_empty (new_to_group))
    tp_base_contact_list_groups_changed (manager, new_to_group, &group, 1,
        NULL, 0);

  tp_handle_set_destroy (new_to_group);
  tp_handle_set_destroy (new_contacts);
  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_add_to_group_async);
}

static void
example_contact_list_manager_remove_from_group_async (
    TpBaseContactList *manager,
    const gchar *group,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpHandleSet *changed = tp_handle_set_copy (contacts);
  TpIntSetFastIter iter;
  TpHandle member;

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      ExampleContactDetails *d = lookup_contact (self, member);

      /* If not on the roster or not in any groups, we have nothing to do */
      if (d != NULL && d->tags != NULL && g_hash_table_remove (d->tags, group))
        send_updated_roster (self, member);
      else
        tp_handle_set_remove (changed, member);
    }

  if (!tp_handle_set_is_empty (changed))
    tp_base_contact_list_groups_changed (manager, changed, NULL, 0,
        &group, 1);

  tp_handle_set_destroy (changed);
  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_remove_from_group_async);
}

typedef struct {
    ExampleContactListManager *self;
    TpHandle contact;
} SelfAndContact;

static SelfAndContact *
self_and_contact_new (ExampleContactListManager *self,
                      TpHandle contact)
{
  SelfAndContact *ret = g_slice_new0 (SelfAndContact);

  ret->self = g_object_ref (self);
  ret->contact = contact;
  tp_handle_ref (self->priv->contact_repo, contact);
  return ret;
}

static void
self_and_contact_destroy (gpointer p)
{
  SelfAndContact *s = p;

  tp_handle_unref (s->self->priv->contact_repo, s->contact);
  g_object_unref (s->self);
  g_slice_free (SelfAndContact, s);
}

static void
receive_auth_request (ExampleContactListManager *self,
                      TpHandle contact)
{
  ExampleContactDetails *d;
  TpHandleSet *set;

  /* if shutting down, do nothing */
  if (self->priv->conn == NULL)
    return;

  /* A remote contact has asked to see our presence.
   *
   * In a real connection manager this would be the result of incoming
   * data from the server. */

  g_message ("From server: %s has sent us a publish request",
      tp_handle_inspect (self->priv->contact_repo, contact));

  d = ensure_contact (self, contact, NULL);

  if (d->publish)
    return;

  if (d->pre_approved)
    {
      /* the user already said yes, no need to signal anything */
      g_message ("... this publish request was already approved");
      d->pre_approved = FALSE;
      d->publish = TRUE;
      g_free (d->publish_request);
      d->publish_request = NULL;
      send_updated_roster (self, contact);
    }
  else
    {
      d->publish_requested = TRUE;
      d->publish_request = g_strdup ("May I see your presence, please?");
    }

  set = tp_handle_set_new (self->priv->contact_repo);
  tp_handle_set_add (set, contact);
  tp_base_contact_list_contacts_changed ((TpBaseContactList *) self,
      set, NULL);
  tp_handle_set_destroy (set);
}

static gboolean
receive_authorized (gpointer p)
{
  SelfAndContact *s = p;
  ExampleContactDetails *d;
  TpHandleSet *set;

  /* if shutting down, do nothing */
  if (s->self->priv->conn == NULL)
    return FALSE;

  /* A remote contact has accepted our request to see their presence.
   *
   * In a real connection manager this would be the result of incoming
   * data from the server. */

  g_message ("From server: %s has accepted our subscribe request",
      tp_handle_inspect (s->self->priv->contact_repo, s->contact));

  d = ensure_contact (s->self, s->contact, NULL);

  /* if we were already subscribed to them, then nothing really happened */
  if (d->subscribe)
    return FALSE;

  d->subscribe_requested = FALSE;
  d->subscribe = TRUE;

  set = tp_handle_set_new (s->self->priv->contact_repo);
  tp_handle_set_add (set, s->contact);
  tp_base_contact_list_contacts_changed ((TpBaseContactList *) s->self,
      set, NULL);
  tp_handle_set_destroy (set);

  /* their presence changes to something other than UNKNOWN */
  g_signal_emit (s->self, signals[PRESENCE_UPDATED], 0, s->contact);

  /* if we're not publishing to them, also pretend they have asked us to
   * do so */
  if (!d->publish)
    {
      receive_auth_request (s->self, s->contact);
    }

  return FALSE;
}

static gboolean
receive_unauthorized (gpointer p)
{
  SelfAndContact *s = p;
  ExampleContactDetails *d;
  TpHandleSet *set;

  /* if shutting down, do nothing */
  if (s->self->priv->conn == NULL)
    return FALSE;

  /* A remote contact has rejected our request to see their presence.
   *
   * In a real connection manager this would be the result of incoming
   * data from the server. */

  g_message ("From server: %s has rejected our subscribe request",
      tp_handle_inspect (s->self->priv->contact_repo, s->contact));

  d = ensure_contact (s->self, s->contact, NULL);

  if (!d->subscribe && !d->subscribe_requested)
    return FALSE;

  d->subscribe_requested = FALSE;
  d->subscribe = FALSE;

  set = tp_handle_set_new (s->self->priv->contact_repo);
  tp_handle_set_add (set, s->contact);
  tp_base_contact_list_contacts_changed ((TpBaseContactList *) s->self,
      set, NULL);
  tp_handle_set_destroy (set);

  /* their presence changes to UNKNOWN */
  g_signal_emit (s->self, signals[PRESENCE_UPDATED], 0, s->contact);

  return FALSE;
}

static gboolean
auth_request_cb (gpointer p)
{
  SelfAndContact *s = p;

  receive_auth_request (s->self, s->contact);

  return FALSE;
}

ExampleContactListPresence
example_contact_list_manager_get_presence (ExampleContactListManager *self,
                                           TpHandle contact)
{
  ExampleContactDetails *d = lookup_contact (self, contact);
  const gchar *id;

  if (d == NULL || !d->subscribe)
    {
      /* we don't know the presence of people not on the subscribe list,
       * by definition */
      return EXAMPLE_CONTACT_LIST_PRESENCE_UNKNOWN;
    }

  id = tp_handle_inspect (self->priv->contact_repo, contact);

  /* In this example CM, we fake contacts' presence based on their name:
   * contacts in the first half of the alphabet are available, the rest
   * (including non-alphabetic and non-ASCII initial letters) are away. */
  if ((id[0] >= 'A' && id[0] <= 'M') || (id[0] >= 'a' && id[0] <= 'm'))
    {
      return EXAMPLE_CONTACT_LIST_PRESENCE_AVAILABLE;
    }

  return EXAMPLE_CONTACT_LIST_PRESENCE_AWAY;
}

const gchar *
example_contact_list_manager_get_alias (ExampleContactListManager *self,
                                        TpHandle contact)
{
  ExampleContactDetails *d = lookup_contact (self, contact);

  if (d == NULL)
    {
      /* we don't have a user-defined alias for people not on the roster */
      return tp_handle_inspect (self->priv->contact_repo, contact);
    }

  return d->alias;
}

void
example_contact_list_manager_set_alias (ExampleContactListManager *self,
                                        TpHandle contact,
                                        const gchar *alias)
{
  gboolean created;
  ExampleContactDetails *d;
  gchar *old;

  /* if shutting down, do nothing */
  if (self->priv->conn == NULL)
    return;

  d = ensure_contact (self, contact, &created);

  if (created)
    {
      TpHandleSet *changed = tp_handle_set_new (self->priv->contact_repo);

      tp_handle_set_add (changed, contact);
      tp_base_contact_list_contacts_changed (
          (TpBaseContactList *) self, changed, NULL);
      tp_handle_set_destroy (changed);
    }

  /* FIXME: if stored list hasn't been retrieved yet, queue the change for
   * later */

  old = d->alias;
  d->alias = g_strdup (alias);

  if (created || tp_strdiff (old, alias))
    send_updated_roster (self, contact);

  g_free (old);
}

static TpHandleSet *
example_contact_list_manager_get_contacts (TpBaseContactList *manager)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);

  return tp_handle_set_copy (self->priv->contacts);
}

static TpHandleSet *
example_contact_list_manager_get_group_members (TpBaseContactList *manager,
    const gchar *group)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpIntSetFastIter iter;
  TpHandle member;
  TpHandleSet *members = tp_handle_set_copy (self->priv->contacts);

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (self->priv->contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      ExampleContactDetails *d = lookup_contact (self, member);

      if (d == NULL || d->tags == NULL ||
          !g_hash_table_lookup_extended (d->tags, group, NULL, NULL))
        tp_handle_set_remove (members, member);
    }

  return members;
}

static const ExampleContactDetails no_details = {
    NULL,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    "",
    NULL
};

static inline TpPresenceState
compose_presence (gboolean full,
    gboolean ask)
{
  if (full)
    return TP_PRESENCE_STATE_YES;
  else if (ask)
    return TP_PRESENCE_STATE_ASK;
  else
    return TP_PRESENCE_STATE_NO;
}

static void
example_contact_list_manager_get_states (TpBaseContactList *manager,
    TpHandle contact,
    TpPresenceState *subscribe,
    TpPresenceState *publish,
    gchar **publish_request)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  const ExampleContactDetails *details = lookup_contact (self, contact);

  if (details == NULL)
    details = &no_details;

  if (subscribe != NULL)
    *subscribe = compose_presence (details->subscribe,
        details->subscribe_requested);

  if (publish != NULL)
    *publish = compose_presence (details->publish, details->publish_requested);

  if (publish_request != NULL)
    *publish_request = g_strdup (details->publish_request);
}

static void
example_contact_list_manager_request_subscription_async (
    TpBaseContactList *manager,
    TpHandleSet *contacts,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpHandleSet *changed = tp_handle_set_copy (contacts);
  TpIntSetFastIter iter;
  TpHandle member;

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      gboolean created;
      ExampleContactDetails *d = ensure_contact (self, member, &created);
      gchar *message_lc;

      /* if they already authorized us, it's a no-op */
      if (d->subscribe)
        {
          tp_handle_set_remove (changed, member);
          continue;
        }

      /* In a real connection manager we'd start a network request here */
      g_message ("Transmitting authorization request to %s: %s",
          tp_handle_inspect (self->priv->contact_repo, member),
          message);

      if (created || !d->subscribe_requested)
        {
          d->subscribe_requested = TRUE;
          send_updated_roster (self, member);
        }

      /* Pretend that after a delay, the contact notices the request
       * and allows or rejects it. In this example connection manager,
       * empty requests are allowed, as are requests that contain "please"
       * case-insensitively. All other requests are denied. */
      message_lc = g_ascii_strdown (message, -1);

      if (message[0] == '\0' || strstr (message_lc, "please") != NULL)
        {
          g_timeout_add_full (G_PRIORITY_LOW,
              self->priv->simulation_delay, receive_authorized,
              self_and_contact_new (self, member),
              self_and_contact_destroy);
        }
      else
        {
          g_timeout_add_full (G_PRIORITY_LOW,
              self->priv->simulation_delay,
              receive_unauthorized,
              self_and_contact_new (self, member),
              self_and_contact_destroy);
        }

      g_free (message_lc);
    }

  tp_base_contact_list_contacts_changed (manager, changed, NULL);

  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_request_subscription_async);
}

static void
example_contact_list_manager_authorize_publication_async (
    TpBaseContactList *manager,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpHandleSet *changed = tp_handle_set_copy (contacts);
  TpIntSetFastIter iter;
  TpHandle member;

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      ExampleContactDetails *d = ensure_contact (self, member, NULL);

      /* We would like member to see our presence. In this simulated protocol,
       * this is meaningless, unless they have asked for it; but we can still
       * remember the pre-authorization in case they ask later. */
      if (!d->publish_requested)
        {
          d->pre_approved = TRUE;
          tp_handle_set_remove (changed, member);
        }
      else if (!d->publish)
        {
          d->publish = TRUE;
          d->publish_requested = FALSE;
          g_free (d->publish_request);
          d->publish_request = NULL;
          send_updated_roster (self, member);
        }
      else
        {
          tp_handle_set_remove (changed, member);
        }
    }

  tp_base_contact_list_contacts_changed (manager, changed, NULL);

  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_authorize_publication_async);
}

static void
example_contact_list_manager_store_contacts_async (
    TpBaseContactList *manager,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpHandleSet *changed = tp_handle_set_copy (contacts);
  TpIntSetFastIter iter;
  TpHandle member;

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      gboolean created;

      /* we would like member to be on the roster, but nothing more */

      ensure_contact (self, member, &created);

      if (created)
        send_updated_roster (self, member);
      else
        tp_handle_set_remove (changed, member);
    }

  tp_base_contact_list_contacts_changed (manager, changed, NULL);

  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_store_contacts_async);
}

static void
example_contact_list_manager_remove_contacts_async (TpBaseContactList *manager,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpHandleSet *removed = tp_handle_set_copy (contacts);
  TpIntSetFastIter iter;
  TpHandle member;

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      /* we would like to remove member from the roster altogether */
      if (lookup_contact (self, member) != NULL)
        {
          g_hash_table_remove (self->priv->contact_details,
              GUINT_TO_POINTER (member));
          send_updated_roster (self, member);

          tp_handle_set_remove (self->priv->contacts, member);

          /* since they're no longer on the subscribe list, we can't
           * see their presence, so emit a signal changing it to
           * UNKNOWN */
          g_signal_emit (self, signals[PRESENCE_UPDATED], 0, member);
        }
      else
        {
          /* no actual change */
          tp_handle_set_remove (removed, member);
        }
    }

  tp_base_contact_list_contacts_changed (manager, NULL, removed);

  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_remove_contacts_async);
}

static void
example_contact_list_manager_unsubscribe_async (TpBaseContactList *manager,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpHandleSet *changed = tp_handle_set_copy (contacts);
  TpIntSetFastIter iter;
  TpHandle member;

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      ExampleContactDetails *d = lookup_contact (self, member);

      /* we would like to avoid receiving member's presence any more,
       * or we would like to cancel an outstanding request for their
       * presence */

      if (d != NULL)
        {
          if (d->subscribe_requested)
            {
              g_message ("Cancelling our authorization request to %s",
                  tp_handle_inspect (self->priv->contact_repo, member));
              d->subscribe_requested = FALSE;
            }
          else if (d->subscribe)
            {
              g_message ("We no longer want presence from %s",
                  tp_handle_inspect (self->priv->contact_repo, member));
              d->subscribe = FALSE;

              /* since they're no longer on the subscribe list, we can't
               * see their presence, so emit a signal changing it to
               * UNKNOWN */
              g_signal_emit (self, signals[PRESENCE_UPDATED], 0, member);

            }
          else
            {
              /* nothing to do, avoid "updating the roster" */
              tp_handle_set_remove (changed, member);
              continue;
            }

          send_updated_roster (self, member);
        }
      else
        {
          tp_handle_set_remove (changed, member);
        }
    }

  tp_base_contact_list_contacts_changed (manager, changed, NULL);

  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_unsubscribe_async);
}

static void
example_contact_list_manager_unpublish_async (TpBaseContactList *manager,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpHandleSet *changed = tp_handle_set_copy (contacts);
  TpIntSetFastIter iter;
  TpHandle member;

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      ExampleContactDetails *d = lookup_contact (self, member);

      /* we would like member not to see our presence any more, or we
       * would like to reject a request from them to see our presence */

      if (d != NULL)
        {
          d->pre_approved = FALSE;

          if (d->publish_requested)
            {
              g_message ("Rejecting authorization request from %s",
                  tp_handle_inspect (self->priv->contact_repo, member));
              d->publish_requested = FALSE;
              g_free (d->publish_request);
              d->publish_request = NULL;
            }
          else if (d->publish)
            {
              g_message ("Removing authorization from %s",
                  tp_handle_inspect (self->priv->contact_repo, member));
              d->publish = FALSE;

              /* Pretend that after a delay, the contact notices the change
               * and asks for our presence again */
              g_timeout_add_full (G_PRIORITY_LOW,
                  self->priv->simulation_delay, auth_request_cb,
                  self_and_contact_new (self, member),
                  self_and_contact_destroy);
            }
          else
            {
              /* nothing to do, avoid "updating the roster" */
              tp_handle_set_remove (changed, member);
              continue;
            }

          send_updated_roster (self, member);
        }
      else
        {
          tp_handle_set_remove (changed, member);
        }
    }

  tp_base_contact_list_contacts_changed (manager, changed, NULL);

  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_unpublish_async);
}

static TpHandleSet *
example_contact_list_manager_get_blocked_contacts (
    TpBaseContactList *manager)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);

  return tp_handle_set_copy (self->priv->blocked_contacts);
}

static void
example_contact_list_manager_block_contacts_async (
    TpBaseContactList *manager,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpIntSetFastIter iter;
  TpHandleSet *changed = tp_handle_set_copy (contacts);
  TpHandle member;

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      if (tp_handle_set_is_member (self->priv->blocked_contacts, member))
        {
          tp_handle_set_remove (changed, member);
        }
      else
        {
          g_message ("Adding contact %s to blocked list",
              tp_handle_inspect (self->priv->contact_repo, member));
          tp_handle_set_add (self->priv->blocked_contacts, member);
        }
    }

  tp_base_contact_list_contact_blocking_changed (manager, changed);
  tp_handle_set_destroy (changed);
  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_block_contacts_async);
}

static void
example_contact_list_manager_unblock_contacts_async (
    TpBaseContactList *manager,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpIntSetFastIter iter;
  TpHandleSet *changed = tp_handle_set_copy (contacts);
  TpHandle member;

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      if (tp_handle_set_remove (self->priv->blocked_contacts, member))
        {
          g_message ("Removing contact %s from blocked list",
              tp_handle_inspect (self->priv->contact_repo, member));
        }
      else
        {
          tp_handle_set_remove (changed, member);
        }
    }

  tp_base_contact_list_contact_blocking_changed (manager, changed);
  tp_handle_set_destroy (changed);
  tp_simple_async_report_success_in_idle ((GObject *) self, callback,
      user_data, example_contact_list_manager_unblock_contacts_async);
}

static guint
example_contact_list_manager_get_group_storage (TpBaseContactList *manager)
{
  return TP_CONTACT_METADATA_STORAGE_TYPE_ANYONE;
}

static GStrv
example_contact_list_manager_get_groups (TpBaseContactList *manager)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  GPtrArray *pa = g_ptr_array_sized_new (
      g_hash_table_size (self->priv->all_tags) + 1);
  GHashTableIter iter;
  gpointer tag;

  g_hash_table_iter_init (&iter, self->priv->all_tags);

  while (g_hash_table_iter_next (&iter, &tag, NULL))
    g_ptr_array_add (pa, g_strdup (tag));

  g_ptr_array_add (pa, NULL);
  return (GStrv) g_ptr_array_free (pa, FALSE);
}

static GStrv
example_contact_list_manager_get_contact_groups (TpBaseContactList *manager,
    TpHandle contact)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  GPtrArray *pa = g_ptr_array_sized_new (
      g_hash_table_size (self->priv->all_tags) + 1);
  ExampleContactDetails *d = lookup_contact (self, contact);

  if (d != NULL && d->tags != NULL)
    {
      GHashTableIter iter;
      gpointer tag;

      g_hash_table_iter_init (&iter, d->tags);

      while (g_hash_table_iter_next (&iter, &tag, NULL))
        g_ptr_array_add (pa, g_strdup (tag));
    }

  g_ptr_array_add (pa, NULL);
  return (GStrv) g_ptr_array_free (pa, FALSE);
}

static void
example_contact_list_manager_remove_group_async (TpBaseContactList *manager,
    const gchar *group,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  TpIntSetFastIter iter;
  TpHandle member;

  /* signal the deletion */
  g_message ("deleting group %s", group);
  tp_base_contact_list_groups_removed (manager, &group, 1);

  /* apply the change to our model of the contacts too; we don't need to signal
   * the change, because TpBaseContactList already did */

  tp_intset_fast_iter_init (&iter, tp_handle_set_peek (self->priv->contacts));

  while (tp_intset_fast_iter_next (&iter, &member))
    {
      ExampleContactDetails *d = lookup_contact (self, member);

      if (d != NULL && d->tags != NULL)
        g_hash_table_remove (d->tags, group);
    }

  tp_simple_async_report_success_in_idle ((GObject *) manager, callback,
      user_data, example_contact_list_manager_remove_group_async);
}

static gchar *
example_contact_list_manager_normalize_group (TpBaseContactList *manager,
    const gchar *id)
{
  if (id[0] == '\0')
    return NULL;

  return g_utf8_normalize (id, -1, G_NORMALIZE_ALL_COMPOSE);
}

static void
example_contact_list_manager_rename_group_async (TpBaseContactList *manager,
    const gchar *old_name,
    const gchar *new_name,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  ExampleContactListManager *self = EXAMPLE_CONTACT_LIST_MANAGER (manager);
  gchar *tag = ensure_tag (self, new_name, FALSE);
  GHashTableIter iter;
  gpointer v;

  /* signal the rename */
  g_print ("renaming group %s to %s", old_name, new_name);
  tp_base_contact_list_group_renamed (manager, old_name, new_name);

  /* update our model (this doesn't need to signal anything because
   * TpBaseContactList already did) */

  g_hash_table_iter_init (&iter, self->priv->contact_details);

  while (g_hash_table_iter_next (&iter, NULL, &v))
    {
      ExampleContactDetails *d = v;

      if (d->tags != NULL && g_hash_table_remove (d->tags, old_name))
        g_hash_table_insert (d->tags, tag, tag);
    }

  tp_simple_async_report_success_in_idle ((GObject *) manager, callback,
      user_data, example_contact_list_manager_rename_group_async);
}

static void
example_contact_list_manager_class_init (ExampleContactListManagerClass *klass)
{
  TpBaseContactListClass *list_manager_class =
    (TpBaseContactListClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  g_object_class_install_property (object_class, PROP_SIMULATION_DELAY,
      g_param_spec_uint ("simulation-delay", "Simulation delay",
        "Delay between simulated network events",
        0, G_MAXUINT32, 1000,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  list_manager_class->get_contacts = example_contact_list_manager_get_contacts;
  list_manager_class->get_states = example_contact_list_manager_get_states;
  /* for this example CM we pretend there is a server-stored contact list,
   * like in XMPP, even though there obviously isn't really */
  list_manager_class->get_subscriptions_persist =
    tp_base_contact_list_true_func;

  g_type_class_add_private (klass, sizeof (ExampleContactListManagerPrivate));

  signals[ALIAS_UPDATED] = g_signal_new ("alias-updated",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__UINT, G_TYPE_NONE, 1, G_TYPE_UINT);

  signals[PRESENCE_UPDATED] = g_signal_new ("presence-updated",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__UINT, G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void
mutable_contact_list_iface_init (TpMutableContactListInterface *iface)
{
  iface->can_change_subscriptions = tp_base_contact_list_true_func;
  iface->get_request_uses_message = tp_base_contact_list_true_func;
  iface->request_subscription_async =
    example_contact_list_manager_request_subscription_async;
  iface->authorize_publication_async =
    example_contact_list_manager_authorize_publication_async;
  iface->store_contacts_async =
    example_contact_list_manager_store_contacts_async;
  iface->remove_contacts_async =
    example_contact_list_manager_remove_contacts_async;
  iface->unsubscribe_async = example_contact_list_manager_unsubscribe_async;
  iface->unpublish_async = example_contact_list_manager_unpublish_async;
}

static void
blockable_contact_list_iface_init (TpBlockableContactListInterface *iface)
{
  iface->can_block = tp_base_contact_list_true_func;
  iface->get_blocked_contacts =
    example_contact_list_manager_get_blocked_contacts;
  iface->block_contacts_async =
    example_contact_list_manager_block_contacts_async;
  iface->unblock_contacts_async =
    example_contact_list_manager_unblock_contacts_async;
}

static void
contact_group_list_iface_init (TpContactGroupListInterface *iface)
{
  iface->get_groups = example_contact_list_manager_get_groups;
  iface->get_group_members = example_contact_list_manager_get_group_members;
  iface->get_contact_groups = example_contact_list_manager_get_contact_groups;
  iface->normalize_group = example_contact_list_manager_normalize_group;
}

static void
mutable_contact_group_list_iface_init (
    TpMutableContactGroupListInterface *iface)
{
  iface->set_group_members_async =
    example_contact_list_manager_set_group_members_async;
  iface->add_to_group_async = example_contact_list_manager_add_to_group_async;
  iface->remove_from_group_async =
    example_contact_list_manager_remove_from_group_async;
  iface->remove_group_async = example_contact_list_manager_remove_group_async;
  iface->create_groups_async =
    example_contact_list_manager_create_groups_async;
  iface->rename_group_async = example_contact_list_manager_rename_group_async;
  iface->set_contact_groups_async =
    example_contact_list_manager_set_contact_groups_async;
  iface->get_group_storage = example_contact_list_manager_get_group_storage;
}
