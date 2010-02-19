/*
 * Copyright (C) 2010 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Danielle Madeley <danielle.madeley@collabora.co.uk>
 */

#include <config.h>

#include <string.h>

#include <telepathy-glib/telepathy-glib.h>
#include <sqlite3.h>

#include "log-entry-text.h"
#include "log-manager.h"
#include "log-store-counter.h"
#include "datetime.h"

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include "debug.h"

#define GET_PRIVATE(obj)	(G_TYPE_INSTANCE_GET_PRIVATE ((obj), TPL_TYPE_LOG_STORE_COUNTER, TplLogStoreCounterPrivate))

static void log_store_iface_init (TplLogStoreInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TplLogStoreCounter, tpl_log_store_counter,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TPL_TYPE_LOG_STORE, log_store_iface_init));

enum /* properties */
{
  PROP_0,
  PROP_NAME,
  PROP_READABLE,
  PROP_WRITABLE
};

typedef struct _TplLogStoreCounterPrivate TplLogStoreCounterPrivate;
struct _TplLogStoreCounterPrivate
{
  sqlite3 *db;
};

static GObject *singleton = NULL;

static GObject *
tpl_log_store_counter_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  if (singleton != NULL)
    g_object_ref (singleton);
  else
    {
      singleton =
        G_OBJECT_CLASS (tpl_log_store_counter_parent_class)->constructor (
            type, n_props, props);

      if (singleton == NULL)
        return NULL;

      g_object_add_weak_pointer (singleton, (gpointer *) &singleton);
    }

  return singleton;
}

static char *
get_cache_filename (void)
{
  return g_build_filename (g_get_user_cache_dir (),
      "telepathy",
      "logger",
      "message-counts",
      NULL);
}

static void
tpl_log_store_counter_get_property (GObject *self,
    guint id,
    GValue *value,
    GParamSpec *pspec)
{
  switch (id)
    {
      case PROP_NAME:
        g_value_set_string (value, "MessageCounts");
        break;

      case PROP_READABLE:
        /* this store should never be queried by the LogManager */
        g_value_set_boolean (value, FALSE);
        break;

      case PROP_WRITABLE:
        g_value_set_boolean (value, TRUE);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, id, pspec);
        break;
    }
}

static void
tpl_log_store_counter_set_property (GObject *self,
    guint id,
    const GValue *value,
    GParamSpec *pspec)
{
  switch (id)
    {
      case PROP_NAME:
      case PROP_READABLE:
      case PROP_WRITABLE:
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, id, pspec);
        break;
    }
}

static void
tpl_log_store_counter_dispose (GObject *self)
{
  TplLogStoreCounterPrivate *priv = GET_PRIVATE (self);

  if (priv->db != NULL)
    {
      sqlite3_close (priv->db);
      priv->db = NULL;
    }

  G_OBJECT_CLASS (tpl_log_store_counter_parent_class)->dispose (self);
}

static void
tpl_log_store_counter_class_init (TplLogStoreCounterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructor = tpl_log_store_counter_constructor;
  gobject_class->get_property = tpl_log_store_counter_get_property;
  gobject_class->set_property = tpl_log_store_counter_set_property;
  gobject_class->dispose = tpl_log_store_counter_dispose;

  g_object_class_override_property (gobject_class, PROP_NAME, "name");
  g_object_class_override_property (gobject_class, PROP_READABLE, "readable");
  g_object_class_override_property (gobject_class, PROP_WRITABLE, "writable");

  g_type_class_add_private (gobject_class, sizeof (TplLogStoreCounterPrivate));
}

static void
tpl_log_store_counter_init (TplLogStoreCounter *self)
{
  TplLogStoreCounterPrivate *priv = GET_PRIVATE (self);
  char *filename = get_cache_filename ();
  int e;
  char *errmsg = NULL;

  DEBUG ("cache file is '%s'", filename);

  /* check to see if the count cache exists */
  if (!g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      char *dirname = g_path_get_dirname (filename);

      DEBUG ("Creating cache");

      g_mkdir_with_parents (dirname, 0700);
      g_free (dirname);
    }

  e = sqlite3_open_v2 (filename, &priv->db,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
      NULL);
  if (e != SQLITE_OK)
    {
      g_critical ("Failed to open Sqlite3 DB: %s\n",
          sqlite3_errmsg (priv->db));
      goto out;
    }

  sqlite3_exec (priv->db,
      "CREATE TABLE IF NOT EXISTS messagecounts ("
        "account TEXT, "
        "identifier TEXT, "
        "chatroom BOOLEAN, "
        "date DATE, "
        "messages INTEGER)",
      NULL,
      NULL,
      &errmsg);
  if (errmsg != NULL)
    {
      g_critical ("Failed to create table: %s\n", errmsg);
      sqlite3_free (errmsg);
      goto out;
    }

out:
  g_free (filename);
}

static const char *
get_account_name (TpAccount *account)
{
  return tp_proxy_get_object_path (account) +
    strlen (TP_ACCOUNT_OBJECT_PATH_BASE);
}

static const char *
get_account_name_from_entry (TplLogEntry *entry)
{
  return tpl_log_entry_get_account_path (entry) +
    strlen (TP_ACCOUNT_OBJECT_PATH_BASE);
}

static char *
get_date (TplLogEntry *entry)
{
  time_t t;

  t = tpl_log_entry_get_timestamp (entry);

  return tpl_time_to_string_utc (t, "%Y-%m-%d");
}

static const char *
tpl_log_store_counter_get_name (TplLogStore *self)
{
  return "MessageCounts";
}

static gboolean
tpl_log_store_counter_add_message (TplLogStore *self,
    TplLogEntry *message,
    GError **error)
{
  TplLogStoreCounterPrivate *priv = GET_PRIVATE (self);
  const char *account, *identifier;
  gboolean chatroom;
  char *date;
  int count = 0;
  sqlite3_stmt *sql = NULL;
  gboolean retval = FALSE;
  gboolean insert = FALSE;
  int e;

  DEBUG ("tpl_log_store_counter_add_message");

  if (!TPL_IS_LOG_ENTRY_TEXT (message) ||
      tpl_log_entry_get_signal_type (message) !=
          TPL_LOG_ENTRY_TEXT_SIGNAL_RECEIVED)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_MESSAGE,
          "Message not handled by this log store");

      return FALSE;
    }

  DEBUG ("message received");

  account = get_account_name_from_entry (message);
  identifier = tpl_log_entry_get_chat_id (message);
  chatroom = tpl_log_entry_text_is_chatroom (TPL_LOG_ENTRY_TEXT (message));
  date = get_date (message);

  DEBUG ("account = %s", account);
  DEBUG ("identifier = %s", identifier);
  DEBUG ("chatroom = %i", chatroom);
  DEBUG ("date = %s", date);

  /* get the existing row */
  e = sqlite3_prepare_v2 (priv->db,
      "SELECT messages FROM messagecounts WHERE "
        "account=? AND "
        "identifier=? AND "
        "chatroom=? AND "
        "date=date(?)",
      -1, &sql, NULL);
  if (e != SQLITE_OK)
    {
      DEBUG ("Failed to prepare SQL: %s",
          sqlite3_errmsg (priv->db));

      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_MESSAGE,
          "SQL Error");

      goto out;
    }

  sqlite3_bind_text (sql, 1, account, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text (sql, 2, identifier, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (sql, 3, chatroom);
  sqlite3_bind_text (sql, 4, date, -1, SQLITE_TRANSIENT);

  e = sqlite3_step (sql);
  if (e == SQLITE_DONE)
    {
      DEBUG ("no rows, insert");
      insert = TRUE;
    }
  else if (e == SQLITE_ROW)
    {
      count = sqlite3_column_int (sql, 0);
      DEBUG ("got row, count = %i", count);
    }
  else
    {
      DEBUG ("Failed to execute SQL: %s",
          sqlite3_errmsg (priv->db));

      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_MESSAGE,
          "SQL Error");

      goto out;
    }

  sqlite3_finalize (sql);
  sql = NULL;

  /* increment the message count */
  count++;

  DEBUG ("new count = %i, insert = %i", count, insert);

  /* update table with new message count */
  if (insert)
    e = sqlite3_prepare_v2 (priv->db,
        "INSERT INTO messagecounts VALUES (?, ?, ?, date(?), ?)",
        -1, &sql, NULL);
  else
    e = sqlite3_prepare_v2 (priv->db,
        "UPDATE messagecounts SET messages=? WHERE "
          "account=? AND "
          "identifier=? AND "
          "chatroom=? AND "
          "date=date(?)",
        -1, &sql, NULL);

  if (e != SQLITE_OK)
    {
      DEBUG ("Failed to prepare SQL: %s",
          sqlite3_errmsg (priv->db));

      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_MESSAGE,
          "SQL Error");

      goto out;
    }

  if (insert)
    {
      sqlite3_bind_text (sql, 1, account, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text (sql, 2, identifier, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int (sql, 3, chatroom);
      sqlite3_bind_text (sql, 4, date, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int (sql, 5, count);
    }
  else
    {
      sqlite3_bind_int (sql, 1, count);
      sqlite3_bind_text (sql, 2, account, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text (sql, 3, identifier, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int (sql, 4, chatroom);
      sqlite3_bind_text (sql, 5, date, -1, SQLITE_TRANSIENT);
    }

  e = sqlite3_step (sql);
  if (e != SQLITE_DONE)
    {
      DEBUG ("Failed to execute SQL: %s",
          sqlite3_errmsg (priv->db));

      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_MESSAGE,
          "SQL Error");

      goto out;
    }

  retval = TRUE;

out:
  g_free (date);

  if (sql != NULL)
    sqlite3_finalize (sql);

  /* check that we set an error if appropriate */
  g_assert ((retval == TRUE && *error == NULL) ||
            (retval == FALSE && *error != NULL));

  return retval;
}

static GList *
tpl_log_store_counter_get_chats (TplLogStore *self,
    TpAccount *account)
{
  TplLogStoreCounterPrivate *priv = GET_PRIVATE (self);
  sqlite3_stmt *sql = NULL;
  int e;
  GList *list = NULL;
  const char *account_name = get_account_name (account);

  DEBUG ("account = %s", account_name);

  /* list all the identifiers known to the database */
  e = sqlite3_prepare_v2 (priv->db,
      "SELECT DISTINCT identifier, chatroom FROM messagecounts WHERE "
          "account=?",
      -1, &sql, NULL);
  if (e != SQLITE_OK)
    {
      DEBUG ("Failed to prepare SQL: %s",
          sqlite3_errmsg (priv->db));

      goto out;
    }

  sqlite3_bind_text (sql, 1, account_name, -1, SQLITE_TRANSIENT);

  while ((e = sqlite3_step (sql)) == SQLITE_ROW)
    {
      TplLogSearchHit *hit = g_slice_new0 (TplLogSearchHit);
      const char *identifier;
      gboolean chatroom;

      /* for some reason this returns unsigned char */
      identifier = (const char *) sqlite3_column_text (sql, 0);
      chatroom = sqlite3_column_int (sql, 1);

      DEBUG ("identifier = %s, chatroom = %i", identifier, chatroom);

      hit->chat_id = g_strdup (identifier);
      hit->is_chatroom = chatroom;

      list = g_list_prepend (list, hit);
    }
  if (e != SQLITE_DONE)
    {
      DEBUG ("Failed to execute SQL: %s",
          sqlite3_errmsg (priv->db));
      goto out;
    }

out:
  if (sql != NULL)
    sqlite3_finalize (sql);

  return list;
}

static void
log_store_iface_init (TplLogStoreInterface *iface)
{
  iface->get_name = tpl_log_store_counter_get_name;
  iface->add_message = tpl_log_store_counter_add_message;
  iface->get_chats = tpl_log_store_counter_get_chats;
}

TplLogStore *
tpl_log_store_counter_dup (void)
{
  return g_object_new (TPL_TYPE_LOG_STORE_COUNTER, NULL);
}

gint64
tpl_log_store_counter_get_most_recent (TplLogStoreCounter *self,
    TpAccount *account,
    const char *identifier)
{
  TplLogStoreCounterPrivate *priv = GET_PRIVATE (self);
  sqlite3_stmt *sql = NULL;
  int e;
  gint64 date = -1;;
  const char *account_name;

  account_name = get_account_name (account);

  /* this SQL gets this most recent date for a single identifier */
  e = sqlite3_prepare_v2 (priv->db,
      "SELECT STRFTIME('%s', date) FROM messagecounts WHERE "
          "account=? AND "
          "identifier=? "
        "ORDER BY date DESC LIMIT 1",
      -1, &sql, NULL);
  if (e != SQLITE_OK)
    {
      DEBUG ("Failed to prepare SQL: %s",
          sqlite3_errmsg (priv->db));

      goto out;
    }

  sqlite3_bind_text (sql, 1, account_name, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text (sql, 2, identifier, -1, SQLITE_TRANSIENT);

  e = sqlite3_step (sql);
  if (e == SQLITE_DONE)
    {
      DEBUG ("no rows (account identifer doesn't exist?)");
    }
  else if (e == SQLITE_ROW)
    {
      date = sqlite3_column_int64 (sql, 0);
      DEBUG ("got row, date = %" G_GINT64_FORMAT, date);
    }
  else
    {
      DEBUG ("Failed to execute SQL: %s",
          sqlite3_errmsg (priv->db));

      goto out;
    }

out:

  if (sql != NULL)
    sqlite3_finalize (sql);

  return date;
}
double
tpl_log_store_counter_get_frequency (TplLogStoreCounter *self,
    TpAccount *account,
    const char *identifier)
{
  TplLogStoreCounterPrivate *priv = GET_PRIVATE (self);
  sqlite3_stmt *sql = NULL;
  int e;
  double freq = -1.;
  const char *account_name;

  account_name = get_account_name (account);

  /* this SQL query builds the frequency for a single identifier */
  e = sqlite3_prepare_v2 (priv->db,
      "SELECT SUM(messages / ROUND(JULIANDAY('now') - JULIANDAY(date) + 1)) "
        "FROM messagecounts WHERE "
          "account=? AND "
          "identifier=?",
      -1, &sql, NULL);
  if (e != SQLITE_OK)
    {
      DEBUG ("Failed to prepare SQL: %s",
          sqlite3_errmsg (priv->db));

      goto out;
    }

  sqlite3_bind_text (sql, 1, account_name, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text (sql, 2, identifier, -1, SQLITE_TRANSIENT);

  e = sqlite3_step (sql);
  if (e == SQLITE_DONE)
    {
      DEBUG ("no rows (account identifer doesn't exist?)");
    }
  else if (e == SQLITE_ROW)
    {
      freq = sqlite3_column_double (sql, 0);
      DEBUG ("got row, freq = %g", freq);
    }
  else
    {
      DEBUG ("Failed to execute SQL: %s",
          sqlite3_errmsg (priv->db));

      goto out;
    }

out:

  if (sql != NULL)
    sqlite3_finalize (sql);

  return freq;
}
