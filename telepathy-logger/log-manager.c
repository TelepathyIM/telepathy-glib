/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2010 Collabora Ltd.
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 *          Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "config.h"
#include "log-manager.h"
#include "log-manager-internal.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/conf-internal.h>
#include <telepathy-logger/entry.h>
#include <telepathy-logger/entry-internal.h>
#include <telepathy-logger/log-store-internal.h>
#include <telepathy-logger/log-store-xml-internal.h>
#include <telepathy-logger/log-store-sqlite-internal.h>

#define DEBUG_FLAG TPL_DEBUG_LOG_MANAGER
#include <telepathy-logger/datetime-internal.h>
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

/**
 * SECTION:log-manager
 * @title: TplLogManager
 * @short_description: Fetch and search through logs
 *
 * The #TplLogManager object allows user to fetch logs and make searches.
 */

/**
 * TplLogManager:
 *
 * An object used to access logs
 */

/**
 * TplLogMessageFilter:
 * @message: the #TplEntry to filter
 * @user_data: user-supplied data
 *
 * Returns: %TRUE if @message should appear in the result
 */

/**
 * TPL_LOG_MANAGER_ERROR:
 *
 * The error domain for the #TplLogManager.
 */

typedef struct
{
  TplConf *conf;

  GList *stores;
  GList *writable_stores;
  GList *readable_stores;
} TplLogManagerPriv;


typedef void (*TplLogManagerFreeFunc) (gpointer *data);


typedef struct
{
  TpAccount *account;
  gchar *chat_id;
  gboolean is_chatroom;
  GDate *date;
  guint num_messages;
  TplLogMessageFilter filter;
  gchar *search_text;
  gpointer user_data;
  TplEntry *logentry;
} TplLogManagerChatInfo;


typedef struct
{
  TplLogManager *manager;
  TplLogManagerChatInfo *request;
  TplLogManagerFreeFunc request_free;
  GAsyncReadyCallback cb;
  gpointer user_data;
} TplLogManagerAsyncData;



G_DEFINE_TYPE (TplLogManager, tpl_log_manager, G_TYPE_OBJECT);

static TplLogManager *manager_singleton = NULL;

static void
log_manager_finalize (GObject *object)
{
  TplLogManagerPriv *priv;

  priv = TPL_LOG_MANAGER (object)->priv;

  g_object_unref (priv->conf);

  g_list_foreach (priv->stores, (GFunc) g_object_unref, NULL);
  g_list_free (priv->stores);
  /* no unref needed here, the only reference kept is in priv->stores */
  g_list_free (priv->writable_stores);
  g_list_free (priv->readable_stores);

  G_OBJECT_CLASS (tpl_log_manager_parent_class)->finalize (object);
}


/*
 * - Singleton LogManager constructor -
 * Initialises LogStores with LogStoreEmpathy instance
 */
static GObject *
log_manager_constructor (GType type, guint n_props,
    GObjectConstructParam *props)
{
  GObject *retval = NULL;

  if (manager_singleton)
    retval = g_object_ref (manager_singleton);
  else
    {
      retval = G_OBJECT_CLASS (tpl_log_manager_parent_class)->constructor (
          type, n_props, props);
      if (retval == NULL)
        return NULL;

      manager_singleton = TPL_LOG_MANAGER (retval);
      g_object_add_weak_pointer (retval, (gpointer *) &manager_singleton);
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

static TplLogStore *
add_log_store (TplLogManager *self,
    GType type,
    const char *name,
    gboolean readable,
    gboolean writable)
{
  TplLogStore *store;

  g_return_val_if_fail (g_type_is_a (type, TPL_TYPE_LOG_STORE), NULL);

  store = g_object_new (type,
      "name", name,
      "readable", readable,
      "writable", writable,
      NULL);

  if (store == NULL)
    CRITICAL ("Error creating %s (name=%s)", g_type_name (type), name);
  else if (!_tpl_log_manager_register_log_store (self, store))
    CRITICAL ("Failed to register store name=%s", name);

  if (store != NULL)
    /* drop the initial ref */
    g_object_unref (store);

  return store;
}

static void
_globally_enabled_changed (TplConf *conf,
    GParamSpec *pspec,
    gpointer user_data)
{
  DEBUG ("Logging has been globally %s",
      _tpl_conf_is_globally_enabled (conf) ? "enabled" : "disabled");
}

static void
tpl_log_manager_init (TplLogManager *self)
{
  TplLogStore *store;
  TplLogManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_LOG_MANAGER, TplLogManagerPriv);

  self->priv = priv;

  DEBUG ("Initialising the Log Manager");

  priv->conf = _tpl_conf_dup ();

  g_signal_connect (priv->conf, "notify::globally-enabled",
      G_CALLBACK (_globally_enabled_changed), NULL);

  /* The TPL's default read-write logstore */
  add_log_store (self, TPL_TYPE_LOG_STORE_XML, "TpLogger", TRUE, TRUE);

  /* Load by default the Empathy's legacy 'past coversations' LogStore */
  store = add_log_store (self, TPL_TYPE_LOG_STORE_XML, "Empathy", TRUE, FALSE);
  if (store != NULL)
    g_object_set (store, "empathy-legacy", TRUE, NULL);

  /* Load the message counting cache */
  add_log_store (self, TPL_TYPE_LOG_STORE_SQLITE, "Sqlite", FALSE, TRUE);

  DEBUG ("Log Manager initialised");
}


/**
 * tpl_log_manager_dup_singleton
 *
 * Returns: a new reference on the log manager
 */
TplLogManager *
tpl_log_manager_dup_singleton (void)
{
  return g_object_new (TPL_TYPE_LOG_MANAGER, NULL);
}


/**
 * _tpl_log_manager_add_message
 * @manager: the log manager
 * @message: a TplEntry subclass's instance
 * @error: the memory location of GError, filled if an error occurs
 *
 * It stores @message, sending it to all the registered TplLogStore which have
 * #TplLogStore:writable set to %TRUE.
 * Every TplLogManager is guaranteed to have at least TplLogStore a readable
 * and a writable LogStore regitered.
 *
 * It applies for any registered TplLogStore with #TplLogstore:writable property
 * %TRUE
 *
 * Returns: %TRUE if the message has been successfully added, %FALSE otherwise.
 */
gboolean
_tpl_log_manager_add_message (TplLogManager *manager,
    TplEntry *message,
    GError **error)
{
  TplLogManagerPriv *priv;
  GList *l;
  gboolean retval = FALSE;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), FALSE);
  g_return_val_if_fail (TPL_IS_ENTRY (message), FALSE);

  priv = manager->priv;

  if (!_tpl_conf_is_globally_enabled (priv->conf))
    {
      /* ignore message, logging is globally disabled */
      return FALSE;
    }

  /* send the message to any writable log store */
  for (l = priv->writable_stores; l != NULL; l = g_list_next (l))
    {
      GError *loc_error = NULL;
      TplLogStore *store = l->data;
      gboolean result;

      result = _tpl_log_store_add_message (store, message, &loc_error);
      if (!result)
        {
          CRITICAL ("logstore name=%s: %s. "
              "Event may not be logged properly.",
              _tpl_log_store_get_name (store), loc_error->message);
          g_clear_error (&loc_error);
        }
      /* TRUE if at least one LogStore succeeds */
      retval = result || retval;
    }
  if (!retval)
    {
      CRITICAL ("Failed to write to all "
          "writable LogStores log-id %s.", _tpl_entry_get_log_id (message));
      g_set_error_literal (error, TPL_LOG_MANAGER_ERROR,
          TPL_LOG_MANAGER_ERROR_ADD_MESSAGE,
          "Not recoverable error occurred during log manager's "
          "add_message() execution");
    }
  return retval;
}


/**
 * _tpl_log_manager_register_log_store
 * @self: the log manager
 * @logstore: a TplLogStore interface implementation
 *
 * It registers @logstore into @manager, the log store has to be an
 * implementation of the TplLogStore interface.
 *
 * @logstore has to properly implement the add_message method if the
 * #TplLogStore:writable is set to %TRUE.
 *
 * @logstore has to properly implement all the search/query methods if the
 * #TplLogStore:readable is set to %TRUE.
 */
gboolean
_tpl_log_manager_register_log_store (TplLogManager *self,
    TplLogStore *logstore)
{
  TplLogManagerPriv *priv = self->priv;
  GList *l;
  gboolean found = FALSE;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (self), FALSE);
  g_return_val_if_fail (TPL_IS_LOG_STORE (logstore), FALSE);

  /* check that the logstore name is not already used */
  for (l = priv->stores; l != NULL; l = g_list_next (l))
    {
      TplLogStore *store = l->data;
      const gchar *name = _tpl_log_store_get_name (logstore);

      if (!tp_strdiff (name, _tpl_log_store_get_name (store)))
        {
          found = TRUE;
          break;
        }
    }
  if (found)
    {
      DEBUG ("name=%s: already registered", _tpl_log_store_get_name (logstore));
      return FALSE;
    }

  if (_tpl_log_store_is_readable (logstore))
    priv->readable_stores = g_list_prepend (priv->readable_stores, logstore);

  if (_tpl_log_store_is_writable (logstore))
    priv->writable_stores = g_list_prepend (priv->writable_stores, logstore);

  /* reference just once, writable/readable lists are kept in sync with the
   * general list and never written separatedly */
  priv->stores = g_list_prepend (priv->stores, g_object_ref (logstore));
  DEBUG ("LogStore name=%s registered", _tpl_log_store_get_name (logstore));

  return TRUE;
}

/**
 * tpl_log_manager_exists:
 * @manager: TplLogManager
 * @account: TpAccount
 * @chat_id: a non-NULL chat id
 * @chatroom: whether @chat_id is a chatroom or not
 *
 * Checks if @chat_id does exist for @account and
 * - is a chatroom, if @chatroom is %TRUE
 * - is not a chatroom, if @chatroom is %FALSE
 *
 * It applies for any registered TplLogStore with the #TplLogStore:readable
 * property %TRUE.

 * Returns: %TRUE if @chat_id exists, %FALSE otherwise
 */
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

  priv = manager->priv;

  for (l = priv->stores; l != NULL; l = g_list_next (l))
    {
      if (_tpl_log_store_exists (TPL_LOG_STORE (l->data),
            account, chat_id, chatroom))
        return TRUE;
    }

  return FALSE;
}


/**
 * _tpl_log_manager_get_dates:
 * @manager: a TplLogManager
 * @account: a TpAccount
 * @chat_id: a non-NULL chat identifier
 * @chatroom: whather if the request is related to a chatroom or not.
 *
 * Retrieves a list of #GDate corresponding to each day
 * at least a message was sent to or received from @chat_id.
 * @chat_id may be the id of a buddy or a chatroom, depending on the value of
 * @chatroom.
 *
 * It applies for any registered TplLogStore with the #TplLogStore:readable
 * property %TRUE.
 *
 * Returns: a GList of (GDate *), to be freed using something like
 * g_list_foreach (lst, g_date_free, NULL);
 * g_list_free (lst);
 */
GList *
_tpl_log_manager_get_dates (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom)
{
  GList *l, *out = NULL;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (chat_id != NULL, NULL);

  priv = manager->priv;

  for (l = priv->readable_stores; l != NULL; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);
      GList *new;

      /* Insert dates of each store in the out list. Keep the out list sorted
       * and avoid to insert dups. */
      new = _tpl_log_store_get_dates (store, account, chat_id, chatroom);
      while (new)
        {
          if (g_list_find_custom (out, new->data,
                (GCompareFunc) g_date_compare))
            g_date_free (new->data);
          else
            out =
              g_list_insert_sorted (out, new->data,
                  (GCompareFunc) g_date_compare);

          new = g_list_delete_link (new, new);
        }
    }

  return out;
}


GList *
_tpl_log_manager_get_messages_for_date (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id,
    gboolean chatroom,
    const GDate *date)
{
  GList *l, *out = NULL;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (chat_id != NULL, NULL);

  priv = manager->priv;

  for (l = priv->readable_stores; l != NULL; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);

      out = g_list_concat (out, _tpl_log_store_get_messages_for_date (store,
          account, chat_id, chatroom, date));
    }

  return out;
}


static gint
log_manager_message_date_cmp (gconstpointer a,
    gconstpointer b)
{
  TplEntry *one = (TplEntry *) a;
  TplEntry *two = (TplEntry *) b;
  gint64 one_time, two_time;

  g_assert (TPL_IS_ENTRY (one));
  g_assert (TPL_IS_ENTRY (two));

  one_time = tpl_entry_get_timestamp (one);
  two_time = tpl_entry_get_timestamp (two);

  /* return -1, o or 1 depending on message1 is newer, the same or older than
   * message2 */
  return CLAMP (one_time - two_time, -1, 1);
}


GList *
_tpl_log_manager_get_filtered_messages (TplLogManager *manager,
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

  priv = manager->priv;

  /* Get num_messages from each log store and keep only the
   * newest ones in the out list. Keep that list sorted: Older first. */
  for (l = priv->readable_stores; l != NULL; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);
      GList *new;

      new = _tpl_log_store_get_filtered_messages (store, account, chat_id,
          chatroom, num_messages, filter, user_data);
      while (new != NULL)
        {
          if (i < num_messages)
            {
              /* We have less message than needed so far. Keep this message */
              out = g_list_insert_sorted (out, new->data,
                  (GCompareFunc) log_manager_message_date_cmp);
              i++;
            }
          else if (log_manager_message_date_cmp (new->data, out->data) > 0)
            {
              /* This message is newer than the oldest message we have in out
               * list. Remove the head of out list and insert this message */
              g_object_unref (out->data);
              out = g_list_delete_link (out, out);
              out = g_list_insert_sorted (out, new->data,
                  (GCompareFunc) log_manager_message_date_cmp);
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


/**
 * _tpl_log_manager_search_hit_compare:
 * @a: a TplLogSerachHit
 * @b: a TplLogSerachHit
 *
 * Compare @a and @b, returning an ordered relation between the two.
 * Acts similar to the strcmp family, with the difference that since
 * TplLogSerachHit is not a plain string, but a struct, the order relation
 * will be a coposition of:
 * - the order relation between @a.chat_it and @b.chat_id
 * - the order relation between @a.chatroom and @b.chatroom, being
 *   chatroom = %FALSE > chatroom = %TRUE (meaning: a 1-1 message is greater
 *   than a chatroom one).
 *
 * Returns: -1 if a > b, 1 if a < b or 0 is a == b */
gint
_tpl_log_manager_search_hit_compare (TplLogSearchHit *a,
    TplLogSearchHit *b)
{
  /* if chat_ids differ, just return their sorting return value */
  gint ret;

  g_return_val_if_fail (a != NULL && a->chat_id != NULL, 1);
  g_return_val_if_fail (b != NULL && b->chat_id != NULL, -1);

  ret = g_strcmp0 (a->chat_id, b->chat_id);

  if (ret == 0)
    {
      /* if chat_id are the same, a further check needed: chat_id may be equal
       * but one can refer to a chatroom and the other not.
       * Definition: chatroom < not_chatroom */
      if (a->is_chatroom != b->is_chatroom)
        {
          if (a->is_chatroom)
            ret = 1; /* b > a */
          else
            ret = -1; /* a > b */
        }
      else
        ret = 0; /* a = b */
    }
  /* else original strcmp result is returned */

  return ret;
}


/**
 * _tpl_log_manager_get_chats
 * @manager: the log manager
 * @account: a TpAccount the query will return data related to
 *
 * It queries the readable TplLogStores in @manager for all the buddies the
 * log store has at least a conversation stored originated using @account.
 *
 * Returns: a list of pointer to TplLogSearchHit, having chat_id and
 * is_chatroom fields filled. the result needs to be freed after use using
 * _tpl_log_manager_search_hit_free
 */
GList *
_tpl_log_manager_get_chats (TplLogManager *manager,
    TpAccount *account)
{
  GList *l, *out = NULL;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);

  priv = manager->priv;

  for (l = priv->readable_stores; l != NULL; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);
      GList *in;

      /* merge the lists avoiding duplicates */
      for (in = _tpl_log_store_get_chats (store, account);
          in != NULL;
          in = g_list_next (in))
        {
          TplLogSearchHit *hit = in->data;

          if (g_list_find_custom (out, hit,
                (GCompareFunc) _tpl_log_manager_search_hit_compare) == NULL)
            {
              /* add data if not already present */
              out = g_list_prepend (out, hit);
            }
          else
            /* free hit if already present in out */
            _tpl_log_manager_search_hit_free (hit);
        }
      g_list_free (in);
    }

  return out;
}


GList *
_tpl_log_manager_search_in_identifier_chats_new (TplLogManager *manager,
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

  priv = manager->priv;

  for (l = priv->readable_stores; l != NULL; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);

      out = g_list_concat (out,
          _tpl_log_store_search_in_identifier_chats_new
          (store, account, identifier, text));
    }

  return out;
}


GList *
_tpl_log_manager_search (TplLogManager *manager,
    const gchar *text)
{
  GList *l, *out = NULL;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (text), NULL);

  priv = manager->priv;

  for (l = priv->readable_stores; l != NULL; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);

      out = g_list_concat (out, _tpl_log_store_search_new (store, text));
    }

  return out;
}


void
_tpl_log_manager_search_hit_free (TplLogSearchHit *hit)
{
  if (hit->account != NULL)
    g_object_unref (hit->account);

  if (hit->date != NULL)
    g_date_free (hit->date);

  g_free (hit->filename);
  g_free (hit->chat_id);

  g_slice_free (TplLogSearchHit, hit);
}

/**
 * tpl_log_manager_search_free:
 * @hits: a #GList of #TplLogSearchHit
 *
 * Free @hits and its content.
 */
void
tpl_log_manager_search_free (GList *hits)
{
  GList *l;

  for (l = hits; l != NULL; l = g_list_next (l))
    {
      _tpl_log_manager_search_hit_free (l->data);
    }

  g_list_free (hits);
}

/* start of Async definitions */
static TplLogManagerAsyncData *
tpl_log_manager_async_data_new (void)
{
  return g_slice_new0 (TplLogManagerAsyncData);
}


static void
tpl_log_manager_async_data_free (TplLogManagerAsyncData *data)
{
  if (data->manager != NULL)
    g_object_unref (data->manager);
  data->request_free ((gpointer) data->request);
  g_slice_free (TplLogManagerAsyncData, data);
}


static TplLogManagerChatInfo *
tpl_log_manager_chat_info_new (void)
{
  return g_slice_new0 (TplLogManagerChatInfo);
}


static void
tpl_log_manager_chat_info_free (TplLogManagerChatInfo *data)
{
  if (data->account != NULL)
    g_object_unref (data->account);
  if (data->chat_id != NULL)
    g_free (data->chat_id);
  if (data->date != NULL)
    g_date_free (data->date);
  g_slice_free (TplLogManagerChatInfo, data);
}


static void
_tpl_log_manager_async_operation_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  TplLogManagerAsyncData *async_data = (TplLogManagerAsyncData *) user_data;

  if (async_data->cb)
    async_data->cb (G_OBJECT (async_data->manager), result,
        async_data->user_data);

  tpl_log_manager_async_data_free (async_data);
}
/* end of Async common function */


gboolean
_tpl_log_manager_add_message_finish (TplLogManager *self,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), _tpl_log_manager_add_message_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  return TRUE;
}


static void
_add_message_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerChatInfo *chat_info;
  GError *error = NULL;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  chat_info = async_data->request;

  _tpl_log_manager_add_message (async_data->manager, chat_info->logentry,
      &error);
  if (error != NULL)
    {
      DEBUG ("synchronous operation error: %s", error->message);
      g_simple_async_result_set_from_error (simple, error);
      g_error_free (error);
    }
}

void
_tpl_log_manager_add_message_async (TplLogManager *manager,
    TplEntry *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerChatInfo *chat_info = tpl_log_manager_chat_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (manager));
  g_return_if_fail (TPL_IS_ENTRY (message));

  chat_info->logentry = g_object_ref (message);

  async_data->manager = g_object_ref (manager);
  async_data->request = chat_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (manager),
      _tpl_log_manager_async_operation_cb, async_data,
      _tpl_log_manager_add_message_async);

  g_simple_async_result_run_in_thread (simple, _add_message_async_thread, 0,
      NULL);
}

/**
 * tpl_log_manager_get_dates_finish:
 * @self: a #TplLogManager
 * @result: a #GAsyncResult
 * @dates: a pointer to a #GList used to return the list of #GDate
 * @error: a #GError to fill
 *
 * Returns: #TRUE if the operation was successful, otherwise #FALSE
 */
gboolean
tpl_log_manager_get_dates_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **dates,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), tpl_log_manager_get_dates_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  if (dates != NULL)
    *dates = g_simple_async_result_get_op_res_gpointer (simple);

  return TRUE;
}

static void
_get_dates_async_result_free (gpointer data)
{
  GList *lst = data; /* list of (char *) */
  g_return_if_fail (data != NULL);

  g_list_foreach (lst, (GFunc) g_free, NULL);
  g_list_free (lst);
}


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

  lst = _tpl_log_manager_get_dates (async_data->manager,
      chat_info->account, chat_info->chat_id,
      chat_info->is_chatroom);

  g_simple_async_result_set_op_res_gpointer (simple, lst,
      _get_dates_async_result_free);
}

/**
 * tpl_log_manager_get_dates_async:
 * @manager: a #TplLogManager
 * @account: a #TpAccount
 * @chat_id: the chat identifier (can't be %NULL)
 * @is_chatroom: whather if the request is related to a chatroom or not.
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Retrieves a list of #GDate corresponding to each day
 * at least a message was sent to or received from @chat_id.
 * @chat_id may be the id of a buddy or a chatroom, depending on the value of
 * @is_chatroom.
 *
 * It applies for any registered TplLogStore with the #TplLogStore:readable
 * property %TRUE.
 */
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
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (manager));
  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (!TPL_STR_EMPTY (chat_id));

  chat_info->account = g_object_ref (account);
  chat_info->chat_id = g_strdup (chat_id);
  chat_info->is_chatroom = is_chatroom;

  async_data->manager = g_object_ref (manager);
  async_data->request = chat_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (manager),
      _tpl_log_manager_async_operation_cb, async_data,
      tpl_log_manager_get_dates_async);

  g_simple_async_result_run_in_thread (simple, _get_dates_async_thread, 0,
      NULL);
}

/**
 * tpl_log_manager_get_messages_for_date_finish
 * @self: a #TplLogManager
 * @result: a #GAsyncResult
 * @messages: a pointer to a #GList used to return the list of #GDate
 * @error: a #GError to fill
 *
 * Returns: #TRUE if the operation was successful, otherwise #FALSE
 */
gboolean
tpl_log_manager_get_messages_for_date_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **messages,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), tpl_log_manager_get_messages_for_date_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  if (messages != NULL)
    *messages = g_simple_async_result_get_op_res_gpointer (simple);

  return TRUE;
}


static void
_get_messages_for_date_async_result_free (gpointer data)
{
  GList *lst = data; /* list of TPL_ENTRY */
  g_return_if_fail (data != NULL);

  g_list_foreach (lst, (GFunc) g_object_unref, NULL);
  g_list_free (lst);
}


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

  lst = _tpl_log_manager_get_messages_for_date (async_data->manager,
      chat_info->account,
      chat_info->chat_id,
      chat_info->is_chatroom,
      chat_info->date);

  g_simple_async_result_set_op_res_gpointer (simple, lst,
      _get_messages_for_date_async_result_free);
}

/**
 * tpl_log_manager_get_messages_for_date_async
 * @manager: a #TplLogManager
 * @account: a #TpAccount
 * @chat_id: the chat identifier (can't be %NULL)
 * @is_chatroom: %TRUE if the request is related to a chatroom
 * @date: a #GDate
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Retrieve a list of #TplEntry exchanged at @date with @chat_id.
 */
void
tpl_log_manager_get_messages_for_date_async (TplLogManager *manager,
    TpAccount *account,
    const gchar *chat_id,
    gboolean is_chatroom,
    const GDate *date,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerChatInfo *chat_info = tpl_log_manager_chat_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (manager));
  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (!TPL_STR_EMPTY (chat_id));
  g_return_if_fail (date != NULL);

  chat_info->account = g_object_ref (account);
  chat_info->chat_id = g_strdup (chat_id);
  chat_info->is_chatroom = is_chatroom;
  /* There is no g_date_copy() */
  chat_info->date = g_date_new_julian (g_date_get_julian (date));

  async_data->manager = g_object_ref (manager);
  async_data->request = chat_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (manager),
      _tpl_log_manager_async_operation_cb, async_data,
      tpl_log_manager_get_messages_for_date_async);

  g_simple_async_result_run_in_thread (simple,
      _get_messages_for_date_async_thread, 0, NULL);
}

/**
 * tpl_log_manager_get_filtered_messages_finish:
 * @self: a #TplLogManager
 * @result: a #GAsyncResult
 * @messages: a pointer to a #GList used to return the list #TplEntry
 * @error: a #GError to fill
 *
 * Returns: #TRUE if the operation was successful, otherwise #FALSE
 */
gboolean
tpl_log_manager_get_filtered_messages_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **messages,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), tpl_log_manager_get_filtered_messages_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  if (messages != NULL)
    *messages = g_simple_async_result_get_op_res_gpointer (simple);

  return TRUE;
}


static void
_get_filtered_messages_async_result_free (gpointer data)
{
  GList *lst = data; /* list of TPL_ENTRY */
  g_return_if_fail (data != NULL);

  g_list_foreach (lst, (GFunc) g_object_unref, NULL);
  g_list_free (lst);
}

static void
_get_filtered_messages_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerChatInfo *chat_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  chat_info = async_data->request;

  lst = _tpl_log_manager_get_filtered_messages (async_data->manager,
      chat_info->account, chat_info->chat_id, chat_info->is_chatroom,
      chat_info->num_messages, chat_info->filter, chat_info->user_data);

  g_simple_async_result_set_op_res_gpointer (simple, lst,
      _get_filtered_messages_async_result_free);
}

/**
 * tpl_log_manager_get_filtered_messages_async:
 * @manager: a #TplLogManager
 * @account: a #TpAccount
 * @chat_id: the chat identifier (can't be %NULL)
 * @is_chatroom: %TRUE if the request is related to a chatroom
 * @num_messages: number of maximum messages to fetch
 * @filter: an optionnal filter function
 * @filter_user_data: user data to pass to @filter
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Retrieve the @num_messages most recent messages exchanged with @chat_id.
 */
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
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (manager));
  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (!TPL_STR_EMPTY (chat_id));
  g_return_if_fail (num_messages > 0);

  chat_info->account = g_object_ref (account);
  chat_info->chat_id = g_strdup (chat_id);
  chat_info->is_chatroom = is_chatroom;
  chat_info->num_messages = num_messages;
  chat_info->filter = filter;
  chat_info->user_data = filter_user_data;

  async_data->manager = g_object_ref (manager);
  async_data->request = chat_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (manager),
      _tpl_log_manager_async_operation_cb, async_data,
      tpl_log_manager_get_filtered_messages_async);

  g_simple_async_result_run_in_thread (simple,
      _get_filtered_messages_async_thread, 0, NULL);
}

/**
 * tpl_log_manager_get_chats_finish:
 * @self: a #TplLogManager
 * @result: a #GAsyncResult
 * @chats: a pointer to a #GList used to return the list of chats
 * @error: a #GError to fill
 *
 * Returns: #TRUE if the operation was successful, otherwise #FALSE
 */
gboolean
tpl_log_manager_get_chats_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **chats,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), tpl_log_manager_get_chats_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  if (chats != NULL)
    *chats = g_simple_async_result_get_op_res_gpointer (simple);

  return TRUE;
}

static void
_get_chats_async_result_free (gpointer data)
{
  GList *lst = data; /* list of (gchar *) */
  g_return_if_fail (data != NULL);

  g_list_foreach (lst, (GFunc) g_free, NULL);
  g_list_free (lst);
}


static void
_get_chats_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerChatInfo *chat_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  chat_info = async_data->request;

  lst = _tpl_log_manager_get_chats (async_data->manager, chat_info->account);

  g_simple_async_result_set_op_res_gpointer (simple, lst,
      _get_chats_async_result_free);
}

/**
 * tpl_log_manager_get_chats_async:
 * @self: a #TplLogManager
 * @account: a #TpAccount
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Start a query looking for all the conversations on @account.
 */
void
tpl_log_manager_get_chats_async (TplLogManager *self,
    TpAccount *account,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerChatInfo *chat_info = tpl_log_manager_chat_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (self));
  g_return_if_fail (TP_IS_ACCOUNT (account));

  chat_info->account = g_object_ref (account);

  async_data->manager = g_object_ref (self);
  async_data->request = chat_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (self),
      _tpl_log_manager_async_operation_cb, async_data,
      tpl_log_manager_get_chats_async);

  g_simple_async_result_run_in_thread (simple, _get_chats_async_thread, 0,
      NULL);
}

gboolean
_tpl_log_manager_search_in_identifier_chats_new_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **chats,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self),
        _tpl_log_manager_search_in_identifier_chats_new_async),
      FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  if (chats != NULL)
    *chats = g_simple_async_result_get_op_res_gpointer (simple);

  return TRUE;
}

static void
_search_in_identifier_chats_new_async_result_free (gpointer data)
{
  GList *lst = data; /* list of TplSearchHit */
  g_return_if_fail (data != NULL);

  g_list_foreach (lst, (GFunc) _tpl_log_manager_search_hit_free, NULL);
  g_list_free (lst);
}


static void
_search_in_identifier_chats_new_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerChatInfo *chat_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  chat_info = async_data->request;

  lst = _tpl_log_manager_search_in_identifier_chats_new (async_data->manager,
      chat_info->account, chat_info->chat_id, chat_info->search_text);

  g_simple_async_result_set_op_res_gpointer (simple, lst,
      _search_in_identifier_chats_new_async_result_free);
}


void
_tpl_log_manager_search_in_identifier_chats_new_async (TplLogManager *manager,
    TpAccount *account,
    gchar const *identifier,
    const gchar *text,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerChatInfo *chat_info = tpl_log_manager_chat_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (manager));
  g_return_if_fail (TP_IS_ACCOUNT (account));

  chat_info->account = g_object_ref (account);
  chat_info->chat_id = g_strdup (identifier);
  chat_info->search_text = g_strdup (text);

  async_data->manager = g_object_ref (manager);
  async_data->request = chat_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (manager),
      _tpl_log_manager_async_operation_cb, async_data,
      _tpl_log_manager_search_in_identifier_chats_new_async);

  g_simple_async_result_run_in_thread (simple,
      _search_in_identifier_chats_new_async_thread, 0, NULL);
}

/**
 * tpl_log_manager_search_finish:
 * @self: a #TplLogManager
 * @result: a #GAsyncResult
 * @chats: a pointer to a #GList used to return the list of #TplLogSearchHit
 * @error: a #GError to fill
 *
 * Returns: #TRUE if the operation was successful, otherwise #FALSE
 */
gboolean
tpl_log_manager_search_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **chats,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), tpl_log_manager_search_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  if (chats != NULL)
    *chats = g_simple_async_result_get_op_res_gpointer (simple);

  return TRUE;
}

static void
_search_new_async_result_free (gpointer data)
{
  GList *lst = data; /* list of TplSearchHit */
  g_return_if_fail (data != NULL);

  g_list_foreach (lst, (GFunc) _tpl_log_manager_search_hit_free, NULL);
  g_list_free (lst);
}


static void
_search_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerChatInfo *chat_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  chat_info = async_data->request;

  lst = _tpl_log_manager_search (async_data->manager,
      chat_info->search_text);

  g_simple_async_result_set_op_res_gpointer (simple, lst,
      _search_new_async_result_free);
}

/**
 * tpl_log_manager_search_async:
 * @manager: a #TplLogManager
 * @text: the pattern to search
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Search for all the conversations containing @text.
 */
void
tpl_log_manager_search_async (TplLogManager *manager,
    const gchar *text,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerChatInfo *chat_info = tpl_log_manager_chat_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (manager));

  chat_info->search_text = g_strdup (text);

  async_data->manager = g_object_ref (manager);
  async_data->request = chat_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_chat_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (manager),
      _tpl_log_manager_async_operation_cb, async_data,
      tpl_log_manager_search_async);

  g_simple_async_result_run_in_thread (simple, _search_async_thread, 0,
      NULL);
}

/**
 * tpl_log_manager_errors_quark:
 *
 * Returns: the #GQuark associated with the error domain of #TplLogManager
 */
GQuark
tpl_log_manager_errors_quark (void)
{
  static gsize quark = 0;

  if (g_once_init_enter (&quark))
    {
      GQuark domain = g_quark_from_static_string (
          "tpl_log_manager_errors");

      g_once_init_leave (&quark, domain);
    }

  return (GQuark) quark;
}
