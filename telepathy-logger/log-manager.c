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
 *          Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "log-manager.h"	// RO
#include "log-manager-priv.h"	// W

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/log-entry.h>
#include <telepathy-logger/log-store.h>
#include <telepathy-logger/log-store-empathy.h>
#include <telepathy-logger/datetime.h>
#include <telepathy-logger/util.h>

//#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
//#include <empathy-debug.h>

#define DEBUG g_debug

#define GET_PRIV(obj) TPL_GET_PRIV (obj, TplLogManager)
typedef struct
{
  GList *stores;
} TplLogManagerPriv;


typedef void (*TplLogManagerFreeFunc) (gpointer *data);

typedef struct
{
  TplLogManager *manager;
  gpointer request;
  TplLogManagerFreeFunc request_free;
  GAsyncReadyCallback cb;
  gpointer user_data;
} TplLogManagerAsyncData;


typedef struct
{
  TpAccount *account;
  gchar *chat_id;
  gboolean is_chatroom;
  gchar *date;
  guint num_messages;
  TplLogMessageFilter filter;
  gchar *search_text;
  gpointer user_data;
  gpointer logentry;
} TplLogManagerChatInfo;


G_DEFINE_TYPE (TplLogManager, tpl_log_manager, G_TYPE_OBJECT);

static TplLogManager *manager_singleton = NULL;

static void
log_manager_finalize (GObject *object)
{
  TplLogManagerPriv *priv;

  priv = GET_PRIV (object);

  g_list_foreach (priv->stores, (GFunc) g_object_unref, NULL);
  g_list_free (priv->stores);
}


/* 
 * - Singleton LogManager constructor -
 * Initialises LogStores with LogStoreEmpathy instance
 */
static GObject *
log_manager_constructor (GType type,
    guint n_props, GObjectConstructParam *props)
{
  GObject *retval;
  TplLogManagerPriv *priv;

  if (manager_singleton)
    {
      retval = g_object_ref (manager_singleton);
    }
  else
    {
      retval = G_OBJECT_CLASS (tpl_log_manager_parent_class)->constructor
          (type, n_props, props);

      manager_singleton = TPL_LOG_MANAGER (retval);
      g_object_add_weak_pointer (retval, (gpointer *) & manager_singleton);

      priv = GET_PRIV (manager_singleton);

      priv->stores = g_list_append (priv->stores,
          g_object_new (TPL_TYPE_LOG_STORE_EMPATHY,
          NULL));
    }

  return retval;
}


static void
tpl_log_manager_class_init (TplLogManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = log_manager_constructor;
  object_class->finalize = log_manager_finalize;

  g_type_class_add_private (object_class, sizeof (TplLogManagerPriv));
}


static void
tpl_log_manager_init (TplLogManager *manager)
{
  TplLogManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
      TPL_TYPE_LOG_MANAGER,
      TplLogManagerPriv);
  manager->priv = priv;

  /* initialise thread support. It can be called just once, so check it already
   * ON and call if if it's not.
   * Threads are needed by Async APIs.
   */
  if (!g_thread_supported ())
    {
      g_debug ("Initializing GThread");
      g_thread_init (NULL);
    }
  else
    g_debug ("GThread already initialized. Brilliant!");
}


TplLogManager *
tpl_log_manager_dup_singleton (void)
{
  return g_object_new (TPL_TYPE_LOG_MANAGER, NULL);
}

/*
 * @message: a TplLogEntry subclass
 */
gboolean
tpl_log_manager_add_message (TplLogManager *manager,
    TplLogEntry *message,
    GError **error)
{
  TplLogManagerPriv *priv;
  GList *l;
  gboolean out = FALSE;
  gboolean found = FALSE;

  /* TODO: When multiple log stores appear with add_message implementations
   * make this customisable. */
  const gchar *add_store = "TpLogger";

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), FALSE);
  g_return_val_if_fail (TPL_IS_LOG_ENTRY (message), FALSE);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      if (!tp_strdiff
          (tpl_log_store_get_name (TPL_LOG_STORE (l->data)), add_store))
        {
          out = tpl_log_store_add_message (TPL_LOG_STORE (l->data),
              message, error);
          found = TRUE;
          break;
        }
    }

  if (!found)
    DEBUG ("Failed to find chosen log store to write to.");

  return out;
}


gboolean
tpl_log_manager_exists (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom)
{
  GList *l;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), FALSE);
  g_return_val_if_fail (chat_id != NULL, FALSE);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      if (tpl_log_store_exists (TPL_LOG_STORE (l->data),
            account, chat_id, chatroom))
        return TRUE;
    }

  return FALSE;
}

/*
 * @returns a list of gchar dates
 */
GList *
tpl_log_manager_get_dates (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id, gboolean chatroom)
{
  GList *l, *out = NULL;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (chat_id != NULL, NULL);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);
      GList *new;

      /* Insert dates of each store in the out list. Keep the out list sorted
       * and avoid to insert dups. */
      new = tpl_log_store_get_dates (store, account, chat_id, chatroom);
      while (new)
        {
          if (g_list_find_custom (out, new->data, (GCompareFunc) strcmp))
            g_free (new->data);
          else
            out =
              g_list_insert_sorted (out, new->data, (GCompareFunc) strcmp);

          new = g_list_delete_link (new, new);
        }
    }

  return out;
}

GList *
tpl_log_manager_get_messages_for_date (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom,
    const gchar *date)
{
  GList *l, *out = NULL;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (chat_id != NULL, NULL);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);

      out =
        g_list_concat (out,
            tpl_log_store_get_messages_for_date (store, account,
              chat_id, chatroom,
              date));
    }

  return out;
}


static gint
log_manager_message_date_cmp (gconstpointer a,
    gconstpointer b)
{
  TplLogEntry *one = (TplLogEntry *) a;
  TplLogEntry *two = (TplLogEntry *) b;
  time_t one_time, two_time;

  /* TODO better to use a real method call, instead or dereferencing it's
   * pointer */
  one_time = TPL_LOG_ENTRY_GET_CLASS (one)->get_timestamp (one);
  two_time = TPL_LOG_ENTRY_GET_CLASS (two)->get_timestamp (two);

  /* Return -1 of message1 is older than message2 */
  return one_time < two_time ? -1 : one_time - two_time;
}


GList *
tpl_log_manager_get_filtered_messages (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom,
    guint num_messages,
    TplLogMessageFilter filter,
    gpointer user_data)
{
  TplLogManagerPriv *priv;
  GList *out = NULL;
  GList *l;
  guint i = 0;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (chat_id), NULL);

  priv = GET_PRIV (manager);

  /* Get num_messages from each log store and keep only the
   * newest ones in the out list. Keep that list sorted: Older first. */
  for (l = priv->stores; l; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);
      GList *new;

      new = tpl_log_store_get_filtered_messages (store, account, chat_id,
          chatroom, num_messages,
          filter, user_data);
      while (new)
        {
          if (i < num_messages)
            {
              /* We have less message than needed so far. Keep this message */
              out = g_list_insert_sorted (out, new->data,
                  (GCompareFunc)
                  log_manager_message_date_cmp);
              i++;
            }
          else if (log_manager_message_date_cmp (new->data, out->data) > 0)
            {
              /* This message is newer than the oldest message we have in out
               * list. Remove the head of out list and insert this message */
              g_object_unref (out->data);
              out = g_list_delete_link (out, out);
              out = g_list_insert_sorted (out, new->data,
                  (GCompareFunc)
                  log_manager_message_date_cmp);
            }
          else
            {
              /* This message is older than the oldest message we have in out
               * list. Drop it. */
              g_object_unref (new->data);
            }

          new = g_list_delete_link (new, new);
        }
    }

  return out;
}


GList *
tpl_log_manager_get_chats (TplLogManager *manager,
    TpAccount *account)
{
  GList *l, *out = NULL;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);

      out = g_list_concat (out, tpl_log_store_get_chats (store, account));
    }

  return out;
}


GList *
tpl_log_manager_search_in_identifier_chats_new (TplLogManager *manager,
    TpAccount *account,
    gchar const *identifier,
    const gchar *text)
{
  GList *l, *out = NULL;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (identifier), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (text), NULL);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);

      out = g_list_concat (out,
          tpl_log_store_search_in_identifier_chats_new
          (store, account, identifier, text));
    }

  return out;
}


GList *
tpl_log_manager_search_new (TplLogManager *manager,
    const gchar *text)
{
  GList *l, *out = NULL;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (text), NULL);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);

      out = g_list_concat (out, tpl_log_store_search_new (store, text));
    }

  return out;
}


void
tpl_log_manager_search_hit_free (TplLogSearchHit *hit)
{
  if (hit->account != NULL)
    g_object_unref (hit->account);

  g_free (hit->date);
  g_free (hit->filename);
  g_free (hit->chat_id);

  g_slice_free (TplLogSearchHit, hit);
}


void
tpl_log_manager_search_free (GList *hits)
{
  GList *l;

  for (l = hits; l; l = g_list_next (l))
    {
      tpl_log_manager_search_hit_free (l->data);
    }

  g_list_free (hits);
}


/* Format is just date, 20061201. */
gchar *
tpl_log_manager_get_date_readable (const gchar *date)
{
  time_t t;

  t = tpl_time_parse (date);

  return tpl_time_to_string_local (t, "%a %d %b %Y");
}

/* start of Async definitions */
static TplLogManagerAsyncData *
tpl_log_manager_async_data_new (void)
{
  return g_slice_new (TplLogManagerAsyncData);
}

static TplLogManagerChatInfo *
tpl_log_manager_chat_info_new (void)
{
  return g_slice_new0 (TplLogManagerChatInfo);
}

static void
tpl_log_manager_chat_info_free (TplLogManagerChatInfo *data)
{
  tpl_object_unref_if_not_null (data->account);
  if (data->chat_id)
    g_free (data->chat_id);
  if (data->date)
    g_free (data->date);
  g_free (data);
}


gpointer
tpl_log_manager_async_operation_finish (GAsyncResult *result,
    GError **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
  return g_simple_async_result_get_op_res_gpointer (simple);
}


static void
_tpl_log_manager_async_operation_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  TplLogManagerAsyncData *async_data = (TplLogManagerAsyncData *) user_data;

  if (async_data->cb)
    {
      async_data->cb (G_OBJECT (async_data->manager), result, async_data->user_data);
    }

  /* is it needed?
   * tpl_log_manager_async_data_free(async_data); */
}

/* wrapper around GIO's GSimpleAsync* */
/*
static void _result_list_with_string_free(GList *lst)
{
  g_list_foreach(lst, (GFunc) g_free, NULL);
  g_list_free(lst);
}


static void _result_list_with_gobject_free(GList *lst)
{
  g_list_foreach(lst, (GFunc) g_object_unref, NULL);
  g_list_free(lst);
}
*/


static void
_tpl_log_manager_call_async_operation (TplLogManager *manager,
    GSimpleAsyncThreadFunc
    operation_thread_func,
    TplLogManagerAsyncData *async_data,
    GAsyncReadyCallback callback)
{
  GSimpleAsyncResult *simple;

  simple = g_simple_async_result_new (G_OBJECT (manager),
      _tpl_log_manager_async_operation_cb, async_data,
      tpl_log_manager_async_operation_finish);

  g_simple_async_result_run_in_thread (simple, operation_thread_func, 0,
      NULL);
}
/* end of Async common function */

/* Start of add_message async implementation */
static void
_add_message_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerChatInfo *chat_info;
  GError *error;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  chat_info = async_data->request;

  tpl_log_manager_add_message (async_data->manager, chat_info->logentry,
      &error);

  if(error!=NULL) {
      g_error("synchronous operation error: %s", error->message);
      g_simple_async_result_set_from_error(simple, error);
      g_clear_error(&error);
      g_error_free(error);
  } else
    g_simple_async_result_set_op_res_gboolean (simple, TRUE);
}

void
tpl_log_manager_add_message_async (TplLogManager *manager,
    TplLogEntry *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerChatInfo *chat_info = tpl_log_manager_chat_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();

  tpl_call_with_err_if_fail (TPL_IS_LOG_MANAGER (manager), manager,
      TPL_LOG_MANAGER, FAILED,
      "manager argument passed is not a TplManager instance",
      callback, user_data);
  tpl_call_with_err_if_fail (TPL_IS_LOG_ENTRY (message), manager,
      TPL_LOG_MANAGER, FAILED,
      "message argument passed is not a TplLogEntry instance",
      callback, user_data);

  chat_info->logentry = message;
  g_object_ref (chat_info->logentry);

  async_data->manager = manager;
  g_object_ref (manager);
  async_data->request = (gpointer) chat_info;
  async_data->request_free =
      (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  _tpl_log_manager_call_async_operation (manager, _add_message_async_thread,
      async_data, callback);
}

/* End of get_dates async implementation */


/* Start of get_dates async implementation */
static void
_get_dates_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerChatInfo *chat_info;
  GList *lst = NULL;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  chat_info = async_data->request;

  lst = tpl_log_manager_get_dates (async_data->manager,
      chat_info->account, chat_info->chat_id,
      chat_info->is_chatroom);

  /* TODO add destructor */
  g_simple_async_result_set_op_res_gpointer (simple, lst, NULL);
}


void
tpl_log_manager_get_dates_async (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id,
    gboolean is_chatroom,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerChatInfo *chat_info = tpl_log_manager_chat_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();

  tpl_call_with_err_if_fail (TPL_IS_LOG_MANAGER (manager), manager,
      TPL_LOG_MANAGER, FAILED,
      "manager argument passed is not a TplManager instance",
      callback, user_data);
  tpl_call_with_err_if_fail (TP_IS_ACCOUNT (account), manager,
      TPL_LOG_MANAGER, FAILED,
      "account argument is not a TpAccount instance",
      callback, user_data);
  tpl_call_with_err_if_fail (!TPL_STR_EMPTY (chat_id), manager,
      TPL_LOG_MANAGER, FAILED,
      "chat_id argument passed cannot be empty string or NULL ptr",
      callback, user_data);

  chat_info->account = account;
  g_object_ref (account);
  chat_info->chat_id = g_strdup (chat_id);
  chat_info->is_chatroom = is_chatroom;

  async_data->manager = manager;
  g_object_ref (manager);
  async_data->request = (gpointer) chat_info;
  async_data->request_free =
      (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  _tpl_log_manager_call_async_operation (manager, _get_dates_async_thread,
      async_data, callback);
}

/* End of get_dates async implementation */

/* Start of get_messages_for_date async implementation */
static void
_get_messages_for_date_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerChatInfo *chat_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  chat_info = async_data->request;

  lst = tpl_log_manager_get_messages_for_date (async_data->manager,
      chat_info->account,
      chat_info->chat_id,
      chat_info->is_chatroom,
      chat_info->date);

  g_simple_async_result_set_op_res_gpointer (simple, lst, NULL);
}


void
tpl_log_manager_get_messages_for_date_async (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id,
    gboolean is_chatroom,
    const gchar *date,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerChatInfo *chat_info = tpl_log_manager_chat_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();

  tpl_call_with_err_if_fail (TPL_IS_LOG_MANAGER (manager), manager,
      TPL_LOG_MANAGER, FAILED,
      "manager argument passed is not a TplManager instance",
      callback, user_data);
  tpl_call_with_err_if_fail (TP_IS_ACCOUNT (account), manager,
      TPL_LOG_MANAGER, FAILED,
      "account argument is not a TpAccount instance",
      callback, user_data);
  tpl_call_with_err_if_fail (!TPL_STR_EMPTY (chat_id), manager,
      TPL_LOG_MANAGER, FAILED,
      "chat_id argument passed cannot be empty string or NULL ptr",
      callback, user_data);
  tpl_call_with_err_if_fail (!TPL_STR_EMPTY (date), manager,
      TPL_LOG_MANAGER, FAILED,
      "date argument passed cannot be empty string or NULL ptr",
      callback, user_data);

  chat_info->account = account;
  g_object_ref (account);
  chat_info->chat_id = g_strdup (chat_id);
  chat_info->is_chatroom = is_chatroom;
  chat_info->date = g_strdup(date);

  async_data->manager = manager;
  g_object_ref (manager);
  async_data->request = (gpointer) chat_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  _tpl_log_manager_call_async_operation (manager,
      _get_messages_for_date_async_thread,
      async_data, callback);
}
/* End of get_messages_for_date async implementation */


/* Start of get_filtered_messages async implementation */
static void
_get_filtered_messages_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerChatInfo *chat_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  chat_info = async_data->request;

  lst = tpl_log_manager_get_filtered_messages (async_data->manager,
      chat_info->account,
      chat_info->chat_id,
      chat_info->is_chatroom,
      chat_info->num_messages,
      chat_info->filter,
      chat_info->user_data);

  g_simple_async_result_set_op_res_gpointer (simple, lst, NULL);
}


void
tpl_log_manager_get_filtered_messages_async (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id,
    gboolean is_chatroom,
    guint num_messages,
    TplLogMessageFilter filter,
    gpointer filter_user_data,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerChatInfo *chat_info = tpl_log_manager_chat_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();

  tpl_call_with_err_if_fail (TPL_IS_LOG_MANAGER (manager), manager,
      TPL_LOG_MANAGER, FAILED,
      "manager argument passed is not a TplManager instance",
      callback, user_data);
  tpl_call_with_err_if_fail (TP_IS_ACCOUNT (account), manager,
      TPL_LOG_MANAGER, FAILED,
      "account argument is not a TpAccount instance",
      callback, user_data);
  tpl_call_with_err_if_fail (!TPL_STR_EMPTY (chat_id), manager,
      TPL_LOG_MANAGER, FAILED,
      "chat_id argument passed cannot be empty string or NULL ptr",
      callback, user_data);
  tpl_call_with_err_if_fail ((num_messages > 0), manager,
      TPL_LOG_MANAGER, FAILED,
      "num_message argument passed needs to be greater than 0",
      callback, user_data);
  tpl_call_with_err_if_fail (filter != NULL, manager,
      TPL_LOG_MANAGER, FAILED,
      "filter function should be not NULL",
      callback, user_data);

  chat_info->account = account;
  g_object_ref (account);
  chat_info->chat_id = g_strdup (chat_id);
  chat_info->is_chatroom = is_chatroom;
  chat_info->num_messages = num_messages;
  chat_info->filter = filter;
  chat_info->user_data = filter_user_data;

  async_data->manager = manager;
  g_object_ref (manager);
  async_data->request = (gpointer) chat_info;
  async_data->request_free =
      (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  _tpl_log_manager_call_async_operation (manager,
      _get_filtered_messages_thread,
      async_data, callback);
}
/* End of get_filtered_messages async implementation */


/* Start of get_chats async implementation */
static void
_get_chats_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerChatInfo *chat_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  chat_info = async_data->request;

  lst = tpl_log_manager_get_chats (async_data->manager, chat_info->account);

  g_simple_async_result_set_op_res_gpointer (simple, lst, NULL);
}


void
tpl_log_manager_get_chats_async (TplLogManager *manager,
    TpAccount *account,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerChatInfo *chat_info = tpl_log_manager_chat_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();

  tpl_call_with_err_if_fail (TPL_IS_LOG_MANAGER (manager), manager,
      TPL_LOG_MANAGER, FAILED,
      "manager argument is not a TplManager instance",
      callback, user_data);
  tpl_call_with_err_if_fail (TP_IS_ACCOUNT (account), manager,
      TPL_LOG_MANAGER, FAILED,
      "account argument is not a TpAccount instance",
      callback, user_data);

  chat_info->account = account;
  g_object_ref (account);

  async_data->manager = manager;
  g_object_ref (manager);
  async_data->request = (gpointer) chat_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  _tpl_log_manager_call_async_operation (manager,
      _get_chats_thread,
      async_data, callback);
}

/* End of get_filtered_messages async implementation */

/* Start of tpl_log_manager_search_in_identifier_chats_new async implementation */
static void
_search_in_identifier_chats_new_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerChatInfo *chat_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  chat_info = async_data->request;

  lst = tpl_log_manager_search_in_identifier_chats_new (async_data->manager, chat_info->account,
      chat_info->chat_id, chat_info->search_text);

  // TODO add destructor
  g_simple_async_result_set_op_res_gpointer (simple, lst, NULL);
}


void
tpl_log_manager_search_in_identifier_chats_new_async (TplLogManager *manager,
    TpAccount *account,
    gchar const *identifier,
    const gchar *text,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerChatInfo *chat_info = tpl_log_manager_chat_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();

  tpl_call_with_err_if_fail (TPL_IS_LOG_MANAGER (manager), manager,
      TPL_LOG_MANAGER, FAILED,
      "manager argument is not a TplManager instance",
      callback, user_data);
  tpl_call_with_err_if_fail (TP_IS_ACCOUNT (account), manager,
      TPL_LOG_MANAGER, FAILED,
      "account argument is not a TpAccount instance",
      callback, user_data);

  chat_info->account = account;
  g_object_ref (account);
  chat_info->chat_id = g_strdup(identifier);
  chat_info->search_text = g_strdup(text);

  async_data->manager = manager;
  g_object_ref (manager);
  async_data->request = (gpointer) chat_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  _tpl_log_manager_call_async_operation (manager,
      _search_in_identifier_chats_new_thread,
      async_data, callback);
}
/* End of tpl_log_manager_search_in_identifier_chats_new async implementation */


/* Start of tpl_log_manager_search_new async implementation */
static void
_search_new_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerChatInfo *chat_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  chat_info = async_data->request;

  lst = tpl_log_manager_search_new (async_data->manager, chat_info->search_text);

  g_simple_async_result_set_op_res_gpointer (simple, lst, NULL);
}


void
tpl_log_manager_search_new_async (TplLogManager *manager,
    const gchar *text,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerChatInfo *chat_info = tpl_log_manager_chat_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();

  tpl_call_with_err_if_fail (TPL_IS_LOG_MANAGER (manager), manager,
      TPL_LOG_MANAGER, FAILED,
      "manager argument is not a TplManager instance",
      callback, user_data);

  chat_info->search_text = g_strdup(text);

  async_data->manager = manager;
  g_object_ref (manager);
  async_data->request = (gpointer) chat_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  _tpl_log_manager_call_async_operation (manager, _search_new_thread,
      async_data, callback);
}

/* End of tpl_log_manager_search_new async implementation */
