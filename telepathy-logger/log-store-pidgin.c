/*
 * Copyright (C) 2008-2011 Collabora Ltd.
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
 * Authors: Jonny Lamb <jonny.lamb@collabora.co.uk>
 *          Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include <config.h>

#define _XOPEN_SOURCE
#include <time.h>
#include <string.h>
#include <stdio.h>

#include <telepathy-glib/telepathy-glib.h>

#include "log-store-internal.h"
#include "log-store-pidgin-internal.h"
#include "log-manager-internal.h"
#include "text-event-internal.h"
#include "entity-internal.h"
#include "util-internal.h"

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include "debug-internal.h"

#define TXT_LOG_FILENAME_SUFFIX ".txt"
#define HTML_LOG_FILENAME_SUFFIX ".html"

struct _TplLogStorePidginPriv
{
  gboolean test_mode;

  gchar *basedir;
  gchar *name;
  gboolean readable;
  gboolean writable;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_READABLE,
    PROP_WRITABLE,
    PROP_BASEDIR,
    PROP_TESTMODE,
};



static void log_store_iface_init (gpointer g_iface, gpointer iface_data);
static void tpl_log_store_pidgin_get_property (GObject *object, guint param_id, GValue *value,
    GParamSpec *pspec);
static void tpl_log_store_pidgin_set_property (GObject *object, guint param_id, const GValue *value,
    GParamSpec *pspec);
static const gchar *log_store_pidgin_get_name (TplLogStore *store);
static void log_store_pidgin_set_name (TplLogStorePidgin *self, const gchar *data);
static const gchar *log_store_pidgin_get_basedir (TplLogStorePidgin *self);
static void log_store_pidgin_set_basedir (TplLogStorePidgin *self,
    const gchar *data);
static void log_store_pidgin_set_writable (TplLogStorePidgin *self, gboolean data);
static void log_store_pidgin_set_readable (TplLogStorePidgin *self, gboolean data);


G_DEFINE_TYPE_WITH_CODE (TplLogStorePidgin, tpl_log_store_pidgin,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TPL_TYPE_LOG_STORE, log_store_iface_init));

static void
tpl_log_store_pidgin_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplLogStorePidginPriv *priv = TPL_LOG_STORE_PIDGIN (object)->priv;

  switch (param_id)
    {
      case PROP_NAME:
        g_value_set_string (value, priv->name);
        break;
      case PROP_WRITABLE:
        g_value_set_boolean (value, priv->writable);
        break;
      case PROP_READABLE:
        g_value_set_boolean (value, priv->readable);
        break;
      case PROP_BASEDIR:
        g_value_set_string (value, priv->basedir);
        break;
      case PROP_TESTMODE:
        g_value_set_boolean (value, priv->test_mode);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}


static void
tpl_log_store_pidgin_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplLogStorePidgin *self = TPL_LOG_STORE_PIDGIN (object);

  switch (param_id)
    {
      case PROP_NAME:
        log_store_pidgin_set_name (self, g_value_get_string (value));
        break;
      case PROP_READABLE:
        log_store_pidgin_set_readable (self, g_value_get_boolean (value));
        break;
      case PROP_WRITABLE:
        log_store_pidgin_set_writable (self, g_value_get_boolean (value));
        break;
      case PROP_BASEDIR:
        log_store_pidgin_set_basedir (self, g_value_get_string (value));
        break;
      case PROP_TESTMODE:
        self->priv->test_mode = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}


static void
tpl_log_store_pidgin_dispose (GObject *self)
{
  TplLogStorePidginPriv *priv = TPL_LOG_STORE_PIDGIN (self)->priv;

  g_free (priv->basedir);
  priv->basedir = NULL;

  g_free (priv->name);
  priv->name = NULL;

  G_OBJECT_CLASS (tpl_log_store_pidgin_parent_class)->dispose (self);
}


static void
tpl_log_store_pidgin_class_init (TplLogStorePidginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->get_property = tpl_log_store_pidgin_get_property;
  object_class->set_property = tpl_log_store_pidgin_set_property;
  object_class->dispose = tpl_log_store_pidgin_dispose;

  g_object_class_override_property (object_class, PROP_NAME, "name");
  g_object_class_override_property (object_class, PROP_READABLE, "readable");
  g_object_class_override_property (object_class, PROP_WRITABLE, "writable");

  /**
   * TplLogStorePidgin:basedir:
   *
   * The log store's basedir.
   */
  param_spec = g_param_spec_string ("basedir",
      "Basedir",
      "The directory where the LogStore will look for data",
      NULL, G_PARAM_READABLE | G_PARAM_WRITABLE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_BASEDIR, param_spec);


  param_spec = g_param_spec_boolean ("testmode",
      "TestMode",
      "Whether the logstore is in testmode, for testsuite use only",
      FALSE, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TESTMODE, param_spec);


  g_type_class_add_private (object_class, sizeof (TplLogStorePidginPriv));
}


static void
tpl_log_store_pidgin_init (TplLogStorePidgin *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_LOG_STORE_PIDGIN, TplLogStorePidginPriv);
}


static const gchar *
log_store_pidgin_get_name (TplLogStore *store)
{
  TplLogStorePidgin *self = (TplLogStorePidgin *) store;

  g_return_val_if_fail (TPL_IS_LOG_STORE_PIDGIN (self), NULL);

  return self->priv->name;
}


/* returns an absolute path for the base directory of LogStore */
static const gchar *
log_store_pidgin_get_basedir (TplLogStorePidgin *self)
{
  g_return_val_if_fail (TPL_IS_LOG_STORE_PIDGIN (self), NULL);

  /* If basedir isn't yet set (defaults to NULL), use the libpurple default
   * location, useful for testing logstore with a different basedir */
  if (self->priv->basedir == NULL)
    {
      gchar *dir;

      if (self->priv->test_mode && g_getenv ("TPL_TEST_LOG_DIR") != NULL)
        dir = g_build_path (G_DIR_SEPARATOR_S, g_getenv ("TPL_TEST_LOG_DIR"),
            "purple", NULL);
      else
        dir = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".purple",
            "logs", NULL);
      log_store_pidgin_set_basedir (self, dir);

      g_free (dir);
    }

  return self->priv->basedir;
}


static void
log_store_pidgin_set_name (TplLogStorePidgin *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_LOG_STORE_PIDGIN (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->name == NULL);

  self->priv->name = g_strdup (data);
}


static void
log_store_pidgin_set_basedir (TplLogStorePidgin *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_LOG_STORE_PIDGIN (self));
  g_return_if_fail (self->priv->basedir == NULL);
  /* data may be NULL when the class is initialized and the default value is
   * set */

  self->priv->basedir = g_strdup (data);

  /* at install_spec time, default value is set to NULL, ignore it */
  if (self->priv->basedir != NULL)
    DEBUG ("logstore set to dir: %s", data);
}


static void
log_store_pidgin_set_readable (TplLogStorePidgin *self,
    gboolean data)
{
  g_return_if_fail (TPL_IS_LOG_STORE_PIDGIN (self));

  self->priv->readable = data;
}


static void
log_store_pidgin_set_writable (TplLogStorePidgin *self,
    gboolean data)
{
  g_return_if_fail (TPL_IS_LOG_STORE_PIDGIN (self));

  self->priv->writable = data;
}


/* internal: get the full name of the storing directory, including protocol
 * and id */
static gchar *
log_store_pidgin_get_dir (TplLogStore *self,
    TpAccount *account,
    TplEntity *target)
{
  const gchar *protocol;
  gchar *basedir;
  gchar *username, *normalized, *tmp;
  gchar *id = NULL; /* if not NULL, it contains a modified version of
                       target id, to be g_free'd */
  const GHashTable *params;

  params = tp_account_get_parameters (account);
  protocol = tp_account_get_protocol (account);

  if (tp_strdiff (protocol, "irc") == 0)
    {
      const gchar *account_param, *server;

      account_param = tp_asv_get_string (params, "account");
      server = tp_asv_get_string (params, "server");

      username = g_strdup_printf ("%s@%s", account_param, server);
    }
  else
    {
      username = g_strdup (tp_asv_get_string (params, "account"));
    }

  if (username == NULL)
    {
      DEBUG ("Failed to get account");
      return NULL;
    }

  normalized = g_utf8_normalize (username, -1, G_NORMALIZE_DEFAULT);
  g_free (username);

  if (target != NULL)
    {
      const gchar *orig_id = tpl_entity_get_identifier (target);

      if (tpl_entity_get_entity_type (target) == TPL_ENTITY_ROOM)
        id = g_strdup_printf ("%s.chat", orig_id);
      else if (g_str_has_suffix (orig_id, "#1"))
        /* Small butterfly workaround */
        id = g_strndup (orig_id, strlen (orig_id) - 2);
      else
        id = g_strdup (orig_id);
    }

  tmp = g_uri_escape_string (normalized, "#@", TRUE);
  g_free (normalized);
  normalized = tmp; /* now normalized and escaped */

  /* purple basedir + protocol name + account name + recipient id */
  basedir = g_build_path (G_DIR_SEPARATOR_S,
      log_store_pidgin_get_basedir (TPL_LOG_STORE_PIDGIN (self)),
      protocol,
      normalized,
      id,
      NULL);

  g_free (id);
  g_free (normalized);

  return basedir;
}


/* public: returns whether some data for @id exist in @account */
static gboolean
log_store_pidgin_exists (TplLogStore *self,
    TpAccount *account,
    TplEntity *target,
    gint type_mask)
{
  gchar *dir;
  gboolean exists;

  if (!(type_mask & TPL_EVENT_MASK_TEXT))
    return FALSE;

  dir = log_store_pidgin_get_dir (self, account, target);

  if (dir != NULL)
    exists = g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR);
  else
    exists = FALSE;

  g_free (dir);

  return exists;
}


/* internal */
static GDate *
log_store_pidgin_get_time (const gchar *filename)
{
  gchar *date;
  GDate *retval = NULL;
  const gchar *p;

  gint year;
  gint month;
  gint day;

  if (filename == NULL)
    return NULL;

  if (g_str_has_suffix (filename, TXT_LOG_FILENAME_SUFFIX))
    {
      p = strstr (filename, TXT_LOG_FILENAME_SUFFIX);
      date = g_strndup (filename, p - filename);
    }
  else if (g_str_has_suffix (filename, HTML_LOG_FILENAME_SUFFIX))
    {
      p = strstr (filename, HTML_LOG_FILENAME_SUFFIX);
      date = g_strndup (filename, p - filename);
    }
  else
    {
      date = g_strdup (filename);
    }

  sscanf (date, "%4d-%2d-%2d.*s", &year, &month, &day);

  DEBUG ("date is %s", date);
  retval = g_date_new_dmy (day, month, year);
  g_free (date);

  return retval;
}


static GList *
log_store_pidgin_get_dates (TplLogStore *self,
    TpAccount *account,
    TplEntity *target,
    gint type_mask)
{
  GList *dates = NULL;
  gchar *directory;
  GDir *dir;
  const gchar *filename;

  g_return_val_if_fail (TPL_IS_LOG_STORE_PIDGIN (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (TPL_IS_ENTITY (target), NULL);

  if (!(type_mask & TPL_EVENT_MASK_TEXT))
    return NULL;

  directory = log_store_pidgin_get_dir (self, account, target);

  if (directory == NULL)
    return NULL;

  dir = g_dir_open (directory, 0, NULL);
  if (dir == NULL)
    {
      DEBUG ("Could not open directory:'%s'", directory);
      g_free (directory);
      return NULL;
    }

  DEBUG ("Collating a list of dates in: '%s'", directory);

  while ((filename = g_dir_read_name (dir)) != NULL)
    {
      GDate *date;

      if (!g_str_has_suffix (filename, TXT_LOG_FILENAME_SUFFIX)
          && !g_str_has_suffix (filename, HTML_LOG_FILENAME_SUFFIX))
        continue;
      DEBUG ("%s: %s %s\n", G_STRFUNC, directory, filename);

      date = log_store_pidgin_get_time (filename);
      dates = g_list_insert_sorted (dates, date, (GCompareFunc) g_date_compare);
    }

  g_free (directory);
  g_dir_close (dir);

  DEBUG ("Parsed %d dates", g_list_length (dates));

  return dates;
}


static GList *
log_store_pidgin_get_filenames_for_date (TplLogStore *self,
    TpAccount *account,
    TplEntity *target,
    const GDate *date)
{
  gchar *basedir;
  gchar timestamp[11];
  GList *filenames = NULL;
  GDir *dir;
  const gchar *dirfile;

  basedir = log_store_pidgin_get_dir (self, account, target);

  if (basedir == NULL)
    return NULL;

  dir = g_dir_open (basedir, 0, NULL);
  if (dir == NULL)
    {
      g_free (basedir);
      return NULL;
    }

  g_date_strftime (timestamp, 11, "%F", date);

  while ((dirfile = g_dir_read_name (dir)) != NULL)
    {
      if (!g_str_has_suffix (dirfile, TXT_LOG_FILENAME_SUFFIX)
          && !g_str_has_suffix (dirfile, HTML_LOG_FILENAME_SUFFIX))
        continue;

      if (g_str_has_prefix (dirfile, timestamp))
        {
          filenames = g_list_insert_sorted (filenames,
              g_build_filename (basedir, dirfile, NULL),
              (GCompareFunc) g_strcmp0);
        }
    }

  g_dir_close (dir);

  g_free (basedir);

  return filenames;
}


static TpAccount *
log_store_pidgin_dup_account (const gchar *filename)
{
  GList *accounts, *l;
  TpAccount *account = NULL;
  TpAccountManager *account_manager;
  gchar **strv;
  guint len;
  gchar *protocol, *username, *server = NULL, *tmp;
  gboolean is_irc;

  account_manager = tp_account_manager_dup ();
  accounts = tp_account_manager_get_valid_accounts (account_manager);

  strv = g_strsplit (filename, G_DIR_SEPARATOR_S, -1);
  len = g_strv_length (strv);

  protocol = strv[len - 4];
  tmp = strchr (strv[len - 3], '@');
  is_irc = !tp_strdiff (protocol, "irc");

  if (is_irc && tmp != NULL)
    {
      username = g_strndup (strv[len - 3], tmp - strv[len - 3]);
      server = g_strdup (strv[len - 3] + (tmp - strv[len - 3]) + 1);
    }
  else
    {
      username = g_strdup (strv[len - 3]);
    }

  /* You can have multiple accounts with the same username so we have to
   * look at all the accounts to find the right one going on the username and
   * protocol. */
  for (l = accounts; l != NULL; l = l->next)
    {
      TpAccount *acc = (TpAccount *) l->data;
      const GHashTable *params;

      if (tp_strdiff (tp_account_get_protocol (acc), protocol))
        continue;

      params = tp_account_get_parameters (acc);

      if (!tp_strdiff (username, tp_asv_get_string (params, "account")))
        {
          if (is_irc && tp_strdiff (server, tp_asv_get_string (params, "server")))
            continue;

          account = g_object_ref (acc);
          break;
        }
    }

  g_free (username);
  g_free (server);
  g_list_free (accounts);
  g_strfreev (strv);
  g_object_unref (account_manager);

  return account;
}


static TplLogSearchHit *
log_store_pidgin_search_hit_new (TplLogStore *self,
    const gchar *filename)
{
  TplLogSearchHit *hit;
  gchar **strv;
  guint len;
  TplEntityType type;
  gchar *id;

  if (!g_str_has_suffix (filename, TXT_LOG_FILENAME_SUFFIX)
      && !g_str_has_suffix (filename, HTML_LOG_FILENAME_SUFFIX))
    return NULL;

  strv = g_strsplit (filename, G_DIR_SEPARATOR_S, -1);
  len = g_strv_length (strv);

  hit = g_slice_new0 (TplLogSearchHit);
  hit->date = log_store_pidgin_get_time (strv[len-1]);

  type = g_str_has_suffix (strv[len-2], ".chat")
    ? TPL_ENTITY_ROOM : TPL_ENTITY_CONTACT;

  /* Remove ".chat" suffix. */
  if (type == TPL_ENTITY_ROOM)
    id = g_strndup (strv[len-2], (strlen (strv[len-2]) - 5));
  else
    id = g_strdup (strv[len-2]);

  hit->target = tpl_entity_new (id, type, NULL, NULL);

  g_free (id);

  hit->account = log_store_pidgin_dup_account (filename);

  g_strfreev (strv);

  return hit;
}


static GList *
log_store_pidgin_get_events_for_files (TplLogStore *self,
    TpAccount *account,
    const GList *filenames)
{
  GList *events = NULL;
  const GList *l;

  g_return_val_if_fail (TPL_IS_LOG_STORE_PIDGIN (self), NULL);
  g_return_val_if_fail (filenames != NULL, NULL);

  for (l = filenames; l != NULL; l = l->next)
    {
      const gchar *filename;

      gchar *target_id = NULL;
      gchar *date = NULL;
      gchar *own_user = NULL;
      gchar *protocol = NULL;
      gboolean is_room;
      gchar *dirname;
      gchar *date_str;
      gchar *basename;
      gchar **split;

      gchar *buffer;
      GError *error = NULL;
      gchar **lines;
      int i;

      GRegex *regex;
      GMatchInfo *match_info;
      gchar **hits = NULL;
      gboolean is_html = FALSE;

      filename = (gchar *) l->data;

      DEBUG ("Attempting to parse filename:'%s'...", filename);

      if (!g_file_test (filename, G_FILE_TEST_EXISTS))
        {
          DEBUG ("Filename:'%s' does not exist", filename);
          continue;
        }

      if (!g_file_get_contents (filename, &buffer, NULL, &error))
        {
          DEBUG ("Failed to read file: %s",
              error ? error->message : "no event");
          g_error_free (error);
          continue;
        }

      dirname = g_path_get_dirname (filename);
      is_room = g_str_has_suffix (dirname, ".chat");
      g_free (dirname);

      basename = g_path_get_basename (filename);
      split = g_strsplit_set (basename, "-.", 4);

      if (g_strv_length (split) < 3)
        {
          DEBUG ("Unexpected filename: %s (expected YYYY-MM-DD ...)",
              basename);
          g_strfreev (split);
          g_free (basename);
          g_free (buffer);
          continue;
        }

      date_str = g_strdup_printf ("%s%s%sT", split[0], split[1], split[2]);
      g_free (basename);
      g_strfreev (split);

      lines = g_strsplit (buffer, "\n", -1);

      g_free (buffer);

      is_html = g_str_has_suffix (filename, HTML_LOG_FILENAME_SUFFIX);

      if (is_html)
        {
          regex = g_regex_new ("<h3>Conversation with (.+) at (.+) on (.+) \\((.+)\\)</h3>",
              0, 0, NULL);
        }
      else
        {
          regex = g_regex_new ("Conversation with (.+) at (.+) on (.+) \\((.+)\\)",
              0, 0, NULL);
        }

      if (lines[0] != NULL)
        {
          g_regex_match (regex, lines[0], 0, &match_info);
          hits = g_match_info_fetch_all (match_info);

          g_match_info_free (match_info);
        }

      g_regex_unref (regex);

      if (hits == NULL)
        {
          g_strfreev (lines);
          continue;
        }

      if (g_strv_length (hits) != 5)
        {
          g_strfreev (lines);
          g_strfreev (hits);
          continue;
        }

      target_id = g_strdup (hits[1]);
      own_user = g_strdup (hits[3]);
      protocol = g_strdup (hits[4]);

      g_strfreev (hits);

      for (i = 1; lines[i] != NULL; i++)
        {
          TplTextEvent *event;
          TplEntity *sender;
          TplEntity *receiver = NULL;
          gchar *sender_name = NULL;
          gchar *time_str = NULL;
          gchar *timestamp_str = NULL;
          gchar *body = NULL;
          int j = i + 1;
          gboolean is_user = FALSE;
          gint64 timestamp;

          if (is_html)
            {
              if (!tp_strdiff (lines[i], "</body></html>"))
                break;

              regex = g_regex_new (
                  "<font size=\"2\">\\((.+)\\)</font> <b>(.+):</b></font> (<body>|)(.*)(</body>|)<br/>$",
                  G_REGEX_UNGREEDY, 0, NULL);
            }
          else
            {
              regex = g_regex_new ("^\\((.+)\\) (.+): (.+)", 0, 0, NULL);
            }

          g_regex_match (regex, lines[i], 0, &match_info);
          hits = g_match_info_fetch_all (match_info);

          g_match_info_free (match_info);
          g_regex_unref (regex);

          if (hits == NULL
              || (is_html && g_strv_length (hits) < 5)
              || (g_strv_length (hits) < 4))
            {
              g_strfreev (hits);
              continue;
            }

          time_str = g_strdup (hits[1]);
          sender_name = g_strdup (hits[2]);

          if (is_html)
            {
              GRegex *r;

              r = g_regex_new ("<br/>", 0, 0, NULL);
              body = g_regex_replace (r, hits[4], -1, 0, "\n", 0, NULL);
              g_regex_unref (r);

              is_user = strstr (lines[i], "16569E") != NULL;
            }
          else
            {
              body = g_strdup (hits[3]);
            }

          g_strfreev (hits);

          /* time_str -> "%H:%M:%S" */
          timestamp_str = g_strdup_printf ("%s%s", date_str, time_str);
          timestamp = _tpl_time_parse (timestamp_str);
          g_free (timestamp_str);

          /* Unfortunately, there's no way to tell which user is you in plain
           * text logs as they appear like this:
           *
           * Conversation with contacts@jid at date on my@jid (protocol)
           * (10:17:18) Some Person: hello
           * (10:17:19) Another person: hey
           *
           * We can hack around it in the HTML logs because we know what
           * colour the local user will be displayed as. sigh.
           */

          /* FIXME: in text format (is_html==FALSE) there is no actual way to
           * understand what type the entity is, it might lead to inaccuracy,
           * as is_user will be always FALSE  */
          sender = tpl_entity_new (
              is_user ? own_user : sender_name,
              is_user ? TPL_ENTITY_SELF : TPL_ENTITY_CONTACT,
              sender_name, NULL);

          /* FIXME: in text format it's not possible to guess who is the
           * receiver (unless we are in a room). In this case the receiver will
           * be left to NULL in the generated event. */
          if (is_html || is_room)
            {
              const gchar *receiver_id;
              TplEntityType receiver_type;

              /* In chatrooms, the receiver is always the room */
              if (is_room)
                {
                  receiver_id = target_id;
                  receiver_type = TPL_ENTITY_ROOM;
                }
              else if (is_user)
                {
                  receiver_id = target_id;
                  receiver_type = TPL_ENTITY_CONTACT;
                }
              else
                {
                  receiver_id = own_user;
                  receiver_type = TPL_ENTITY_SELF;
                }

              receiver = tpl_entity_new (receiver_id, receiver_type,
                  NULL, NULL);
            }

          event = g_object_new (TPL_TYPE_TEXT_EVENT,
              /* TplEvent */
              "account", account,
              /* MISSING: "channel-path", channel_path, */
              "receiver", receiver,
              "sender", sender,
              "timestamp", timestamp,
              /* TplTextEvent */
              "message-type", TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
              "message", body,
              NULL);

          /* prepend and then reverse is better than append */
          events = g_list_prepend (events, event);

          g_free (sender_name);
          g_free (time_str);
          g_object_unref (sender);

          i = j - 1;
        }
      events = g_list_reverse (events);

      g_free (target_id);
      g_free (own_user);
      g_free (date);
      g_free (protocol);

      g_strfreev (lines);
    }

  DEBUG ("Parsed %d events", g_list_length (events));

  return events;
}


/* internal: return a GList of file names (char *) which need to be freed with
 * g_free */
static GList *
log_store_pidgin_get_all_files (TplLogStore *self,
    const gchar *dir)
{
  GDir *gdir;
  GList *files = NULL;
  const gchar *name;
  const gchar *basedir;


  basedir = (dir != NULL) ?
    dir : log_store_pidgin_get_basedir (TPL_LOG_STORE_PIDGIN (self));

  gdir = g_dir_open (basedir, 0, NULL);
  if (gdir == NULL)
    return NULL;

  while ((name = g_dir_read_name (gdir)) != NULL)
    {
      gchar *filename;

      filename = g_build_filename (basedir, name, NULL);
      if (g_str_has_suffix (filename, TXT_LOG_FILENAME_SUFFIX)
          || g_str_has_suffix (filename, HTML_LOG_FILENAME_SUFFIX))
        {
          files = g_list_prepend (files, filename);
          continue;
        }

      if (g_file_test (filename, G_FILE_TEST_IS_DIR))
        {
          files = g_list_concat (files,
              log_store_pidgin_get_all_files (self, filename));
        }

      g_free (filename);
    }

  g_dir_close (gdir);

  return files;
}


static GList *
_log_store_pidgin_search_in_files (TplLogStorePidgin *self,
    const gchar *text,
    GList *files)
{
  GList *l;
  GList *hits = NULL;
  gchar *text_casefold;

  text_casefold = g_utf8_casefold (text, -1);

  for (l = files; l != NULL; l = l->next)
    {
      gchar *filename;
      GMappedFile *file;
      gsize length;
      gchar *contents;
      gchar *contents_casefold = NULL;

      filename = l->data;

      file = g_mapped_file_new (filename, FALSE, NULL);
      if (file == NULL)
        continue;

      length = g_mapped_file_get_length (file);
      contents = g_mapped_file_get_contents (file);

      if (contents != NULL)
        contents_casefold = g_utf8_casefold (contents, length);

      g_mapped_file_unref (file);

      if (contents_casefold == NULL)
        continue;

      if (strstr (contents_casefold, text_casefold))
        {
          TplLogSearchHit *hit;

          hit = log_store_pidgin_search_hit_new (TPL_LOG_STORE (self),
              filename);

          if (hit != NULL)
            {
              hits = g_list_prepend (hits, hit);
              DEBUG ("Found text:'%s' in file:'%s' on date:'%04u-%02u-%02u'",
                  text_casefold, filename, g_date_get_year (hit->date),
                  g_date_get_month (hit->date), g_date_get_day (hit->date));
            }
        }

      g_free (contents_casefold);
    }

  g_free (text_casefold);

  return hits;
}


static GList *
log_store_pidgin_search_new (TplLogStore *self,
    const gchar *text,
    gint type_mask)
{
  GList *files;
  GList *retval;

  g_return_val_if_fail (TPL_IS_LOG_STORE_PIDGIN (self), NULL);
  g_return_val_if_fail (!tp_str_empty (text), NULL);

  if (!(type_mask & TPL_EVENT_MASK_TEXT))
    return NULL;

  files = log_store_pidgin_get_all_files (self, NULL);
  DEBUG ("Found %d log files in total", g_list_length (files));

  retval = _log_store_pidgin_search_in_files (TPL_LOG_STORE_PIDGIN (self),
      text, files);

  g_list_foreach (files, (GFunc) g_free, NULL);
  g_list_free (files);

  return retval;
}


static GList *
log_store_pidgin_get_entities_for_dir (TplLogStore *self,
    const gchar *dir)
{
  GDir *gdir;
  GList *entities = NULL;
  const gchar *name;

  gdir = g_dir_open (dir, 0, NULL);
  if (gdir == NULL)
    return NULL;

  while ((name = g_dir_read_name (gdir)) != NULL)
    {
      TplEntity *entity;

      /* pidgin internal ".system" directory is not a target ID */
      if (g_strcmp0 (name, ".system") == 0)
        continue;

      /* Check if it's a chatroom */
      if (g_str_has_suffix (name, ".chat"))
        {
          gchar *id = g_strndup (name, strlen (name) - 5);
          entity = tpl_entity_new_from_room_id (id);
          g_free (id);
        }
      else
        entity = tpl_entity_new (name, TPL_ENTITY_CONTACT, NULL, NULL);

      entities = g_list_prepend (entities, entity);
    }

  g_dir_close (gdir);

  return entities;
}


static GList *
log_store_pidgin_get_events_for_date (TplLogStore *self,
    TpAccount *account,
    TplEntity *target,
    gint type_mask,
    const GDate *date)
{
  GList *events, *filenames;

  g_return_val_if_fail (TPL_IS_LOG_STORE_PIDGIN (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (TPL_IS_ENTITY (target), NULL);

  if (!(type_mask & TPL_EVENT_MASK_TEXT))
    return NULL;

  /* pidgin stores multiple files related to the same date */
  filenames = log_store_pidgin_get_filenames_for_date (self, account,
      target, date);

  if (filenames == NULL)
    return NULL;

  events = log_store_pidgin_get_events_for_files (self, account, filenames);

  g_list_foreach (filenames, (GFunc) g_free, NULL);
  g_list_free (filenames);

  return events;
}


static GList *
log_store_pidgin_get_entities (TplLogStore *self,
    TpAccount *account)
{
  gchar *dir;
  GList *hits;

  dir = log_store_pidgin_get_dir (self, account, NULL);

  if (dir != NULL)
    hits = log_store_pidgin_get_entities_for_dir (self, dir);
  else
    hits = NULL;

  g_free (dir);

  return hits;
}


static GList *
log_store_pidgin_get_filtered_events (TplLogStore *self,
    TpAccount *account,
    TplEntity *target,
    gint type_mask,
    guint num_events,
    TplLogEventFilter filter,
    gpointer user_data)
{
  GList *dates, *l, *events = NULL;
  guint i = 0;

  dates = log_store_pidgin_get_dates (self, account, target, type_mask);

  for (l = g_list_last (dates); l != NULL && i < num_events; l = l->prev)
    {
      GList *new_events, *n, *next;

      /* FIXME: We should really restrict the event parsing to get only
       * the newest num_events. */
      new_events = log_store_pidgin_get_events_for_date (self, account,
          target, type_mask, l->data);

      n = new_events;
      while (n != NULL)
        {
          next = n->next;
          if (filter != NULL && !filter (n->data, user_data))
            {
              g_object_unref (n->data);
              new_events = g_list_delete_link (new_events, n);
            }
          else
            {
              i++;
            }
          n = next;
        }
      events = g_list_concat (events, new_events);
    }

  g_list_foreach (dates, (GFunc) g_free, NULL);
  g_list_free (dates);

  return events;
}


static void
log_store_iface_init (gpointer g_iface,
    gpointer iface_data)
{
  TplLogStoreInterface *iface = (TplLogStoreInterface *) g_iface;

  iface->get_name = log_store_pidgin_get_name;
  iface->exists = log_store_pidgin_exists;
  iface->add_event = NULL;
  iface->get_dates = log_store_pidgin_get_dates;
  iface->get_events_for_date = log_store_pidgin_get_events_for_date;
  iface->get_entities = log_store_pidgin_get_entities;
  iface->search_new = log_store_pidgin_search_new;
  iface->get_filtered_events = log_store_pidgin_get_filtered_events;
}
