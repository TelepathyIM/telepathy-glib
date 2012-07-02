/*
 * Copyright (C) 2009-2011 Collabora Ltd.
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
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "config.h"
#include "dbus-service-internal.h"

#include <string.h>
#include <sys/stat.h>

#include <glib.h>
#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

#include <telepathy-logger/event-internal.h>
#include <telepathy-logger/text-event.h>
#include <telepathy-logger/log-manager.h>
#include <telepathy-logger/log-manager-internal.h>

#include <extensions/extensions.h>

#define DEBUG_FLAG TPL_DEBUG_DBUS_SERVICE
#include <telepathy-logger/action-chain-internal.h>
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

#define FAVOURITE_CONTACTS_FILENAME "favourite-contacts.txt"

static void tpl_logger_iface_init (gpointer iface, gpointer iface_data);

struct _TplDBusServicePriv
{
  TplLogManager *manager;
  /* map of (string) account name -> (string set) contact ID */
  /* (the set is implemented as a hash table) */
  GHashTable *accounts_contacts_map;
  TplActionChain *favourite_contacts_actions;
};

G_DEFINE_TYPE_WITH_CODE (TplDBusService, _tpl_dbus_service, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TPL_TYPE_SVC_LOGGER, tpl_logger_iface_init));

typedef struct _FavouriteContactClosure FavouriteContactClosure;
typedef void (*FavouriteContactCallback) (gboolean success,
    FavouriteContactClosure *closure);


struct _FavouriteContactClosure {
  TplDBusService *service;
  gchar *account;
  gchar *contact_id;
  gchar *file_contents;
  DBusGMethodInvocation *context;
  FavouriteContactCallback cb;
};


static void
favourite_contact_closure_free (FavouriteContactClosure *closure)
{
  if (closure == NULL)
    return;

  if (closure->service != NULL)
    g_object_unref (closure->service);

  g_free (closure->account);
  g_free (closure->contact_id);
  g_free (closure->file_contents);
  g_slice_free (FavouriteContactClosure, closure);
}


static FavouriteContactClosure *
favourite_contact_closure_new (TplDBusService *self,
    const gchar *account,
    const gchar *contact_id,
    DBusGMethodInvocation *context)
{
  FavouriteContactClosure *closure;

  closure = g_slice_new0 (FavouriteContactClosure);
  closure->service = g_object_ref (G_OBJECT (self));
  closure->account = g_strdup (account);
  closure->contact_id = g_strdup (contact_id);
  /* XXX: ideally we'd up the ref count or duplicate this */
  closure->context = context;

  return closure;
}


static gboolean
favourite_contacts_add_event (TplDBusService *self,
    const gchar *account,
    const gchar *contact_id)
{
  GHashTable *contacts;
  gboolean new_event = FALSE;
  TplDBusServicePriv *priv;

  g_return_val_if_fail (TPL_IS_DBUS_SERVICE (self), FALSE);
  g_return_val_if_fail (account != NULL, FALSE);
  g_return_val_if_fail (contact_id != NULL, FALSE);

  priv = self->priv;

  DEBUG ("adding favourite contact: account '%s', ID '%s'",
      account, contact_id);

  contacts = g_hash_table_lookup (priv->accounts_contacts_map, account);
  if (contacts == NULL)
    {
      contacts = g_hash_table_new_full (g_str_hash, g_str_equal,
          (GDestroyNotify) g_free, NULL);
      g_hash_table_insert (priv->accounts_contacts_map, g_strdup (account),
          contacts);
      new_event = TRUE;
    }
  else if (g_hash_table_lookup (contacts, contact_id) == NULL)
    {
      new_event = TRUE;
    }

  if (new_event)
    {
      /* add dummy string for the value just for the convenience of looking up
       * whether the key already exists */
      g_hash_table_insert (contacts, g_strdup (contact_id),
          GINT_TO_POINTER (TRUE));
    }

  return new_event;
}


static const gchar *
favourite_contacts_get_filename (void)
{
  static gchar *filename = NULL;

  if (filename == NULL)
    {
      filename = g_build_filename (g_get_user_data_dir (), TPL_DATA_DIR,
          FAVOURITE_CONTACTS_FILENAME, NULL);
    }

  return filename;
}


static gboolean
favourite_contacts_parse_line (TplDBusService *self,
    const gchar *line)
{
  gboolean success = TRUE;
  gchar **elements;

  if (line == NULL || line[0] == '\0')
    return TRUE;

  /* this works on the assumption that account names can't have spaces in them
   */
  elements = g_strsplit (line, " ", 2);
  if (g_strv_length (elements) < 2)
    {
      DEBUG ("invalid number of elements on favourite contacts file line:\n"
          "%s\n", line);
      success = FALSE;
    }
  else
    favourite_contacts_add_event (self, elements[0], elements[1]);

  g_strfreev (elements);

  return success;
}


static void
favourite_contacts_file_read_line_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  GDataInputStream *data_stream = G_DATA_INPUT_STREAM (object);
  TplActionChain *action_chain = (TplActionChain *) (user_data);
  TplDBusService *self = _tpl_action_chain_get_object (action_chain);
  gchar *line;
  GError *error = NULL;

  line = g_data_input_stream_read_line_finish (data_stream, result, NULL, &error);

  if (error != NULL)
    {
      g_prefix_error (&error, "failed to open favourite contacts file: ");
      _tpl_action_chain_terminate (action_chain, error);
      g_clear_error (&error);
    }
  else if (line != NULL)
    {
      favourite_contacts_parse_line (self, line);

      g_data_input_stream_read_line_async (data_stream, G_PRIORITY_DEFAULT,
          NULL, favourite_contacts_file_read_line_cb, action_chain);
    }
  else
    _tpl_action_chain_continue (action_chain);
}


static void
favourite_contacts_file_open_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  GFile *file = G_FILE (object);
  TplActionChain *action_chain = (TplActionChain *) user_data;
  GFileInputStream *stream;
  GError *error = NULL;

  if ((stream = g_file_read_finish (file, result, &error)))
    {
      GDataInputStream *data_stream = g_data_input_stream_new (
          G_INPUT_STREAM (stream));

      g_data_input_stream_read_line_async (data_stream, G_PRIORITY_DEFAULT,
          NULL, favourite_contacts_file_read_line_cb, action_chain);

      g_object_unref (stream);
    }
  else if (error->code == G_IO_ERROR_NOT_FOUND)
    {
      DEBUG ("Favourite contacts file doesn't exist yet. Will create as "
          "necessary.");

      g_clear_error (&error);
      _tpl_action_chain_continue (action_chain);
    }
  else
    {
      g_prefix_error (&error, "Failed to open the favourite contacts file: ");
      _tpl_action_chain_terminate (action_chain, error);
      g_clear_error (&error);
    }
}


static void
pendingproc_favourite_contacts_file_open (TplActionChain *action_chain,
    gpointer user_data)
{
  const gchar *filename;
  GFile *file;

  filename = favourite_contacts_get_filename ();
  file = g_file_new_for_path (filename);

  g_file_read_async (file, G_PRIORITY_DEFAULT, NULL,
      favourite_contacts_file_open_cb, action_chain);

  g_object_unref (G_OBJECT (file));
}


static void
tpl_dbus_service_dispose (GObject *obj)
{
  TplDBusServicePriv *priv = TPL_DBUS_SERVICE (obj)->priv;

  if (priv->accounts_contacts_map != NULL)
    {
      g_hash_table_unref (priv->accounts_contacts_map);
      priv->accounts_contacts_map = NULL;
    }

  if (priv->favourite_contacts_actions != NULL)
    priv->favourite_contacts_actions = NULL;

  G_OBJECT_CLASS (_tpl_dbus_service_parent_class)->dispose (obj);
}


static void
favourite_contacts_file_parsed_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TplDBusService *self = TPL_DBUS_SERVICE (object);
  TplDBusServicePriv *priv = self->priv;
  GError *error = NULL;

  if (!_tpl_action_chain_new_finish (object, result, &error))
    {
      DEBUG ("Failed to parse the favourite contacts file and/or execute "
          "subsequent queued method calls: %s", error->message);
      g_error_free (error);
    }

  priv->favourite_contacts_actions = NULL;
}


static void
tpl_dbus_service_constructed (GObject *object)
{
  TplDBusServicePriv *priv = TPL_DBUS_SERVICE (object)->priv;

  priv->favourite_contacts_actions = _tpl_action_chain_new_async (object,
      favourite_contacts_file_parsed_cb, object);

  _tpl_action_chain_append (priv->favourite_contacts_actions,
      pendingproc_favourite_contacts_file_open, NULL);
  _tpl_action_chain_continue (priv->favourite_contacts_actions);
}


static void
_tpl_dbus_service_class_init (TplDBusServiceClass *klass)
{
  GObjectClass* object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = tpl_dbus_service_constructed;
  object_class->dispose = tpl_dbus_service_dispose;

  g_type_class_add_private (object_class, sizeof (TplDBusServicePriv));
}


static void
_tpl_dbus_service_init (TplDBusService *self)
{
  TplDBusServicePriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_DBUS_SERVICE, TplDBusServicePriv);

  g_return_if_fail (TPL_IS_DBUS_SERVICE (self));

  self->priv = priv;
  priv->manager = tpl_log_manager_dup_singleton ();
  priv->accounts_contacts_map = g_hash_table_new_full (g_str_hash, g_str_equal,
      (GDestroyNotify) g_free, (GDestroyNotify) g_hash_table_unref);
  priv->favourite_contacts_actions = NULL;
}


TplDBusService *
_tpl_dbus_service_new (void)
{
  return g_object_new (TPL_TYPE_DBUS_SERVICE, NULL);
}


static void
append_favourite_contacts_account_and_contacts (const gchar *account,
    GHashTable *contacts,
    GPtrArray *packed)
{
  GList *l;
  gchar **contact_ids;
  gint i;

  /* this case shouldn't happen, but this is just some basic sanity checking */
  if (g_hash_table_size (contacts) < 1)
    return;

  /* includes room for the terminal NULL */
  contact_ids = g_new0 (gchar *, g_hash_table_size (contacts)+1);

  for (i = 0, l = g_hash_table_get_keys (contacts);
      l;
      i++, l = g_list_delete_link (l, l))
    {
      contact_ids[i] = l->data;
    }

  g_ptr_array_add (packed, tp_value_array_build (2,
      DBUS_TYPE_G_OBJECT_PATH, account,
      G_TYPE_STRV, contact_ids,
      G_TYPE_INVALID));

  g_free (contact_ids);
}


static void
pendingproc_get_favourite_contacts (TplActionChain *action_chain,
    gpointer user_data)
{
  FavouriteContactClosure *closure = user_data;
  TplDBusServicePriv *priv;
  GPtrArray *packed;

  g_return_if_fail (closure);
  g_return_if_fail (TPL_IS_DBUS_SERVICE (closure->service));
  g_return_if_fail (closure->context != NULL);

  priv = closure->service->priv;

  packed = g_ptr_array_new_with_free_func ((GDestroyNotify) g_value_array_free);

  g_hash_table_foreach (priv->accounts_contacts_map,
      (GHFunc) append_favourite_contacts_account_and_contacts, packed);

  tpl_svc_logger_return_from_get_favourite_contacts (closure->context, packed);

  g_ptr_array_unref (packed);
  favourite_contact_closure_free (closure);

  if (action_chain != NULL)
    _tpl_action_chain_continue (action_chain);
}


static void
tpl_dbus_service_get_favourite_contacts (TplSvcLogger *logger,
    DBusGMethodInvocation *context)
{
  TplDBusService *self;
  TplDBusServicePriv *priv;
  FavouriteContactClosure *closure;

  g_return_if_fail (TPL_IS_DBUS_SERVICE (logger));
  g_return_if_fail (context != NULL);

  self = TPL_DBUS_SERVICE (logger);
  priv = self->priv;

  closure = favourite_contact_closure_new (self, NULL, NULL, context);

  /* If we're still waiting on the contacts to finish being parsed from disk,
   * queue this action */
  if (priv->favourite_contacts_actions != NULL)
    {
      _tpl_action_chain_append (priv->favourite_contacts_actions,
          pendingproc_get_favourite_contacts, closure);
    }
  else
    pendingproc_get_favourite_contacts (NULL, closure);
}


static void
append_favourite_contacts_file_entries (const gchar *account,
    GHashTable *contacts,
    GString *string)
{
  GList *l;

  for (l = g_hash_table_get_keys (contacts); l; l = g_list_delete_link (l, l))
    g_string_append_printf (string, "%s %s\n", account, (const gchar*) l->data);
}


static gchar *
favourite_contacts_to_string (TplDBusService *self)
{
  TplDBusServicePriv *priv = self->priv;
  GString *string;

  string = g_string_new ("");

  g_hash_table_foreach (priv->accounts_contacts_map,
      (GHFunc) append_favourite_contacts_file_entries, string);

  return g_string_free (string, FALSE);
}


static void
favourite_contacts_file_replace_contents_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  GFile *file = G_FILE (object);
  GError *error = NULL;
  FavouriteContactClosure *closure = user_data;
  gboolean success;

  if (g_file_replace_contents_finish (file, result, NULL, &error))
    {
      success = TRUE;
    }
  else
    {
      DEBUG ("Failed to save favourite contacts file: %s", error->message);
      success = FALSE;
      g_clear_error (&error);
    }

  ((FavouriteContactCallback) closure->cb) (success, closure);
}


static void
favourite_contacts_file_save_async (TplDBusService *self,
    FavouriteContactClosure *closure)
{
  gchar *dir;
  const gchar *filename;
  GFile *file;
  gchar *file_contents;

  g_return_if_fail (closure != NULL);

  filename = favourite_contacts_get_filename ();
  dir = g_path_get_dirname (filename);
  g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
  g_free (dir);

  file = g_file_new_for_path (filename);

  file_contents = favourite_contacts_to_string (self);

  closure->file_contents = file_contents;

  g_file_replace_contents_async (file,
      file_contents, strlen (file_contents), NULL, FALSE,
      G_FILE_CREATE_REPLACE_DESTINATION, NULL,
      favourite_contacts_file_replace_contents_cb, closure);

  g_object_unref (file);
}


static void
add_favourite_contact_file_save_cb (gboolean added_favourite,
    FavouriteContactClosure *closure)
{
  TplDBusServicePriv *priv = closure->service->priv;
  TplActionChain *action_chain = priv->favourite_contacts_actions;

  if (added_favourite)
    {
      const gchar *added[] = { NULL, NULL };
      const gchar *removed[] = { NULL };

      added[0] = closure->contact_id;

      tpl_svc_logger_emit_favourite_contacts_changed (closure->service,
          closure->account, added, removed);
    }

  tpl_svc_logger_return_from_add_favourite_contact (closure->context);

  favourite_contact_closure_free (closure);
  if (action_chain != NULL)
    _tpl_action_chain_continue (action_chain);
}


static void
pendingproc_add_favourite_contact (TplActionChain *action_chain,
    gpointer user_data)
{
  FavouriteContactClosure *closure = user_data;
  gboolean should_add = FALSE;
  GError *error = NULL;

  g_return_if_fail (closure);
  g_return_if_fail (TPL_IS_DBUS_SERVICE (closure->service));
  g_return_if_fail (closure->context != NULL);

  if (!tp_dbus_check_valid_object_path (closure->account, &error))
    {
      dbus_g_method_return_error (closure->context, error);

      goto pendingproc_add_favourite_contact_ERROR;
    }

  should_add = favourite_contacts_add_event (closure->service, closure->account,
      closure->contact_id);

  closure->cb = add_favourite_contact_file_save_cb;

  if (should_add)
    favourite_contacts_file_save_async (closure->service, closure);
  else
    add_favourite_contact_file_save_cb (FALSE, closure);

  return;

pendingproc_add_favourite_contact_ERROR:
  if (action_chain != NULL)
    _tpl_action_chain_terminate (action_chain, error);

  g_clear_error (&error);
}


static void
tpl_dbus_service_add_favourite_contact (TplSvcLogger *logger,
    const gchar *account,
    const gchar *contact_id,
    DBusGMethodInvocation *context)
{
  TplDBusService *self = TPL_DBUS_SERVICE (logger);
  TplDBusServicePriv *priv;
  FavouriteContactClosure *closure;

  g_return_if_fail (TPL_IS_DBUS_SERVICE (self));
  g_return_if_fail (context != NULL);

  priv = self->priv;

  closure = favourite_contact_closure_new (self, account, contact_id, context);

  /* If we're still waiting on the contacts to finish being parsed from disk,
   * queue this action */
  if (priv->favourite_contacts_actions != NULL)
    {
      _tpl_action_chain_append (priv->favourite_contacts_actions,
          pendingproc_add_favourite_contact, closure);
    }
  else
    pendingproc_add_favourite_contact (NULL, closure);
}

static void
remove_favourite_contact_file_save_cb (gboolean removed_favourite,
    FavouriteContactClosure *closure)
{
  TplDBusServicePriv *priv = closure->service->priv;
  TplActionChain *action_chain = priv->favourite_contacts_actions;

  if (removed_favourite)
    {
      const gchar *added[] = { NULL };
      const gchar *removed[] = { NULL, NULL };

      removed[0] = closure->contact_id;

      tpl_svc_logger_emit_favourite_contacts_changed (closure->service,
          closure->account, added, removed);
    }

  tpl_svc_logger_return_from_remove_favourite_contact (closure->context);

  favourite_contact_closure_free (closure);
  if (action_chain != NULL)
    _tpl_action_chain_continue (action_chain);
}


static void
pendingproc_remove_favourite_contact (TplActionChain *action_chain,
    gpointer user_data)
{
  FavouriteContactClosure *closure = user_data;
  GHashTable *contacts;
  gboolean removed = FALSE;
  GError *error = NULL;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (TPL_IS_DBUS_SERVICE (closure->service));
  g_return_if_fail (closure->context != NULL);

  TplDBusServicePriv *priv = closure->service->priv;

  if (!tp_dbus_check_valid_object_path (closure->account, &error))
    {
      dbus_g_method_return_error (closure->context, error);

      goto pendingproc_remove_favourite_contact_ERROR;
    }

  DEBUG ("removing favourite contact: account '%s', ID '%s'",
      closure->account, closure->contact_id);

  contacts = g_hash_table_lookup (priv->accounts_contacts_map,
      closure->account);
  if (contacts != NULL && g_hash_table_remove (contacts, closure->contact_id))
      removed = TRUE;

  closure->cb = remove_favourite_contact_file_save_cb;

  if (removed)
    favourite_contacts_file_save_async (closure->service, closure);
  else
    remove_favourite_contact_file_save_cb (FALSE, closure);

  return;

pendingproc_remove_favourite_contact_ERROR:
  if (action_chain != NULL)
    _tpl_action_chain_terminate (action_chain, error);

  g_clear_error (&error);
}

static void
tpl_dbus_service_remove_favourite_contact (TplSvcLogger *logger,
    const gchar *account,
    const gchar *contact_id,
    DBusGMethodInvocation *context)
{
  TplDBusService *self = TPL_DBUS_SERVICE (logger);
  TplDBusServicePriv *priv;
  FavouriteContactClosure *closure;

  g_return_if_fail (TPL_IS_DBUS_SERVICE (self));
  g_return_if_fail (context != NULL);

  priv = self->priv;

  closure = favourite_contact_closure_new (self, account, contact_id, context);

  /* If we're still waiting on the contacts to finish being parsed from disk,
   * queue this action */
  if (priv->favourite_contacts_actions != NULL)
    {
      _tpl_action_chain_append (priv->favourite_contacts_actions,
          pendingproc_remove_favourite_contact, closure);
    }
  else
    pendingproc_remove_favourite_contact (NULL, closure);
}


static void
tpl_dbus_service_clear (TplSvcLogger *logger,
    DBusGMethodInvocation *context)
{
  TplDBusService *self = TPL_DBUS_SERVICE (logger);

  g_return_if_fail (TPL_IS_DBUS_SERVICE (self));
  g_return_if_fail (context != NULL);

  /* We want to clear synchronously to avoid concurent write */
  _tpl_log_manager_clear (self->priv->manager);

  tpl_svc_logger_return_from_clear (context);
}


static void
tpl_dbus_service_clear_account (TplSvcLogger *logger,
    const gchar *account_path,
    DBusGMethodInvocation *context)
{
  TplDBusService *self = TPL_DBUS_SERVICE (logger);
  TpDBusDaemon *bus;
  TpAccount *account;
  GError *error = NULL;

  g_return_if_fail (TPL_IS_DBUS_SERVICE (self));
  g_return_if_fail (context != NULL);

  bus = tp_dbus_daemon_dup (&error);
  if (bus == NULL)
    {
      DEBUG ("Unable to acquire the bus daemon: %s", error->message);
      dbus_g_method_return_error (context, error);
      goto out;
    }

  account = tp_account_new (bus, account_path, &error);
  if (account == NULL)
    {
      DEBUG ("Unable to acquire the account for %s: %s", account_path,
          error->message);
      dbus_g_method_return_error (context, error);
      goto out;
    }

  /* We want to clear synchronously to avoid concurent write */
  _tpl_log_manager_clear_account (self->priv->manager, account);
  g_object_unref (account);

  tpl_svc_logger_return_from_clear_account (context);

out:
  if (bus != NULL)
    g_object_unref (bus);

  g_clear_error (&error);
}


static void
tpl_dbus_service_clear_entity (TplSvcLogger *logger,
    const gchar *account_path,
    const gchar *identifier,
    gint type,
    DBusGMethodInvocation *context)
{
  TplDBusService *self = TPL_DBUS_SERVICE (logger);
  TpDBusDaemon *bus;
  TpAccount *account;
  TplEntity *entity;
  GError *error = NULL;

  g_return_if_fail (TPL_IS_DBUS_SERVICE (self));
  g_return_if_fail (context != NULL);
  g_return_if_fail (!TPL_STR_EMPTY (identifier));

  bus = tp_dbus_daemon_dup (&error);
  if (bus == NULL)
    {
      DEBUG ("Unable to acquire the bus daemon: %s", error->message);
      dbus_g_method_return_error (context, error);
      goto out;
    }

  account = tp_account_new (bus, account_path, &error);
  if (account == NULL)
    {
      DEBUG ("Unable to acquire the account for %s: %s", account_path,
          error->message);
      dbus_g_method_return_error (context, error);
      goto out;
    }

  entity = tpl_entity_new (identifier, type, NULL, NULL);

  /* We want to clear synchronously to avoid concurent write */
  _tpl_log_manager_clear_entity (self->priv->manager, account, entity);

  g_object_unref (account);
  g_object_unref (entity);

  tpl_svc_logger_return_from_clear_account (context);

out:
  if (bus != NULL)
    g_object_unref (bus);

  g_clear_error (&error);
}

static void
tpl_logger_iface_init (gpointer iface,
    gpointer iface_data)
{
  TplSvcLoggerClass *klass = (TplSvcLoggerClass *) iface;

#define IMPLEMENT(x) tpl_svc_logger_implement_##x (klass, tpl_dbus_service_##x)
  IMPLEMENT (get_favourite_contacts);
  IMPLEMENT (add_favourite_contact);
  IMPLEMENT (remove_favourite_contact);
  IMPLEMENT (clear);
  IMPLEMENT (clear_account);
  IMPLEMENT (clear_entity);
#undef IMPLEMENT
}
