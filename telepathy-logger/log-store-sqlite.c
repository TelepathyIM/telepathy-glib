/*
 * Copyright (C) 2010-2011 Collabora Ltd.
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
 * Authors: Danielle Madeley <danielle.madeley@collabora.co.uk>
 *          Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 *          Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#include <config.h>
#include "log-store-sqlite-internal.h"

#include <string.h>

#include <telepathy-glib/telepathy-glib.h>
#include <sqlite3.h>

#include "event-internal.h"
#include "text-event.h"
#include "text-event-internal.h"
#include "entity-internal.h"
#include "log-manager-internal.h"

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include "debug-internal.h"
#include "util-internal.h"

#define TPL_LOG_STORE_SQLITE_NAME "Sqlite"

static void log_store_iface_init (TplLogStoreInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TplLogStoreSqlite, _tpl_log_store_sqlite,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TPL_TYPE_LOG_STORE, log_store_iface_init));

enum /* properties */
{
  PROP_0,
  PROP_NAME,
  PROP_READABLE,
  PROP_WRITABLE
};

struct _TplLogStoreSqlitePrivate
{
  sqlite3 *db;
};

static GObject *singleton = NULL;


static GObject *
tpl_log_store_sqlite_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  if (singleton != NULL)
    g_object_ref (singleton);
  else
    {
      singleton =
        G_OBJECT_CLASS (_tpl_log_store_sqlite_parent_class)->constructor (
            type, n_props, props);

      if (singleton == NULL)
        return NULL;

      g_object_add_weak_pointer (singleton, (gpointer *) &singleton);
    }

  return singleton;
}


static char *
get_db_filename (void)
{
  return g_build_filename (g_get_user_cache_dir (),
      "telepathy",
      "logger",
      "sqlite-data",
      NULL);
}


static void
tpl_log_store_sqlite_get_property (GObject *self,
    guint id,
    GValue *value,
    GParamSpec *pspec)
{
  switch (id)
    {
      case PROP_NAME:
        g_value_set_string (value, TPL_LOG_STORE_SQLITE_NAME);
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
tpl_log_store_sqlite_set_property (GObject *self,
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
purge_pending_messages (TplLogStoreSqlitePrivate *priv,
    GTimeSpan delta,
    GError **error)
{
  sqlite3_stmt *sql = NULL;
  GDateTime *now;
  GDateTime *timestamp;
  gchar *date;
  int e;

  g_return_if_fail (error == NULL || *error == NULL);

  now = g_date_time_new_now_utc ();
  timestamp = g_date_time_add (now, -(delta * G_TIME_SPAN_SECOND));

  date = g_date_time_format (timestamp,
      TPL_LOG_STORE_SQLITE_TIMESTAMP_FORMAT);

  g_date_time_unref (now);

  DEBUG ("Purging entries older than %s (%u seconds ago)", date, (guint) delta);

  e = sqlite3_prepare_v2 (priv->db,
      "DELETE FROM pending_messages WHERE timestamp<?",
      -1, &sql, NULL);

  if (e != SQLITE_OK)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "SQL Error preparing statement in %s: %s", G_STRFUNC,
          sqlite3_errmsg (priv->db));

      goto out;
    }

  sqlite3_bind_int64 (sql, 1, g_date_time_to_unix (timestamp));

  e = sqlite3_step (sql);
  if (e != SQLITE_DONE)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "SQL Error in %s: %s", G_STRFUNC, sqlite3_errmsg (priv->db));
    }

out:
  g_date_time_unref (timestamp);

  if (sql != NULL)
    sqlite3_finalize (sql);

  g_free (date);
}


static void
_tpl_log_store_sqlite_init (TplLogStoreSqlite *self)
{
  TplLogStoreSqlitePrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_LOG_STORE_SQLITE, TplLogStoreSqlitePrivate);
  char *filename = get_db_filename ();
  int e;
  char *errmsg = NULL;
  GError *error = NULL;

  self->priv = priv;

  DEBUG ("cache file is '%s'", filename);

  /* counter & cache tables - common part */
  /* check to see if the sqlite db exists */
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
      CRITICAL ("Failed to open Sqlite3 DB: %s\n",
          sqlite3_errmsg (priv->db));
      goto out;
    }
  /* end of common part */

  /* start of cache table init */

  /* drop deprecated table (since 0.2.6) */
  sqlite3_exec (priv->db, "DROP TABLE IF EXISTS message_cache",
      NULL, NULL, &errmsg);
  if (errmsg != NULL)
    {
      CRITICAL ("Failed to drop deprecated message_cache table: %s\n", errmsg);
      sqlite3_free (errmsg);
      goto out;
    }

  sqlite3_exec (priv->db, "CREATE TABLE IF NOT EXISTS pending_messages ( "
      "channel TEXT NOT NULL, "
      "id INTEGER, "
      "timestamp INTEGER)",
      NULL, NULL, &errmsg);
  if (errmsg != NULL)
    {
      CRITICAL ("Failed to create table pending_messages: %s\n", errmsg);
      sqlite3_free (errmsg);
      goto out;
    }

  /* purge old entries  */
  purge_pending_messages (priv,
      TPL_LOG_STORE_SQLITE_CLEANUP_DELTA_LIMIT, &error);
  if (error != NULL)
    {
      CRITICAL ("Failed to purge pending messages: %s", error->message);
      g_error_free (error);
      goto out;
    }

  /* end of cache table init */

  /* start of counter table init */
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
      CRITICAL ("Failed to create table messagecounts: %s\n", errmsg);
      sqlite3_free (errmsg);
      goto out;
    }
  /* end of counter table init */

out:
  g_free (filename);
}


static void
tpl_log_store_sqlite_dispose (GObject *self)
{
  TplLogStoreSqlitePrivate *priv = TPL_LOG_STORE_SQLITE (self)->priv;

  if (priv->db != NULL)
    {
      sqlite3_close (priv->db);
      priv->db = NULL;
    }

  G_OBJECT_CLASS (_tpl_log_store_sqlite_parent_class)->dispose (self);
}


static void
_tpl_log_store_sqlite_class_init (TplLogStoreSqliteClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructor = tpl_log_store_sqlite_constructor;
  gobject_class->get_property = tpl_log_store_sqlite_get_property;
  gobject_class->set_property = tpl_log_store_sqlite_set_property;
  gobject_class->dispose = tpl_log_store_sqlite_dispose;

  g_object_class_override_property (gobject_class, PROP_NAME, "name");
  g_object_class_override_property (gobject_class, PROP_READABLE, "readable");
  g_object_class_override_property (gobject_class, PROP_WRITABLE, "writable");

  g_type_class_add_private (gobject_class, sizeof (TplLogStoreSqlitePrivate));
}


static const char *
get_account_name (TpAccount *account)
{
  return tp_proxy_get_object_path (account) +
    strlen (TP_ACCOUNT_OBJECT_PATH_BASE);
}


static const char *
get_account_name_from_event (TplEvent *event)
{
  return tpl_event_get_account_path (event) +
    strlen (TP_ACCOUNT_OBJECT_PATH_BASE);
}


static const char *
get_channel_name (TpChannel *chan)
{
  return tp_proxy_get_object_path (chan) +
    strlen (TP_CONN_OBJECT_PATH_BASE);
}


static char *
get_date (TplEvent *event)
{
  GDateTime *ts;
  gchar *date;

  ts = g_date_time_new_from_unix_utc (tpl_event_get_timestamp (event));
  date = g_date_time_format (ts, "%Y-%m-%d");

  g_date_time_unref (ts);

  return date;
}


static char *
get_datetime (gint64 timestamp)
{
  GDateTime *ts;
  gchar *date;

  ts = g_date_time_new_from_unix_utc (timestamp);
  date = g_date_time_format (ts, TPL_LOG_STORE_SQLITE_TIMESTAMP_FORMAT);

  g_date_time_unref (ts);

  return date;
}


static const char *
tpl_log_store_sqlite_get_name (TplLogStore *self)
{
  return TPL_LOG_STORE_SQLITE_NAME;
}


static gboolean
tpl_log_store_sqlite_add_message_counter (TplLogStore *self,
    TplEvent *message,
    GError **error)
{
  TplLogStoreSqlitePrivate *priv = TPL_LOG_STORE_SQLITE (self)->priv;
  const char *account, *identifier;
  gboolean chatroom;
  char *date = NULL;
  int count = 0;
  sqlite3_stmt *sql = NULL;
  gboolean retval = FALSE;
  gboolean insert = FALSE;
  int e;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (TPL_IS_TEXT_EVENT (message) == FALSE)
    {
      DEBUG ("ignoring non-text event not intersting for message-counter");
      retval = TRUE;
      goto out;
    }

  DEBUG ("message received");

  account = get_account_name_from_event (message);
  identifier = _tpl_event_get_target_id (message);
  chatroom = _tpl_event_target_is_room (message);
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
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "SQL Error checking current counter in %s: %s", G_STRFUNC,
          sqlite3_errmsg (priv->db));

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
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "SQL Error binding counter checking query in %s: %s", G_STRFUNC,
          sqlite3_errmsg (priv->db));

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
        "INSERT INTO messagecounts "
          "(messages, account, identifier, chatroom, date) "
        "VALUES (?, ?, ?, ?, date(?))",
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
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "SQL Error preparing query in %s: %s", G_STRFUNC,
          sqlite3_errmsg (priv->db));

      goto out;
    }

  sqlite3_bind_int (sql, 1, count);
  sqlite3_bind_text (sql, 2, account, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text (sql, 3, identifier, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (sql, 4, chatroom);
  sqlite3_bind_text (sql, 5, date, -1, SQLITE_TRANSIENT);

  e = sqlite3_step (sql);
  if (e != SQLITE_DONE)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "SQL Error %s counter in %s: %s",
          (insert ? "inserting new" : "updating"),
          G_STRFUNC, sqlite3_errmsg (priv->db));

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


/**
 * tpl_log_store_sqlite_add_event:
 * @self: TplLogstoreSqlite instance
 * @message: a TplEvent instance
 * @error: memory pointer use in case of error
 *
 * Text messages will be accounted for statistics purpose.
 *
 * Returns: %TRUE if @self was able to store, %FALSE with @error set if an error occurred.
 */
static gboolean
tpl_log_store_sqlite_add_event (TplLogStore *self,
    TplEvent *message,
    GError **error)
{
  gboolean retval = FALSE;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!TPL_IS_LOG_STORE_SQLITE (self))
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "TplLogStoreSqlite intance needed");
      goto out;
    }

  if (!TPL_IS_EVENT (message))
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT, "TplEvent instance needed");
      goto out;
    }

  retval = tpl_log_store_sqlite_add_message_counter (self, message, error);

out:
  /* check that we set an error if appropriate */
  g_assert ((retval == TRUE && *error == NULL) ||
            (retval == FALSE && *error != NULL));

  DEBUG ("returning with %d", retval);
  return retval;
}


static GList *
tpl_log_store_sqlite_get_entities (TplLogStore *self,
    TpAccount *account)
{
  TplLogStoreSqlitePrivate *priv = TPL_LOG_STORE_SQLITE (self)->priv;
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
      TplEntity *entity;
      const char *identifier;
      gboolean chatroom;
      TplEntityType type;

      /* for some reason this returns unsigned char */
      identifier = (const char *) sqlite3_column_text (sql, 0);
      chatroom = sqlite3_column_int (sql, 1);
      type = chatroom ? TPL_ENTITY_ROOM : TPL_ENTITY_CONTACT;

      DEBUG ("identifier = %s, chatroom = %i", identifier, chatroom);

      entity = tpl_entity_new (identifier, type, NULL, NULL);

      list = g_list_prepend (list, entity);
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
  iface->get_name = tpl_log_store_sqlite_get_name;
  iface->add_event = tpl_log_store_sqlite_add_event;
  iface->get_entities = tpl_log_store_sqlite_get_entities;
}

TplLogStore *
_tpl_log_store_sqlite_dup (void)
{
  return g_object_new (TPL_TYPE_LOG_STORE_SQLITE, NULL);
}


/**
 * _tpl_log_store_sqlite_get_pending_messages:
 * @self: a TplLogStoreSqlite instance
 * @channel: a pointer to a TpChannel
 * @error: set if an error occurs
 *
 * Returns the list of pending message IDs and timestamp for this @channel.
 * Note that those message might not be valid anymore, check timestamp match
 * before assuming a message is not new.
 *
 * Returns: (transfer full): a #GList of #TplLogStoreSqlitePendingMessage
 */
GList *
_tpl_log_store_sqlite_get_pending_messages (TplLogStore *self,
    TpChannel *channel,
    GError **error)
{
  TplLogStoreSqlitePrivate *priv = TPL_LOG_STORE_SQLITE (self)->priv;
  sqlite3_stmt *sql = NULL;
  GList *retval = NULL;
  int e;

  g_return_val_if_fail (TPL_IS_LOG_STORE_SQLITE (self), NULL);
  g_return_val_if_fail (TP_IS_CHANNEL (channel), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  DEBUG ("Listing pending messages for channel %s",
      get_channel_name (channel));

  e = sqlite3_prepare_v2 (priv->db, "SELECT id,timestamp "
      "FROM pending_messages "
      "WHERE channel=? "
      "ORDER BY id ASC",
      -1, &sql, NULL);

  if (e != SQLITE_OK)
    {
      CRITICAL ("Error preparing SQL for pending messages list: %s",
          sqlite3_errmsg (priv->db));
      g_set_error (error, TPL_LOG_STORE_SQLITE_ERROR,
          TPL_LOG_STORE_SQLITE_ERROR_GET_PENDING_MESSAGES,
          "SQL Error in %s: %s", G_STRFUNC, sqlite3_errmsg (priv->db));
      goto out;
    }

  sqlite3_bind_text (sql, 1, get_channel_name (channel), -1,
      SQLITE_TRANSIENT);

  while (SQLITE_ROW == (e = sqlite3_step (sql)))
    {
      /* create the pending messages list */
      TplPendingMessage *pending;

      pending = g_new (TplPendingMessage, 1);

      pending->id = (guint) sqlite3_column_int64 (sql, 0);
      pending->timestamp = sqlite3_column_int64 (sql, 1);

      DEBUG (" - pending id=%u timestamp=%"G_GINT64_FORMAT,
          pending->id, pending->timestamp);

      retval = g_list_prepend (retval, pending);
    }

  if (e != SQLITE_DONE)
    {
      g_set_error (error, TPL_LOG_STORE_SQLITE_ERROR,
          TPL_LOG_STORE_SQLITE_ERROR_GET_PENDING_MESSAGES,
          "SQL Error in %s: %s", G_STRFUNC, sqlite3_errmsg (priv->db));

      /* free partial result, which might be misleading */
      g_list_foreach (retval, (GFunc) g_free, NULL);
      g_list_free (retval);
      retval = NULL;
    }

out:
  if (sql != NULL)
    sqlite3_finalize (sql);

  /* check that we set an error if appropriate
   * NOTE: retval == NULL && *error !=
   * NULL doesn't apply to this method, since NULL is also for an empty list */
  g_assert ((retval != NULL && *error == NULL) || retval == NULL);

  return retval;
}


/**
 *_tpl_log_store_sqlite_remove_pending_messages:
 * @self: a #TplLogStore
 * @channel: a #TpAccount
 * @pending_ids: a #GList of pending message IDs (guint)
 * @error: a #GError to be set on error, or NULL
 *
 * Removes listed pending IDs for @channel.
 *
 * Returns: #TRUE on success, #FALSE on error with @error set
 */
gboolean
_tpl_log_store_sqlite_remove_pending_messages (TplLogStore *self,
    TpChannel *channel,
    GList *pending_ids,
    GError **error)
{
  TplLogStoreSqlitePrivate *priv = TPL_LOG_STORE_SQLITE (self)->priv;
  gboolean retval = TRUE;
  GString *query = NULL;
  GList *it;
  sqlite3_stmt *sql = NULL;

  g_return_val_if_fail (TPL_IS_LOG_STORE_SQLITE (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (pending_ids != NULL, FALSE);

  DEBUG ("Removing pending messages for channel %s",
      get_channel_name (channel));

  query = g_string_new ("DELETE FROM pending_messages WHERE ");

  g_string_append_printf (query, "channel='%s' AND id IN (%u",
      get_channel_name (channel), GPOINTER_TO_UINT (pending_ids->data));

  DEBUG (" - pending_id: %u", GPOINTER_TO_UINT (pending_ids->data));

  for (it = g_list_next (pending_ids); it != NULL; it = g_list_next (it))
    {
      DEBUG (" - pending_id: %u", GPOINTER_TO_UINT (it->data));
      g_string_append_printf (query, ",%u", GPOINTER_TO_UINT (it->data));
    }

  g_string_append (query, ")");

  if (sqlite3_prepare_v2 (priv->db, query->str, -1, &sql, NULL) != SQLITE_OK)
    {
      g_set_error (error, TPL_LOG_STORE_SQLITE_ERROR,
          TPL_LOG_STORE_SQLITE_ERROR_REMOVE_PENDING_MESSAGES,
          "SQL Error in %s: %s", G_STRFUNC, sqlite3_errmsg (priv->db));
      retval = FALSE;
      goto out;
    }

  if (sqlite3_step (sql) != SQLITE_DONE)
    {
      g_set_error (error, TPL_LOG_STORE_SQLITE_ERROR,
          TPL_LOG_STORE_SQLITE_ERROR_REMOVE_PENDING_MESSAGES,
          "SQL Error in %s: %s", G_STRFUNC, sqlite3_errmsg (priv->db));
      retval = FALSE;
      goto out;
    }

out:
  if (query != NULL)
    g_string_free (query, TRUE);

  if (sql != NULL)
    sqlite3_finalize (sql);

  return retval;
}

/**
 *_tpl_log_store_sqlite_add_pending_message:
 * @self: a #TplLogStore
 * @channel: a #TpChannel
 * @pending_msg_id: the pending message ID
 * @timestamp: a unix utc timestamp
 * @error: a #GError to be set on error, or NULL
 *
 * Add an entry to the list of pending message.
 *
 * Returns: #TRUE on success, #FALSE on error with @error set
 */
gboolean
_tpl_log_store_sqlite_add_pending_message (TplLogStore *self,
    TpChannel *channel,
    guint id,
    gint64 timestamp,
    GError **error)
{
  TplLogStoreSqlitePrivate *priv = TPL_LOG_STORE_SQLITE (self)->priv;
  gboolean retval = FALSE;
  const gchar *channel_path;
  gchar *date = NULL;
  sqlite3_stmt *sql = NULL;
  int e;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  channel_path = get_channel_name (channel);
  date = get_datetime (timestamp);

  DEBUG ("Caching pending message %u", id);
  DEBUG (" - channel = %s", channel_path);
  DEBUG (" - date = %s", date);

  if (TPL_STR_EMPTY (channel_path)
      || timestamp <= 0)
    {
      g_set_error_literal (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_SQLITE_ERROR_ADD_PENDING_MESSAGE,
          "passed LogStore has at least one of the needed properties unset: "
          "channel-path, timestamp");
      goto out;
    }

  e = sqlite3_prepare_v2 (priv->db,
      "INSERT INTO pending_messages (channel, id, timestamp) VALUES (?, ?, ?)",
      -1, &sql, NULL);
  if (e != SQLITE_OK)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_SQLITE_ERROR_ADD_PENDING_MESSAGE,
          "SQL Error in %s: %s", G_STRFUNC, sqlite3_errmsg (priv->db));
      goto out;
    }

  sqlite3_bind_text (sql, 1, channel_path, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (sql, 2, (gint) id);
  sqlite3_bind_int64 (sql, 3, timestamp);

  e = sqlite3_step (sql);
  if (e != SQLITE_DONE)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_SQLITE_ERROR_ADD_PENDING_MESSAGE,
          "SQL Error bind in %s: %s", G_STRFUNC, sqlite3_errmsg (priv->db));
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


gint64
_tpl_log_store_sqlite_get_most_recent (TplLogStoreSqlite *self,
    TpAccount *account,
    const char *identifier)
{
  TplLogStoreSqlitePrivate *priv = TPL_LOG_STORE_SQLITE (self)->priv;
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
_tpl_log_store_sqlite_get_frequency (TplLogStoreSqlite *self,
    TpAccount *account,
    const char *identifier)
{
  TplLogStoreSqlitePrivate *priv = TPL_LOG_STORE_SQLITE (self)->priv;
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
