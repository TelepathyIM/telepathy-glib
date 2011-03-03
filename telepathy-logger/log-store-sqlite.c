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
 */

#include <config.h>

#include <string.h>

#include <telepathy-glib/telepathy-glib.h>
#include <sqlite3.h>

#include "event-internal.h"
#include "text-event.h"
#include "text-event-internal.h"
#include "entity-internal.h"
#include "log-store-sqlite-internal.h"
#include "log-manager-internal.h"

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include "debug-internal.h"
#include "util-internal.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
      TPL_TYPE_LOG_STORE_SQLITE, TplLogStoreSqlitePrivate))

#define TPL_LOG_STORE_SQLITE_NAME "Sqlite"


static void log_store_iface_init (TplLogStoreInterface *iface);
static gboolean _insert_to_cache_table (TplLogStore *self,
    TplEvent *message, GError **error);
static void tpl_log_store_sqlite_purge (TplLogStoreSqlite *self, GTimeSpan delta,
    GError **error);
static gboolean purge_event_timeout (gpointer logstore);


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

typedef struct _TplLogStoreSqlitePrivate TplLogStoreSqlitePrivate;
struct _TplLogStoreSqlitePrivate
{
  sqlite3 *db;

  guint purge_id;
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
tpl_log_store_sqlite_dispose (GObject *self)
{
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);

  if (priv->db != NULL)
    {
      sqlite3_close (priv->db);
      priv->db = NULL;
    }

  if (priv->purge_id != 0)
    {
      g_source_remove (priv->purge_id);
      priv->purge_id = 0;
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

static void
_tpl_log_store_sqlite_init (TplLogStoreSqlite *self)
{
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);
  char *filename = get_db_filename ();
  int e;
  char *errmsg = NULL;

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
  sqlite3_exec (priv->db, "CREATE TABLE IF NOT EXISTS message_cache ( "
      "channel TEXT NOT NULL, "
      "account TEXT NOT NULL, "
      "pending_msg_id INTEGER DEFAULT NULL, "
      "log_identifier TEXT PRIMARY KEY, "
      "chat_identifier TEXT NOT NULL, "
      "chatroom BOOLEAN NOT NULL, "
      "date DATETIME NOT NULL)",
      NULL, NULL, &errmsg);
  if (errmsg != NULL)
    {
      CRITICAL ("Failed to create table message_cache: %s\n", errmsg);
      sqlite3_free (errmsg);
      goto out;
    }

  /* purge old entries every hour (60*60 secs) and purges 24h old entries */
  priv->purge_id = g_timeout_add_seconds (60*60, purge_event_timeout, self);

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

static const char *
get_channel_name_from_event (TplEvent *event)
{
  return _tpl_event_get_channel_path (event) +
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
get_datetime (TplEvent *event)
{
  GDateTime *ts;
  gchar *date;

  ts = g_date_time_new_from_unix_utc (tpl_event_get_timestamp (event));
  date = g_date_time_format (ts, TPL_LOG_STORE_SQLITE_TIMESTAMP_FORMAT);

  g_date_time_unref (ts);

  return date;
}

static const char *
tpl_log_store_sqlite_get_name (TplLogStore *self)
{
  return TPL_LOG_STORE_SQLITE_NAME;
}

/* returns log-id if present, NULL if not present */
static gchar *
_cache_msg_id_is_present (TplLogStore *self,
  TpChannel *channel,
  guint msg_id)
{
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);
  sqlite3_stmt *sql = NULL;
  gchar *retval = NULL;
  int e;

  g_return_val_if_fail (TPL_IS_LOG_STORE_SQLITE (self), NULL);
  g_return_val_if_fail (TP_IS_CHANNEL (channel), NULL);

  /* get all the (chan,msg_id) couples, the most recent first */
  e = sqlite3_prepare_v2 (priv->db,
      "SELECT log_identifier "
      "FROM message_cache "
      "WHERE channel=? AND pending_msg_id=? "
      "GROUP BY date",
      -1, &sql, NULL);

  if (e != SQLITE_OK)
    {
      CRITICAL ("Error preparing SQL to check msg_id %d for channel %s"
          " presence: %s", msg_id, get_channel_name (channel),
          sqlite3_errmsg (priv->db));
      goto out;
    }

  sqlite3_bind_text (sql, 1, get_channel_name (channel), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (sql, 2, msg_id);

  e = sqlite3_step (sql);
  /* return the first (most recent) event if a raw is found */
  if (e == SQLITE_ROW)
    retval = g_strdup ((const gchar *) sqlite3_column_text (sql, 0));
  else if (e == SQLITE_ERROR)
    CRITICAL ("SQL Error: %s", sqlite3_errmsg (priv->db));

out:
  if (sql != NULL)
    sqlite3_finalize (sql);

  return retval;
}


/**
 * _tpl_log_store_sqlite_log_id_is_present:
 * @self: A TplLogStoreSqlite
 * @log_id: the log identifier token
 *
 * Checks if @log_id is present in DB or not.
 *
 * Note that absence of @log_id in the current Sqlite doesn't mean
 * that the message has never been logged. Sqlite currently maintains a record
 * of recent log identifier (currently fresher than 5 days).
 *
 * This method can be safely used for a just arrived or just acknowledged
 * message.
 *
 * Returns: %TRUE if @log_id is found, %FALSE otherwise
 */
gboolean
_tpl_log_store_sqlite_log_id_is_present (TplLogStore *self,
  const gchar* log_id)
{
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);
  sqlite3_stmt *sql = NULL;
  gboolean retval = TRUE; /* TRUE = present, which usually is a failure */
  int e;

  g_return_val_if_fail (TPL_IS_LOG_STORE_SQLITE (self), FALSE);
  g_return_val_if_fail (!TPL_STR_EMPTY (log_id), FALSE);

  e = sqlite3_prepare_v2 (priv->db, "SELECT log_identifier "
      "FROM message_cache "
      "WHERE log_identifier=?",
      -1, &sql, NULL);
  if (e != SQLITE_OK)
    {
      CRITICAL ("Error preparing SQL to check log_id %s presence: %s",
          log_id, sqlite3_errmsg (priv->db));
      goto out;
    }

  sqlite3_bind_text (sql, 1, log_id, -1, SQLITE_TRANSIENT);

  e = sqlite3_step (sql);
  if (e == SQLITE_DONE)
    {
      DEBUG ("msg id %s not found, returning FALSE", log_id);
      retval = FALSE;
    }
  else if (e == SQLITE_ROW)
    DEBUG ("msg id %s found, returning TRUE", log_id);
  else if (e != SQLITE_ROW)
    CRITICAL ("SQL Error: %s", sqlite3_errmsg (priv->db));

out:
  if (sql != NULL)
    sqlite3_finalize (sql);

  return retval;
}


static gboolean
tpl_log_store_sqlite_add_message_counter (TplLogStore *self,
    TplEvent *message,
    GError **error)
{
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);
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
      DEBUG ("ignoring msg %s, not interesting for message-counter",
          _tpl_event_get_log_id (message));
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

static gboolean
tpl_log_store_sqlite_add_message_cache (TplLogStore *self,
    TplEvent *message,
    GError **error)
{
  const char *log_id;
  gboolean retval = FALSE;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  log_id = _tpl_event_get_log_id (message);
  DEBUG ("received %s, considering if can be cached", log_id);
  if (_tpl_log_store_sqlite_log_id_is_present (self, log_id))
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_PRESENT,
          "in %s: log-id already logged: %s", G_STRFUNC, log_id);

      goto out;
    }

  DEBUG ("caching %s", log_id);
  retval = _insert_to_cache_table (self, message, error);

out:
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
 * @message will be sent to the MessageCounter and MessageSqlite tables.
 *
 * MessageSqlite will accept any instance of TplEvent for @message and will
 * return %FALSE with @error set when a fatal error occurs or when @message
 * has already been logged.
 * For the last case a TPL_LOG_STORE_ERROR_PRESENT will be set as error
 * code in @error, and is considered fatal, since it should never happen.
 *
 * A module implementing a TplChannel should always check for TplEvent
 * log-id presence in the cache log-store if there is a chance to receive the
 * same log-id twice.
 *
 * MessageCounter only handles Text messages, which means that it will
 * silently (ie won't use @error) not log @message, when it won't be an
 * instance ot TplTextEvent, returning anyway %TRUE. This means "I could
 * store @message, but I'm discarding it because I'm not interested in it" and
 * is not cosidered an error (@error won't be set).
 * It will return %FALSE with @error set if a fatal error occurred, for
 * example it wasn't able to store it.
 *
 *
 * Returns: %TRUE if @self was able to store, %FALSE with @error set if an error occurred.
 * An already logged log-id or a failure in the persistence layer will make
 * this method return %FALSE with @error set.
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

  retval = tpl_log_store_sqlite_add_message_cache (self, message, error);
  if (retval == FALSE)
    /* either the message has already been log, or a SQLite fatal error
     * occurred, I won't update the counter table */
    goto out;

  retval = tpl_log_store_sqlite_add_message_counter (self, message, error);

out:
  /* check that we set an error if appropriate */
  g_assert ((retval == TRUE && *error == NULL) ||
            (retval == FALSE && *error != NULL));

  DEBUG ("returning with %d", retval);
  return retval;
}

static gboolean
_insert_to_cache_table (TplLogStore *self,
    TplEvent *message,
    GError **error)
{
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);
  const char *account, *channel, *identifier, *log_id;
  gboolean chatroom;
  char *date = NULL;
  gint msg_id;
  sqlite3_stmt *sql = NULL;
  gboolean retval = FALSE;
  int e;

  if (!TPL_IS_TEXT_EVENT (message))
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "Message not handled by this log store");

      goto out;
    }

  account = get_account_name_from_event (message);
  channel = get_channel_name_from_event (message);
  identifier = _tpl_event_get_target_id (message);
  log_id = _tpl_event_get_log_id (message);
  msg_id = _tpl_text_event_get_pending_msg_id (TPL_TEXT_EVENT (message));
  chatroom = _tpl_event_target_is_room (message);
  date = get_datetime (message);

  DEBUG ("channel = %s", channel);
  DEBUG ("account = %s", account);
  DEBUG ("chat_identifier = %s", identifier);
  DEBUG ("log_identifier = %s", log_id);
  DEBUG ("pending_msg_id = %d (%s)", msg_id,
      (TPL_TEXT_EVENT_MSG_ID_IS_VALID (msg_id) ?
       "pending" : "acknowledged or sent"));
  DEBUG ("chatroom = %i", chatroom);
  DEBUG ("date = %s", date);

  if (TPL_STR_EMPTY (account) || TPL_STR_EMPTY (channel) ||
      TPL_STR_EMPTY (log_id) || TPL_STR_EMPTY (date))
    {
      g_set_error_literal (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "passed LogStore has at least one of the needed properties unset: "
          "account-path, channel-path, log-id, timestamp");

      goto out;
    }

  e = sqlite3_prepare_v2 (priv->db,
      "INSERT INTO message_cache "
      "(channel, account, pending_msg_id, log_identifier, "
      "chat_identifier, chatroom, date) "
      "VALUES (?, ?, ?, ?, ?, ?, datetime(?))",
      -1, &sql, NULL);
  if (e != SQLITE_OK)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "SQL Error in %s: %s", G_STRFUNC, sqlite3_errmsg (priv->db));

      goto out;
    }

  sqlite3_bind_text (sql, 1, channel, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text (sql, 2, account, -1, SQLITE_TRANSIENT);
  /* insert NULL if ACKNOWLEDGED (ie sent message's entries, which are created
   * ACK'd */
  if (!TPL_TEXT_EVENT_MSG_ID_IS_VALID (msg_id))
    sqlite3_bind_null (sql, 3);
  else
    sqlite3_bind_int (sql, 3, msg_id);
  sqlite3_bind_text (sql, 4, log_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text (sql, 5, identifier, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (sql, 6, chatroom);
  sqlite3_bind_text (sql, 7, date, -1, SQLITE_TRANSIENT);

  e = sqlite3_step (sql);
  if (e != SQLITE_DONE)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
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

/**
 * _tpl_log_store_sqlite_get_log_ids:
 * @self: a TplLogStoreSqlite instance
 * @channel: a pointer to a TpChannel or NULL
 * @timestamp: selects entries which timestamp is older than @timestamp.
 *  use %G_MAXUINT to obtain all the entries.
 * @error: set if an error occurs
 *
 * It gets all the log-ids for messages matching the object-path of
 * @channel and older than @timestamp.
 *
 * If @channel is %NULL, it will get all the existing log-ids.
 *
 * All the entries will be filtered against @timestamp, returning only log-ids
 * older than this value (gint64). Set it to %G_MAXINT64 or any other value in
 * the future to obtain all the entries.
 * For example, to obtain entries older than one day ago, use
 * @timestamp = (now - 86400)
 *
 * Note that (in case @channel is not %NULL) this method might return log-ids
 * which are not currently related to @channel but just share the object-path,
 * in fact it is possible that an channel-path is reused over time but referring
 * to two completely different channels.
 * There is no way to understand if a channel-path is actually related to a
 * specific TpChannel instance with the same path or not, just knowking its
 * path.
 * This is not a problem, though, since log-ids are unique within TPL. If two
 * log-ids match, they relates to the same TplEvent instance.
 *
 * Returns: a list of log-id
 */
GList *
_tpl_log_store_sqlite_get_log_ids (TplLogStore *self,
    TpChannel *channel,
    gint64 unix_timestamp,
    GError **error)
{
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);
  sqlite3_stmt *sql = NULL;
  GList *retval = NULL;
  GDateTime *timestamp;
  gchar *date;
  int e;

  g_return_val_if_fail (TPL_IS_LOG_STORE_SQLITE (self), NULL);

  if (channel == NULL)
    /* get the the log-id older than date */
    e = sqlite3_prepare_v2 (priv->db, "SELECT log_identifier "
        "FROM message_cache "
        "WHERE date<datetime(?)",
        -1, &sql, NULL);
  else
    /* get the log-ids related to channel and older than date */
    e = sqlite3_prepare_v2 (priv->db, "SELECT log_identifier "
        "FROM message_cache "
        "WHERE date<datetime(?) AND channel=?",
        -1, &sql, NULL);
  if (e != SQLITE_OK)
    {
      CRITICAL ("Error preparing SQL for log-id list: %s",
          sqlite3_errmsg (priv->db));
      goto out;
    }

  timestamp = g_date_time_new_from_unix_utc (unix_timestamp);
  date = g_date_time_format (timestamp,
      TPL_LOG_STORE_SQLITE_TIMESTAMP_FORMAT);
  sqlite3_bind_text (sql, 1, date, -1, SQLITE_TRANSIENT);

  g_date_time_unref (timestamp);
  g_free (date);

  if (channel != NULL)
    sqlite3_bind_text (sql, 2, get_channel_name (channel), -1,
        SQLITE_TRANSIENT);

  /* create the log-id list */
  while (SQLITE_ROW == (e = sqlite3_step (sql)))
    {
      gchar *log_id = g_strdup ((const gchar *) sqlite3_column_text (sql, 0));
      retval = g_list_prepend (retval, log_id);
    }

  if (e != SQLITE_DONE)
    {
      g_set_error (error, TPL_LOG_STORE_SQLITE_ERROR,
          TPL_LOG_STORE_SQLITE_ERROR_GET_PENDING_MESSAGES,
          "SQL Error in %s: %s", G_STRFUNC, sqlite3_errmsg (priv->db));
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
 * _tpl_log_store_sqlite_get_pending_messages:
 * @self: a TplLogStoreSqlite instance
 * @channel: a pointer to a TpChannel or NULL
 * @error: set if an error occurs
 *
 * It gets all the log-ids for messages matching the object-path of
 * @channel and which are still set as not acknowledged in the persisten
 * layer.
 * If @channel is %NULL, it will get all the pending messages in the
 * persistence layer, not filtering against any channel.
 *
 * Note that (in case @channel is not %NULL) this method might return log-ids
 * which are not currently related to @channel but just share the object-path,
 * in fact it is possible that an channel-path is reused over time but referring
 * to two completely different channels.
 * There is no way to understand if a channel-path is actually related to a
 * specific TpChannel instance with the same path or not, just knowking its
 * path.
 * This is not a problem, though, since log-ids are unique within TPL. If two
 * log-ids match, they relates to the same TplEvent instance.
 *
 * Returns: a list of log-id
 */
GList *
_tpl_log_store_sqlite_get_pending_messages (TplLogStore *self,
    TpChannel *channel,
    GError **error)
{
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);
  sqlite3_stmt *sql = NULL;
  GList *retval = NULL;
  int e;

  g_return_val_if_fail (TPL_IS_LOG_STORE_SQLITE (self), NULL);
  g_return_val_if_fail (TPL_IS_CHANNEL (channel) || channel == NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (channel == NULL)
    /* get all the pending log-ids */
    e = sqlite3_prepare_v2 (priv->db, "SELECT log_identifier "
        "FROM message_cache "
        "WHERE pending_msg_id is NOT NULL",
        -1, &sql, NULL);
  else
    /* get the pending log-ids related to channel */
    e = sqlite3_prepare_v2 (priv->db, "SELECT log_identifier "
        "FROM message_cache "
        "WHERE pending_msg_id is NOT NULL AND channel=?",
        -1, &sql, NULL);
  if (e != SQLITE_OK)
    {
      CRITICAL ("Error preparing SQL for pending messages list: %s",
          sqlite3_errmsg (priv->db));
      goto out;
    }

  if (channel != NULL)
    sqlite3_bind_text (sql, 1, get_channel_name (channel), -1,
        SQLITE_TRANSIENT);

  while (SQLITE_ROW == (e = sqlite3_step (sql)))
    {
      /* create the pending messages list */
      gchar *log_id = g_strdup ((const gchar *) sqlite3_column_text (sql, 0));
      retval = g_list_prepend (retval, log_id);
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

void
_tpl_log_store_sqlite_set_acknowledgment_by_msg_id (TplLogStore *self,
    TpChannel *channel,
    guint msg_id,
    GError **error)
{
  gchar *log_id = NULL;

  g_return_if_fail (error == NULL || *error == NULL);
  g_return_if_fail (TPL_IS_LOG_STORE_SQLITE (self));
  g_return_if_fail (TP_IS_CHANNEL (channel));

  log_id = _cache_msg_id_is_present (self, channel, msg_id);

  if (log_id != NULL)
    {
      DEBUG ("%s: found %s for pending id %d", get_channel_name (channel),
          log_id, msg_id);
      _tpl_log_store_sqlite_set_acknowledgment (self, log_id, error);
    }
  else
    g_set_error (error, TPL_LOG_STORE_ERROR,
        TPL_LOG_STORE_ERROR_NOT_PRESENT,
        "Unable to acknowledge pending message %d for channel %s: not found",
        msg_id, get_channel_name (channel));

  g_free (log_id);
}

void
_tpl_log_store_sqlite_set_acknowledgment (TplLogStore *self,
    const gchar* log_id,
    GError **error)
{
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);
  sqlite3_stmt *sql = NULL;
  int e;

  g_return_if_fail (error == NULL || *error == NULL);
  g_return_if_fail (TPL_IS_LOG_STORE_SQLITE (self));
  g_return_if_fail (!TPL_STR_EMPTY (log_id));

  if (!_tpl_log_store_sqlite_log_id_is_present (TPL_LOG_STORE (self), log_id))
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_NOT_PRESENT,
          "log_id %s not found", log_id);
      goto out;
    }

  e = sqlite3_prepare_v2 (priv->db, "UPDATE message_cache "
      "SET pending_msg_id=NULL "
      "WHERE log_identifier=?", -1, &sql, NULL);
  if (e != SQLITE_OK)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "SQL Error in %s: %s", G_STRFUNC, sqlite3_errmsg (priv->db));

      goto out;
    }

  sqlite3_bind_text (sql, 1, log_id, -1, SQLITE_TRANSIENT);

  e = sqlite3_step (sql);
  if (e != SQLITE_DONE)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "SQL Error in %s: %s", G_STRFUNC, sqlite3_errmsg (priv->db));
    }

out:
  if (sql != NULL)
    sqlite3_finalize (sql);
}

static void
tpl_log_store_sqlite_purge (TplLogStoreSqlite *self,
    GTimeSpan delta,
    GError **error)
{
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);
  sqlite3_stmt *sql = NULL;
  GDateTime *now;
  GDateTime *timestamp;
  gchar *date;
  int e;

  g_return_if_fail (error == NULL || *error == NULL);
  g_return_if_fail (TPL_IS_LOG_STORE_SQLITE (self));

  now = g_date_time_new_now_utc ();
  timestamp = g_date_time_add (now, -delta);

  date = g_date_time_format (timestamp,
      TPL_LOG_STORE_SQLITE_TIMESTAMP_FORMAT);

  g_date_time_unref (now);
  g_date_time_unref (timestamp);

  DEBUG ("Purging entries older than %s (%u seconds ago)", date, (guint) delta);

  e = sqlite3_prepare_v2 (priv->db, "DELETE FROM message_cache "
      "WHERE date<datetime(?)",
      -1, &sql, NULL);

  if (e != SQLITE_OK)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "SQL Error preparing statement in %s: %s", G_STRFUNC,
          sqlite3_errmsg (priv->db));

      goto out;
    }

  sqlite3_bind_text (sql, 1, date, -1, SQLITE_TRANSIENT);

  e = sqlite3_step (sql);
  if (e != SQLITE_DONE)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_ADD_EVENT,
          "SQL Error in %s: %s", G_STRFUNC, sqlite3_errmsg (priv->db));
    }

out:
  if (sql != NULL)
    sqlite3_finalize (sql);

  g_free (date);
}

static gboolean
purge_event_timeout (gpointer logstore)
{
  GError *error = NULL;
  TplLogStoreSqlite *self = logstore;

  tpl_log_store_sqlite_purge (self, TPL_LOG_STORE_SQLITE_CLEANUP_DELTA_LIMIT,
      &error);
  if (error != NULL)
    {
      CRITICAL ("Unable to purge entries: %s", error->message);
      g_error_free (error);
    }

  /* return TRUE to avoid g_timeout_add_seconds cancel the operation */
  return TRUE;
}

static GList *
tpl_log_store_sqlite_get_entities (TplLogStore *self,
    TpAccount *account)
{
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);
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

gint64
_tpl_log_store_sqlite_get_most_recent (TplLogStoreSqlite *self,
    TpAccount *account,
    const char *identifier)
{
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);
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
  TplLogStoreSqlitePrivate *priv = GET_PRIV (self);
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
