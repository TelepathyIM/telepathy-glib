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
 */

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>

#include <telepathy-glib/account-manager.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/defs.h>

#include "empathy-log-store.h"
#include "empathy-log-store-empathy.h"
#include "empathy-log-manager.h"
#include "empathy-contact.h"
#include "empathy-time.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include "empathy-debug.h"

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


#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, TplLogStoreEmpathy)
typedef struct
{
  gchar *basedir;
  gchar *name;
  TpAccountManager *account_manager;
} TplLogStoreEmpathyPriv;

static void log_store_iface_init (gpointer g_iface,gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (TplLogStoreEmpathy, tpl_log_store_empathy,
    G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_LOG_STORE,
      log_store_iface_init));

static void
log_store_empathy_finalize (GObject *object)
{
  TplLogStoreEmpathy *self = TPL_LOG_STORE_EMPATHY (object);
  TplLogStoreEmpathyPriv *priv = GET_PRIV (self);

  g_object_unref (priv->account_manager);
  g_free (priv->basedir);
  g_free (priv->name);
}

static void
tpl_log_store_empathy_class_init (TplLogStoreEmpathyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = log_store_empathy_finalize;

  g_type_class_add_private (object_class, sizeof (TplLogStoreEmpathyPriv));
}

static void
tpl_log_store_empathy_init (TplLogStoreEmpathy *self)
{
  TplLogStoreEmpathyPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_LOG_STORE_EMPATHY, TplLogStoreEmpathyPriv);

  self->priv = priv;

  priv->basedir = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
    PACKAGE_NAME, "logs", NULL);

  priv->name = g_strdup ("Empathy");
  priv->account_manager = tp_account_manager_dup ();
}

static gchar *
log_store_account_to_dirname (TpAccount *account)
{
  const gchar *name;

  name = tp_proxy_get_object_path (account);
  if (g_str_has_prefix (name, TP_ACCOUNT_OBJECT_PATH_BASE))
    name += strlen (TP_ACCOUNT_OBJECT_PATH_BASE);

  return g_strdelimit (g_strdup (name), "/", '_');
}


static gchar *
log_store_empathy_get_dir (TplLogStore *self,
                           TpAccount *account,
                           const gchar *chat_id,
                           gboolean chatroom)
{
  gchar *basedir;
  gchar *escaped;
  TplLogStoreEmpathyPriv *priv;

  priv = GET_PRIV (self);

  escaped = log_store_account_to_dirname (account);

  if (chatroom)
    basedir = g_build_path (G_DIR_SEPARATOR_S, priv->basedir, escaped,
        LOG_DIR_CHATROOMS, chat_id, NULL);
  else
    basedir = g_build_path (G_DIR_SEPARATOR_S, priv->basedir,
        escaped, chat_id, NULL);

  g_free (escaped);

  return basedir;
}

static gchar *
log_store_empathy_get_timestamp_filename (void)
{
  time_t t;
  gchar *time_str;
  gchar *filename;

  t = empathy_time_get_current ();
  time_str = empathy_time_to_string_local (t, LOG_TIME_FORMAT);
  filename = g_strconcat (time_str, LOG_FILENAME_SUFFIX, NULL);

  g_free (time_str);

  return filename;
}

static gchar *
log_store_empathy_get_timestamp_from_message (TplLogEntry *message)
{
  time_t t;

  t = empathy_message_get_timestamp (message);

  /* We keep the timestamps in the messages as UTC. */
  return empathy_time_to_string_utc (t, LOG_TIME_FORMAT_FULL);
}

static gchar *
log_store_empathy_get_filename (TplLogStore *self,
                                TpAccount *account,
                                const gchar *chat_id,
                                gboolean chatroom)
{
  gchar *basedir;
  gchar *timestamp;
  gchar *filename;

  basedir = log_store_empathy_get_dir (self, account, chat_id, chatroom);
  timestamp = log_store_empathy_get_timestamp_filename ();
  filename = g_build_filename (basedir, timestamp, NULL);

  g_free (basedir);
  g_free (timestamp);

  return filename;
}

static gboolean
log_store_empathy_add_message (TplLogStore *self,
                               const gchar *chat_id,
                               gboolean chatroom,
                               TplLogEntry *message,
                               GError **error)
{
  FILE *file;
  TpAccount *account;
  TpContact *sender;
  const gchar *body_str;
  const gchar *str;
  //EmpathyAvatar *avatar;
  //gchar *avatar_token = NULL;
  gchar *filename;
  gchar *basedir;
  gchar *body;
  gchar *timestamp;
  gchar *contact_name;
  gchar *contact_id;
  TpChannelTextMessageType msg_type;

  g_return_val_if_fail (TPL_IS_LOG_STORE (self), FALSE);
  g_return_val_if_fail (chat_id != NULL, FALSE);
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (message), FALSE);

  sender = tpl_log_entry_text_get_sender (message);
  account = tpl_channel_get_account (
	tpl_log_entry_text_get_channel (message) );
  body_str = tpl_log_entry_text_get_message (message);
  msg_type = tpl_log_entry_text_get_message_type (message);


  if (EMP_STR_EMPTY (body_str))
    return FALSE;

  filename = log_store_empathy_get_filename (self, account, chat_id, chatroom);
  basedir = g_path_get_dirname (filename);
  if (!g_file_test (basedir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
    {
      DEBUG ("Creating directory:'%s'", basedir);
      g_mkdir_with_parents (basedir, LOG_DIR_CREATE_MODE);
    }
  g_free (basedir);

  DEBUG ("Adding message: '%s' to file: '%s'", body_str, filename);

  if (!g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      file = g_fopen (filename, "w+");
      if (file != NULL)
        g_fprintf (file, LOG_HEADER);

      g_chmod (filename, LOG_FILE_CREATE_MODE);
    }
  else
    {
      file = g_fopen (filename, "r+");
      if (file != NULL)
        fseek (file, - strlen (LOG_FOOTER), SEEK_END);
    }

  body = g_markup_escape_text (body_str, -1);
  timestamp = log_store_empathy_get_timestamp_from_message (message);

  str = tp_contact_get_alias (sender);
  contact_name = g_markup_escape_text (str, -1);

  str = tp_contact_get_id (sender);
  contact_id = g_markup_escape_text (str, -1);
/*
  avatar = empathy_contact_get_avatar (sender);
  if (avatar != NULL)
    avatar_token = g_markup_escape_text (avatar->token, -1);
*/
  g_fprintf (file,
       "<message time='%s' cm_id='%d' id='%s' name='%s' token='%s' isuser='%s' type='%s'>"
       "%s</message>\n" LOG_FOOTER, timestamp,
       empathy_message_get_id (message),
       contact_id, contact_name,
       avatar_token ? avatar_token : "",
       empathy_contact_is_user (sender) ? "true" : "false",
       empathy_message_type_to_str (msg_type), body);

  fclose (file);
  g_free (filename);
  g_free (contact_id);
  g_free (contact_name);
  g_free (timestamp);
  g_free (body);
  g_free (avatar_token);

  return TRUE;
}

static gboolean
log_store_empathy_exists (TplLogStore *self,
                          TpAccount *account,
                          const gchar *chat_id,
                          gboolean chatroom)
{
  gchar *dir;
  gboolean exists;

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
  g_return_val_if_fail (chat_id != NULL, NULL);

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

  basedir = log_store_empathy_get_dir (self, account, chat_id, chatroom);
  timestamp = g_strconcat (date, LOG_FILENAME_SUFFIX, NULL);
  filename = g_build_filename (basedir, timestamp, NULL);

  g_free (basedir);
  g_free (timestamp);

  return filename;
}

static EmpathyLogSearchHit *
log_store_empathy_search_hit_new (TplLogStore *self,
                                  const gchar *filename)
{
  TplLogStoreEmpathyPriv *priv = GET_PRIV (self);
  EmpathyLogSearchHit *hit;
  gchar *account_name;
  const gchar *end;
  gchar **strv;
  guint len;
  GList *accounts, *l;

  if (!g_str_has_suffix (filename, LOG_FILENAME_SUFFIX))
    return NULL;

  strv = g_strsplit (filename, G_DIR_SEPARATOR_S, -1);
  len = g_strv_length (strv);

  hit = g_slice_new0 (EmpathyLogSearchHit);

  end = strstr (strv[len-1], LOG_FILENAME_SUFFIX);
  hit->date = g_strndup (strv[len-1], end - strv[len-1]);
  hit->chat_id = g_strdup (strv[len-2]);
  hit->is_chatroom = (strcmp (strv[len-3], LOG_DIR_CHATROOMS) == 0);

  if (hit->is_chatroom)
    account_name = strv[len-4];
  else
    account_name = strv[len-3];

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
          hit->account = account;
          g_object_ref (account);
        }
      g_free (name);
    }
  g_list_free (accounts);

  hit->filename = g_strdup (filename);

  g_strfreev (strv);

  return hit;
}

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

  g_return_val_if_fail (EMPATHY_IS_LOG_STORE (self), NULL);
  g_return_val_if_fail (filename != NULL, NULL);

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
      TplLogEntry *message;
      EmpathyContact *sender;
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
      guint cm_id;
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
        is_user = strcmp (is_user_str, "true") == 0;

      if (msg_type_str)
        msg_type = empathy_message_type_from_str (msg_type_str);

      if (cm_id_str)
        cm_id = atoi (cm_id_str);

      t = empathy_time_parse (time_);

      sender = empathy_contact_new_for_log (account, sender_id, sender_name,
					    is_user);

      if (!EMP_STR_EMPTY (sender_avatar_token))
        empathy_contact_load_avatar_cache (sender,
            sender_avatar_token);

      message = empathy_message_new (body);
      empathy_message_set_sender (message, sender);
      empathy_message_set_timestamp (message, t);
      empathy_message_set_tptype (message, msg_type);
      empathy_message_set_is_backlog (message, TRUE);

      if (cm_id_str)
        empathy_message_set_id (message, cm_id);

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

  basedir = dir ? dir : priv->basedir;

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
              log_store_empathy_get_all_files (self, filename));
        }

      g_free (filename);
    }

  g_dir_close (gdir);

  return files;
}

static GList *
log_store_empathy_search_new (TplLogStore *self,
                              const gchar *text)
{
  GList *files, *l;
  GList *hits = NULL;
  gchar *text_casefold;

  g_return_val_if_fail (EMPATHY_IS_LOG_STORE (self), NULL);
  g_return_val_if_fail (!EMP_STR_EMPTY (text), NULL);

  text_casefold = g_utf8_casefold (text, -1);

  files = log_store_empathy_get_all_files (self, NULL);
  DEBUG ("Found %d log files in total", g_list_length (files));

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
          EmpathyLogSearchHit *hit;

          hit = log_store_empathy_search_hit_new (self, filename);

          if (hit)
            {
              hits = g_list_prepend (hits, hit);
              DEBUG ("Found text:'%s' in file:'%s' on date:'%s'",
                  text, hit->filename, hit->date);
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
      EmpathyLogSearchHit *hit;

      if (!is_chatroom && strcmp (name, LOG_DIR_CHATROOMS) == 0)
        {
          gchar *filename = g_build_filename (dir, name, NULL);
          hits = g_list_concat (hits, log_store_empathy_get_chats_for_dir (
                self, filename, TRUE));
          g_free (filename);
          continue;
        }
      hit = g_slice_new0 (EmpathyLogSearchHit);
      hit->chat_id = g_strdup (name);
      hit->is_chatroom = is_chatroom;

      hits = g_list_prepend (hits, hit);
    }

  g_dir_close (gdir);

  return hits;
}


static GList *
log_store_empathy_get_messages_for_date (TplLogStore *self,
                                         TpAccount *account,
                                         const gchar *chat_id,
                                         gboolean chatroom,
                                         const gchar *date)
{
  gchar *filename;
  GList *messages;

  g_return_val_if_fail (EMPATHY_IS_LOG_STORE (self), NULL);
  g_return_val_if_fail (chat_id != NULL, NULL);
  g_return_val_if_fail (account != NULL, NULL);

  filename = log_store_empathy_get_filename_for_date (self, account,
      chat_id, chatroom, date);
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

  return hits;
}

static const gchar *
log_store_empathy_get_name (TplLogStore *self)
{
  TplLogStoreEmpathyPriv *priv = GET_PRIV (self);

  return priv->name;
}

static GList *
log_store_empathy_get_filtered_messages (TplLogStore *self,
                                         TpAccount *account,
                                         const gchar *chat_id,
                                         gboolean chatroom,
                                         guint num_messages,
                                         EmpathyLogMessageFilter filter,
                                         gpointer user_data)
{
  GList *dates, *l, *messages = NULL;
  guint i = 0;

  dates = log_store_empathy_get_dates (self, account, chat_id, chatroom);

  for (l = g_list_last (dates); l && i < num_messages; l = g_list_previous (l))
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
            {
              i++;
            }
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

  iface->get_name = log_store_empathy_get_name;
  iface->exists = log_store_empathy_exists;
  iface->add_message = log_store_empathy_add_message;
  iface->get_dates = log_store_empathy_get_dates;
  iface->get_messages_for_date = log_store_empathy_get_messages_for_date;
  iface->get_chats = log_store_empathy_get_chats;
  iface->search_new = log_store_empathy_search_new;
  iface->ack_message = NULL;
  iface->get_filtered_messages = log_store_empathy_get_filtered_messages;
}
