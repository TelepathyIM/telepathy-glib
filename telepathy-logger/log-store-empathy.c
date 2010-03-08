/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 *          Jonny Lamb <jonny.lamb@collabora.co.uk>
 *          Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "config.h"
#include "log-store-empathy.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>

#include <glib-object.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <telepathy-glib/account.h>
#include <telepathy-glib/account-manager.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/contact.h>
#include <telepathy-logger/log-entry-text.h>
#include <telepathy-logger/log-manager.h>
#include <telepathy-logger/log-store.h>
#include <telepathy-logger/datetime.h>
#include <telepathy-logger/util.h>

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include <telepathy-logger/debug.h>

#define LOG_DIR_CREATE_MODE       (S_IRUSR | S_IWUSR | S_IXUSR)
#define LOG_FILE_CREATE_MODE      (S_IRUSR | S_IWUSR)
#define LOG_DIR_CHATROOMS         "chatrooms"
#define LOG_FILENAME_SUFFIX       ".log"
#define LOG_TIME_FORMAT_FULL      "%Y%m%dT%H:%M:%S"
#define LOG_TIME_FORMAT           "%Y%m%d"
#define LOG_HEADER \
    "<?xml version='1.0' encoding='utf-8'?>\n" \
    "<?xml-stylesheet type=\"text/xsl\" href=\"empathy-log.xsl\"?>\n" \
    "<log>\n"

#define LOG_FOOTER \
    "</log>\n"


#define GET_PRIV(obj) TPL_GET_PRIV (obj, TplLogStoreEmpathy)
typedef struct
{
  gchar *basedir;
  gchar *name;
  gboolean readable;
  gboolean writable;
  TpAccountManager *account_manager;
} TplLogStoreEmpathyPriv;

enum {
    PROP_0,
    PROP_NAME,
    PROP_READABLE,
    PROP_WRITABLE,
    PROP_BASEDIR
};

static void log_store_iface_init (gpointer g_iface, gpointer iface_data);
static const gchar *log_store_empathy_get_name (TplLogStore *self);
static void log_store_empathy_set_name (TplLogStore *self, const gchar *data);
static const gchar *log_store_empathy_get_basedir (TplLogStore *self);
static void log_store_empathy_set_basedir (TplLogStore *self,
    const gchar *data);
static void log_store_empathy_set_writable (TplLogStore *self, gboolean data);
static void log_store_empathy_set_readable (TplLogStore *self, gboolean data);


G_DEFINE_TYPE_WITH_CODE (TplLogStoreEmpathy, tpl_log_store_empathy,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TPL_TYPE_LOG_STORE, log_store_iface_init))

static void
tpl_log_store_empathy_dispose (GObject *object)
{
  TplLogStoreEmpathy *self = TPL_LOG_STORE_EMPATHY (object);
  TplLogStoreEmpathyPriv *priv = GET_PRIV (self);

  /* FIXME See TP-bug #25569, when dispose a non prepared TP_AM, it
     might segfault.
     To avoid it, a *klduge*, a reference in the TplObserver to
     the TplLogManager is kept, so that until TplObserver is instanced,
     there will always be a TpLogManager reference and it won't be
     diposed */
  if (priv->account_manager != NULL)
    {
      g_object_unref (priv->account_manager);
      priv->account_manager = NULL;
    }
}


static void
tpl_log_store_empathy_finalize (GObject *object)
{
  TplLogStoreEmpathy *self = TPL_LOG_STORE_EMPATHY (object);
  TplLogStoreEmpathyPriv *priv = GET_PRIV (self);

  if (priv->basedir != NULL)
    {
      g_free (priv->basedir);
      priv->basedir = NULL;
    }
  if (priv->name != NULL)
    {
      g_free (priv->name);
      priv->name = NULL;
    }
}


static void
tpl_log_store_empathy_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplLogStoreEmpathyPriv *priv = GET_PRIV (object);

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
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}


static void
tpl_log_store_empathy_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplLogStore *self = TPL_LOG_STORE (object);

  switch (param_id)
    {
      case PROP_NAME:
        log_store_empathy_set_name (self, g_value_get_string (value));
        break;
      case PROP_READABLE:
        log_store_empathy_set_readable (self, g_value_get_boolean (value));
        break;
      case PROP_WRITABLE:
        log_store_empathy_set_writable (self, g_value_get_boolean (value));
        break;
      case PROP_BASEDIR:
        log_store_empathy_set_basedir (self, g_value_get_string (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}


static void
tpl_log_store_empathy_class_init (TplLogStoreEmpathyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->finalize = tpl_log_store_empathy_finalize;
  object_class->dispose = tpl_log_store_empathy_dispose;
  object_class->get_property = tpl_log_store_empathy_get_property;
  object_class->set_property = tpl_log_store_empathy_set_property;

  g_object_class_override_property (object_class, PROP_NAME, "name");
  g_object_class_override_property (object_class, PROP_READABLE, "readable");
  g_object_class_override_property (object_class, PROP_WRITABLE, "writable");

  /* the default value for the basedir prop is composed by user_data_dir () +
   * prop "name" value, it's not possible to know it at param_spec time, so
   * it's set to NULL and let to get_basedir to set it to its default if
   * priv->basedir == NULL
   */

  /**
   * TplLogStoreEmpathy:basedir:
   *
   * The log store's basedir.
   */
  param_spec = g_param_spec_string ("basedir",
      "Basedir",
      "The TplLogStore implementation's name",
      NULL, G_PARAM_READABLE | G_PARAM_WRITABLE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_BASEDIR, param_spec);

  g_type_class_add_private (object_class, sizeof (TplLogStoreEmpathyPriv));
}


static void
tpl_log_store_empathy_init (TplLogStoreEmpathy *self)
{
  TplLogStoreEmpathyPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_LOG_STORE_EMPATHY, TplLogStoreEmpathyPriv);

  self->priv = priv;
  priv->account_manager = tp_account_manager_dup ();
}


static gchar *
log_store_account_to_dirname (TpAccount *account)
{
  const gchar *name;

  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);

  name = tp_proxy_get_object_path (account);
  if (g_str_has_prefix (name, TP_ACCOUNT_OBJECT_PATH_BASE))
    name += strlen (TP_ACCOUNT_OBJECT_PATH_BASE);

  return g_strdelimit (g_strdup (name), "/", '_');
}

/* chat_id can be NULL, but if present have to be a non zero-lenght string.
 * If NULL, the returned dir will be composed until the account part.
 * If non-NULL, the returned dir will be composed until the chat_id part */
static gchar *
log_store_empathy_get_dir (TplLogStore *self,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom)
{
  gchar *basedir;
  gchar *escaped;
  TplLogStoreEmpathyPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  /* chat_id may be NULL, but not empthy string if not-NULL */
  g_return_val_if_fail ((chat_id == NULL) || (*chat_id != '\0'), NULL);

  priv = GET_PRIV (self);

  escaped = log_store_account_to_dirname (account);

  if (chatroom)
    basedir = g_build_path (G_DIR_SEPARATOR_S,
        log_store_empathy_get_basedir (self), escaped, LOG_DIR_CHATROOMS,
        chat_id, NULL);
  else
    basedir = g_build_path (G_DIR_SEPARATOR_S,
        log_store_empathy_get_basedir (self), escaped, chat_id, NULL);

  g_free (escaped);

  return basedir;
}


static gboolean
log_store_empathy_exists (TplLogStore *self,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom)
{
  gchar *dir;
  gboolean exists;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), FALSE);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), FALSE);
  g_return_val_if_fail (!TPL_STR_EMPTY (chat_id), FALSE);

  dir = log_store_empathy_get_dir (self, account, chat_id, chatroom);
  exists = g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR);
  g_free (dir);

  return exists;
}


static GList *
log_store_empathy_get_dates (TplLogStore *self,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom)
{
  GList *dates = NULL;
  gchar *date;
  gchar *directory;
  GDir *dir;
  const gchar *filename;
  const gchar *p;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (chat_id), NULL);

  directory = log_store_empathy_get_dir (self, account, chat_id, chatroom);
  dir = g_dir_open (directory, 0, NULL);
  if (!dir)
    {
      DEBUG ("Could not open directory:'%s'", directory);
      g_free (directory);
      return NULL;
    }

  DEBUG ("Collating a list of dates in:'%s'", directory);

  while ((filename = g_dir_read_name (dir)) != NULL)
    {
      if (!g_str_has_suffix (filename, LOG_FILENAME_SUFFIX))
        continue;

      p = strstr (filename, LOG_FILENAME_SUFFIX);
      date = g_strndup (filename, p - filename);

      if (!date)
        continue;

      if (!g_regex_match_simple ("\\d{8}", date, 0, 0))
        continue;

      dates = g_list_insert_sorted (dates, date, (GCompareFunc) strcmp);
    }

  g_free (directory);
  g_dir_close (dir);

  DEBUG ("Parsed %d dates", g_list_length (dates));

  return dates;
}


static gchar *
log_store_empathy_get_filename_for_date (TplLogStore *self,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom,
    const gchar *date)
{
  gchar *basedir;
  gchar *timestamp;
  gchar *filename;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (chat_id), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (date), NULL);

  basedir = log_store_empathy_get_dir (self, account, chat_id, chatroom);
  timestamp = g_strconcat (date, LOG_FILENAME_SUFFIX, NULL);
  filename = g_build_filename (basedir, timestamp, NULL);

  g_free (basedir);
  g_free (timestamp);

  return filename;
}


static TplLogSearchHit *
log_store_empathy_search_hit_new (TplLogStore *self,
    const gchar *filename)
{
  TplLogStoreEmpathyPriv *priv = GET_PRIV (self);
  TplLogSearchHit *hit;
  gchar *account_name;
  const gchar *end;
  gchar **strv;
  guint len;
  GList *accounts, *l;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (filename), NULL);
  g_return_val_if_fail (g_str_has_suffix (filename, LOG_FILENAME_SUFFIX),
      NULL);

  strv = g_strsplit (filename, G_DIR_SEPARATOR_S, -1);
  len = g_strv_length (strv);

  hit = g_slice_new0 (TplLogSearchHit);

  end = strstr (strv[len - 1], LOG_FILENAME_SUFFIX);
  hit->date = g_strndup (strv[len - 1], end - strv[len - 1]);
  hit->chat_id = g_strdup (strv[len - 2]);
  hit->is_chatroom = (strcmp (strv[len - 3], LOG_DIR_CHATROOMS) == 0);

  if (hit->is_chatroom)
    account_name = strv[len - 4];
  else
    account_name = strv[len - 3];

  /* FIXME: This assumes the account manager is prepared, but the
   * synchronous API forces this. See bug #599189. */
  accounts = tp_account_manager_get_valid_accounts (priv->account_manager);

  for (l = accounts; l != NULL; l = g_list_next (l))
    {
      TpAccount *account = TP_ACCOUNT (l->data);
      gchar *name;

      name = log_store_account_to_dirname (account);
      if (!tp_strdiff (name, account_name))
        {
          g_assert (hit->account == NULL);
          hit->account = g_object_ref (account);
        }
      g_free (name);
    }
  g_list_free (accounts);

  hit->filename = g_strdup (filename);

  g_strfreev (strv);

  return hit;
}

/* returns a Glist of TplLogEntryText instances */
static GList *
log_store_empathy_get_messages_for_file (TplLogStore *self,
    TpAccount *account,
    const gchar *filename)
{
  GList *messages = NULL;
  xmlParserCtxtPtr ctxt;
  xmlDocPtr doc;
  xmlNodePtr log_node;
  xmlNodePtr node;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (filename), NULL);

  DEBUG ("Attempting to parse filename:'%s'...", filename);

  if (!g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      DEBUG ("Filename:'%s' does not exist", filename);
      return NULL;
    }

  /* Create parser. */
  ctxt = xmlNewParserCtxt ();

  /* Parse and validate the file. */
  doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
  if (!doc)
    {
      g_warning ("Failed to parse file:'%s'", filename);
      xmlFreeParserCtxt (ctxt);
      return NULL;
    }

  /* The root node, presets. */
  log_node = xmlDocGetRootElement (doc);
  if (!log_node)
    {
      xmlFreeDoc (doc);
      xmlFreeParserCtxt (ctxt);
      return NULL;
    }

  /* Now get the messages. */
  for (node = log_node->children; node; node = node->next)
    {
      TplLogEntryText *message;
      TplContact *sender;
      gchar *time_;
      time_t t;
      gchar *sender_id;
      gchar *sender_name;
      gchar *sender_avatar_token;
      gchar *body;
      gchar *is_user_str;
      gboolean is_user = FALSE;
      gchar *msg_type_str;
      gchar *cm_id_str;
      gint cm_id;
      gchar *instead_of_channel_path;
      gchar *log_id;
      TpChannelTextMessageType msg_type = TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;

      if (strcmp ((const gchar *) node->name, "message") != 0)
        continue;

      body = (gchar *) xmlNodeGetContent (node);
      time_ = (gchar *) xmlGetProp (node, (const xmlChar *) "time");
      sender_id = (gchar *) xmlGetProp (node, (const xmlChar *) "id");
      sender_name = (gchar *) xmlGetProp (node, (const xmlChar *) "name");
      sender_avatar_token = (gchar *) xmlGetProp (node,
          (const xmlChar *) "token");
      is_user_str = (gchar *) xmlGetProp (node, (const xmlChar *) "isuser");
      msg_type_str = (gchar *) xmlGetProp (node, (const xmlChar *) "type");
      cm_id_str = (gchar *) xmlGetProp (node, (const xmlChar *) "cm_id");

      if (is_user_str)
        is_user = (!tp_strdiff (is_user_str, "true"));

      if (msg_type_str)
        msg_type = tpl_log_entry_text_message_type_from_str (msg_type_str);

      if (cm_id_str)
        cm_id = atoi (cm_id_str);
      else
        cm_id = TPL_LOG_ENTRY_MSG_ID_ACKNOWLEDGED;

      t = tpl_time_parse (time_);

      sender = tpl_contact_new (sender_id);
      tpl_contact_set_account (sender, account);
      tpl_contact_set_alias (sender, sender_name);
      tpl_contact_set_avatar_token (sender, sender_avatar_token);

      /* in legacy Empathy LogStore there is no concept of log-id as a unique
       * token, so I'll create, just for it to be present an ad hoc unique
       * token */
      instead_of_channel_path = g_strconcat (
          tp_proxy_get_object_path (account), sender_id, NULL);
      log_id = create_message_token (instead_of_channel_path, t, cm_id);
      message = tpl_log_entry_text_new (log_id,
          tp_proxy_get_object_path (account), TPL_LOG_ENTRY_DIRECTION_NONE);
      tpl_log_entry_set_pending_msg_id (TPL_LOG_ENTRY (message), cm_id);
      tpl_log_entry_set_sender (TPL_LOG_ENTRY (message), sender);
      tpl_log_entry_set_timestamp (TPL_LOG_ENTRY (message), t);
      tpl_log_entry_text_set_message (message, body);
      tpl_log_entry_text_set_message_type (message, msg_type);
      /* TODO uderstand if useful
         tpl_log_entry_text_set_is_backlog (message, TRUE);
       */

      messages = g_list_append (messages, message);

      g_object_unref (sender);
      xmlFree (time_);
      xmlFree (sender_id);
      xmlFree (sender_name);
      xmlFree (body);
      xmlFree (is_user_str);
      xmlFree (msg_type_str);
      xmlFree (cm_id_str);
      xmlFree (sender_avatar_token);
    }

  DEBUG ("Parsed %d messages", g_list_length (messages));

  xmlFreeDoc (doc);
  xmlFreeParserCtxt (ctxt);

  return messages;
}


/* If dir is NULL, basedir will be used instead.
 * Used to make possible the full search vs. specific subtrees search */
static GList *
log_store_empathy_get_all_files (TplLogStore *self,
    const gchar *dir)
{
  GDir *gdir;
  GList *files = NULL;
  const gchar *name;
  const gchar *basedir;
  TplLogStoreEmpathyPriv *priv;

  priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  /* dir can be NULL, do not check :-) */

  basedir = (dir != NULL) ? dir : log_store_empathy_get_basedir (self);

  gdir = g_dir_open (basedir, 0, NULL);
  if (!gdir)
    return NULL;

  while ((name = g_dir_read_name (gdir)) != NULL)
    {
      gchar *filename;

      filename = g_build_filename (basedir, name, NULL);
      if (g_str_has_suffix (filename, LOG_FILENAME_SUFFIX))
        {
          files = g_list_prepend (files, filename);
          continue;
        }

      if (g_file_test (filename, G_FILE_TEST_IS_DIR))
        {
          /* Recursively get all log files */
          files = g_list_concat (files,
              log_store_empathy_get_all_files (self,
                filename));
        }

      g_free (filename);
    }

  g_dir_close (gdir);

  return files;
}


static GList *
_log_store_empathy_search_in_files (TplLogStore *self,
    const gchar *text,
    GList *files)
{
  GList *l;
  GList *hits = NULL;
  gchar *text_casefold;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (text), NULL);

  text_casefold = g_utf8_casefold (text, -1);

  for (l = files; l; l = g_list_next (l))
    {
      gchar *filename;
      GMappedFile *file;
      gsize length;
      gchar *contents;
      gchar *contents_casefold;

      filename = l->data;

      file = g_mapped_file_new (filename, FALSE, NULL);
      if (!file)
        continue;

      length = g_mapped_file_get_length (file);
      contents = g_mapped_file_get_contents (file);
      contents_casefold = g_utf8_casefold (contents, length);

      g_mapped_file_unref (file);

      if (strstr (contents_casefold, text_casefold))
        {
          TplLogSearchHit *hit;

          hit = log_store_empathy_search_hit_new (self, filename);
          if (hit != NULL)
            {
              hits = g_list_prepend (hits, hit);
              DEBUG ("Found text:'%s' in file:'%s' on date:'%s'", text,
                  hit->filename, hit->date);
            }
        }

      g_free (contents_casefold);
      g_free (filename);
    }

  g_list_free (files);
  g_free (text_casefold);

  return hits;
}


static GList *
log_store_empathy_search_in_identifier_chats_new (TplLogStore *self,
    TpAccount *account,
    gchar const *identifier,
    const gchar *text)
{
  GList *files;
  gchar *dir, *account_dir;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (identifier), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (text), NULL);

  account_dir = log_store_account_to_dirname (account);
  dir = g_build_path (G_DIR_SEPARATOR_S, log_store_empathy_get_basedir (self),
      account_dir, identifier, NULL);

  files = log_store_empathy_get_all_files (self, dir);
  DEBUG ("Found %d log files in total", g_list_length (files));

  return _log_store_empathy_search_in_files (self, text, files);
}



static GList *
log_store_empathy_search_new (TplLogStore *self,
    const gchar *text)
{
  GList *files;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (text), NULL);

  files = log_store_empathy_get_all_files (self, NULL);
  DEBUG ("Found %d log files in total", g_list_length (files));

  return _log_store_empathy_search_in_files (self, text, files);
}

/* Returns: (GList *) of (TplLogSearchHit *) */
static GList *
log_store_empathy_get_chats_for_dir (TplLogStore *self,
    const gchar *dir,
    gboolean is_chatroom)
{
  GDir *gdir;
  GList *hits = NULL;
  const gchar *name;
  GError *error = NULL;

  gdir = g_dir_open (dir, 0, &error);
  if (!gdir)
    {
      DEBUG ("Failed to open directory: %s, error: %s", dir, error->message);
      g_error_free (error);
      return NULL;
    }

  while ((name = g_dir_read_name (gdir)) != NULL)
    {
      TplLogSearchHit *hit;

      if (!is_chatroom && strcmp (name, LOG_DIR_CHATROOMS) == 0)
        {
          gchar *filename = g_build_filename (dir, name, NULL);
          hits = g_list_concat (hits,
                log_store_empathy_get_chats_for_dir (self, filename, TRUE));
          g_free (filename);
          continue;
        }
      hit = g_slice_new0 (TplLogSearchHit);
      hit->chat_id = g_strdup (name);
      hit->is_chatroom = is_chatroom;

      hits = g_list_prepend (hits, hit);
    }

  g_dir_close (gdir);

  return hits;
}


/* returns a Glist of TplLogEntryText instances */
static GList *
log_store_empathy_get_messages_for_date (TplLogStore *self,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom,
    const gchar *date)
{
  gchar *filename;
  GList *messages;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (chat_id), NULL);

  filename = log_store_empathy_get_filename_for_date (self, account, chat_id,
      chatroom, date);
  messages = log_store_empathy_get_messages_for_file (self, account,
      filename);
  g_free (filename);

  return messages;
}


static GList *
log_store_empathy_get_chats (TplLogStore *self,
    TpAccount *account)
{
  gchar *dir;
  GList *hits;
  TplLogStoreEmpathyPriv *priv;

  priv = GET_PRIV (self);

  dir = log_store_empathy_get_dir (self, account, NULL, FALSE);
  hits = log_store_empathy_get_chats_for_dir (self, dir, FALSE);
  g_free (dir);

  for (guint i = 0; i < g_list_length (hits); ++i)
    {
      TplLogSearchHit *hit;
      hit = g_list_nth_data (hits, i);
    }


  return hits;
}


static const gchar *
log_store_empathy_get_name (TplLogStore *self)
{
  TplLogStoreEmpathyPriv *priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_LOG_STORE_EMPATHY (self), NULL);

  return priv->name;
}


/* returns am absolute path for the base directory of LogStore */
static const gchar *
log_store_empathy_get_basedir (TplLogStore *self)
{
  TplLogStoreEmpathyPriv *priv = GET_PRIV (self);

  g_return_val_if_fail (TPL_IS_LOG_STORE_EMPATHY (self), NULL);

  /* set default based on name if NULL, see prop's comment about it in
   * class_init method */
  if (priv->basedir == NULL)
    {
      gchar *dir;

      dir = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
          log_store_empathy_get_name (self), "logs", NULL);
      log_store_empathy_set_basedir (self, dir);
      g_free (dir);
    }

  return priv->basedir;
}


static void
log_store_empathy_set_name (TplLogStore *self,
    const gchar *data)
{
  TplLogStoreEmpathyPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_LOG_STORE_EMPATHY (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (priv->name == NULL);

  priv->name = g_strdup (data);
}

static void
log_store_empathy_set_basedir (TplLogStore *self,
    const gchar *data)
{
  TplLogStoreEmpathyPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_LOG_STORE_EMPATHY (self));
  g_return_if_fail (priv->basedir == NULL);
  /* data may be NULL when the class is initialized and the default value is
   * set */

  priv->basedir = g_strdup (data);

  /* at install_spec time, default value is set to NULL, ignore it */
  if (priv->basedir != NULL)
    DEBUG ("logstore set to dir: %s", data);
}


static void
log_store_empathy_set_readable (TplLogStore *self,
    gboolean data)
{
  TplLogStoreEmpathyPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_LOG_STORE_EMPATHY (self));

  priv->readable = data;
}


static void
log_store_empathy_set_writable (TplLogStore *self,
    gboolean data)
{
  TplLogStoreEmpathyPriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_LOG_STORE_EMPATHY (self));

  priv->writable = data;
}


static GList *
log_store_empathy_get_filtered_messages (TplLogStore *self,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom,
    guint num_messages,
    TplLogMessageFilter filter,
    gpointer user_data)
{
  GList *dates, *l, *messages = NULL;
  guint i = 0;

  dates = log_store_empathy_get_dates (self, account, chat_id, chatroom);

  for (l = g_list_last (dates); l != NULL && i < num_messages;
       l = g_list_previous (l))
    {
      GList *new_messages, *n, *next;

      /* FIXME: We should really restrict the message parsing to get only
       * the newest num_messages. */
      new_messages = log_store_empathy_get_messages_for_date (self, account,
          chat_id, chatroom, l->data);

      n = new_messages;
      while (n != NULL)
        {
          next = g_list_next (n);
          if (!filter (n->data, user_data))
            {
              g_object_unref (n->data);
              new_messages = g_list_delete_link (new_messages, n);
            }
          else
            i++;
          n = next;
        }
      messages = g_list_concat (messages, new_messages);
    }

  g_list_foreach (dates, (GFunc) g_free, NULL);
  g_list_free (dates);

  return messages;
}

static void
log_store_iface_init (gpointer g_iface,
    gpointer iface_data)
{
  TplLogStoreInterface *iface = (TplLogStoreInterface *) g_iface;

  iface->add_message = NULL; /* read_only LogStore */
  iface->get_name = log_store_empathy_get_name;
  iface->exists = log_store_empathy_exists;
  iface->get_dates = log_store_empathy_get_dates;
  iface->get_messages_for_date = log_store_empathy_get_messages_for_date;
  iface->get_chats = log_store_empathy_get_chats;
  iface->search_in_identifier_chats_new =
    log_store_empathy_search_in_identifier_chats_new;
  iface->search_new = log_store_empathy_search_new;
  iface->get_filtered_messages = log_store_empathy_get_filtered_messages;
}
