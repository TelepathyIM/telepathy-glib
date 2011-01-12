/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2011 Collabora Ltd.
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
#include <telepathy-logger/event.h>
#include <telepathy-logger/event-internal.h>
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
 * TplLogEventFilter:
 * @event: the #TplEvent to filter
 * @user_data: user-supplied data
 *
 * Returns: %TRUE if @event should appear in the result
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
  gchar *id;
  TplEventSearchType type;
  GDate *date;
  guint num_events;
  TplLogEventFilter filter;
  gchar *search_text;
  gpointer user_data;
  TplEvent *logevent;
} TplLogManagerEventInfo;


typedef struct
{
  TplLogManager *manager;
  TplLogManagerEventInfo *request;
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
log_manager_constructor (GType type,
    guint n_props,
    GObjectConstructParam *props)
{
  GObject *retval = NULL;

  if (G_LIKELY (manager_singleton))
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

  /* Load the event counting cache */
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
 * _tpl_log_manager_add_event
 * @manager: the log manager
 * @event: a TplEvent subclass's instance
 * @error: the memory location of GError, filled if an error occurs
 *
 * It stores @event, sending it to all the registered TplLogStore which have
 * #TplLogStore:writable set to %TRUE.
 * Every TplLogManager is guaranteed to have at least a readable
 * and a writable TplLogStore regitered.
 *
 * It applies for any registered TplLogStore with #TplLogstore:writable property
 * %TRUE
 *
 * Returns: %TRUE if the event has been successfully added, otherwise %FALSE.
 */
gboolean
_tpl_log_manager_add_event (TplLogManager *manager,
    TplEvent *event,
    GError **error)
{
  TplLogManagerPriv *priv;
  GList *l;
  gboolean retval = FALSE;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), FALSE);
  g_return_val_if_fail (TPL_IS_EVENT (event), FALSE);

  priv = manager->priv;

  if (!_tpl_conf_is_globally_enabled (priv->conf))
    {
      /* ignore event, logging is globally disabled */
      return FALSE;
    }

  /* send the event to any writable log store */
  for (l = priv->writable_stores; l != NULL; l = g_list_next (l))
    {
      GError *loc_error = NULL;
      TplLogStore *store = l->data;
      gboolean result;

      result = _tpl_log_store_add_event (store, event, &loc_error);
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
          "writable LogStores log-id %s.", _tpl_event_get_log_id (event));
      g_set_error_literal (error, TPL_LOG_MANAGER_ERROR,
          TPL_LOG_MANAGER_ERROR_ADD_EVENT,
          "Non recoverable error occurred during log manager's "
          "add_event() execution");
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
 * @logstore has to properly implement the add_event method if the
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
   * general list and never written separately */
  priv->stores = g_list_prepend (priv->stores, g_object_ref (logstore));
  DEBUG ("LogStore name=%s registered", _tpl_log_store_get_name (logstore));

  return TRUE;
}


/**
 * tpl_log_manager_exists:
 * @manager: TplLogManager
 * @account: TpAccount
 * @id: a non-NULL event id
 * @type: the event type
 *
 * Checks if @id of type @type exists for @account.
 *
 * It applies for any registered TplLogStore with the #TplLogStore:readable
 * property %TRUE.

 * Returns: %TRUE if @id exists, otherwise %FALSE
 */
gboolean
tpl_log_manager_exists (TplLogManager *manager,
    TpAccount *account,
    const gchar *id,
    TplEventSearchType type)
{
  GList *l;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), FALSE);
  g_return_val_if_fail (id != NULL, FALSE);

  priv = manager->priv;

  for (l = priv->readable_stores; l != NULL; l = g_list_next (l))
    {
      if (_tpl_log_store_exists (TPL_LOG_STORE (l->data),
            account, id, type))
        return TRUE;
    }

  return FALSE;
}


/**
 * _tpl_log_manager_get_dates:
 * @manager: a #TplLogManager
 * @account: a #TpAccount
 * @id: a non-NULL event identifier
 * @type: the event type
 *
 * Retrieves a list of #GDate corresponding to each day
 * at least an event of type @type was sent to or received from @id.
 * @id may be the id of a buddy or a chatroom,
 *
 * It applies for any registered TplLogStore with the #TplLogStore:readable
 * property %TRUE.
 *
 * Returns: a GList of (GDate *), to be freed using something like
 * g_list_free_full (lst, g_date_free);
 */
GList *
_tpl_log_manager_get_dates (TplLogManager *manager,
    TpAccount *account,
    const gchar *id,
    TplEventSearchType type)
{
  GList *l, *out = NULL;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  priv = manager->priv;

  for (l = priv->readable_stores; l != NULL; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);
      GList *new;

      /* Insert dates of each store in the out list. Keep the out list sorted
       * and avoid to insert dups. */
      new = _tpl_log_store_get_dates (store, account, id, type);
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
_tpl_log_manager_get_events_for_date (TplLogManager *manager,
    TpAccount *account,
    const gchar *id,
    TplEventSearchType type,
    const GDate *date)
{
  GList *l, *out = NULL;
  TplLogManagerPriv *priv;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  priv = manager->priv;

  for (l = priv->readable_stores; l != NULL; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);

      out = g_list_concat (out, _tpl_log_store_get_events_for_date (store,
          account, id, type, date));
    }

  return out;
}


static gint
log_manager_event_date_cmp (gconstpointer a,
    gconstpointer b)
{
  TplEvent *one = (TplEvent *) a;
  TplEvent *two = (TplEvent *) b;
  gint64 one_time, two_time;

  g_assert (TPL_IS_EVENT (one));
  g_assert (TPL_IS_EVENT (two));

  one_time = tpl_event_get_timestamp (one);
  two_time = tpl_event_get_timestamp (two);

  /* return -1, 0 or 1 depending on event1 being newer, the same or older
   * than event2 */
  return CLAMP (one_time - two_time, -1, 1);
}


GList *
_tpl_log_manager_get_filtered_events (TplLogManager *manager,
    TpAccount *account,
    const gchar *id,
    TplEventSearchType type,
    guint num_events,
    TplLogEventFilter filter,
    gpointer user_data)
{
  TplLogManagerPriv *priv;
  GList *out = NULL;
  GList *l;
  guint i = 0;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (id), NULL);

  priv = manager->priv;

  /* Get num_events from each log store and keep only the
   * newest ones in the out list. Keep that list sorted: olders first. */
  for (l = priv->readable_stores; l != NULL; l = g_list_next (l))
    {
      TplLogStore *store = TPL_LOG_STORE (l->data);
      GList *new;

      new = _tpl_log_store_get_filtered_events (store, account, id,
          type, num_events, filter, user_data);
      while (new != NULL)
        {
          if (i < num_events)
            {
              /* We have less events than needed so far. Keep this event. */
              out = g_list_insert_sorted (out, new->data,
                  (GCompareFunc) log_manager_event_date_cmp);
              i++;
            }
          else if (log_manager_event_date_cmp (new->data, out->data) > 0)
            {
              /* This event is newer than the oldest event we have in out
               * list. Remove the head of out list and insert this event. */
              g_object_unref (out->data);
              out = g_list_delete_link (out, out);
              out = g_list_insert_sorted (out, new->data,
                  (GCompareFunc) log_manager_event_date_cmp);
            }
          else
            {
              /* This event is older than the oldest event we have in out
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
 * @a: a #TplLogSearchHit
 * @b: a #TplLogSearchHit
 *
 * Compares @a and @b.
 *
 * Returns: 0 if a == b, nonzero otherwise.
 */
gint
_tpl_log_manager_search_hit_compare (TplLogSearchHit *a,
    TplLogSearchHit *b)
{
  g_return_val_if_fail (a != NULL && a->id != NULL, 1);
  g_return_val_if_fail (b != NULL && b->id != NULL, 1);

  if (g_strcmp0 (a->id, b->id) == 0)
    {
      if (a->type == b->type)
        return 0;
    }

  return 1;
}


/**
 * _tpl_log_manager_get_chats
 * @manager: the log manager
 * @account: a TpAccount the query will return data related to
 *
 * It queries the readable TplLogStores in @manager for all the buddies the
 * log store has at least a conversation stored originated using @account.
 *
 * Returns: a list of pointer to TplLogSearchHit, having id and
 * type fields filled. the result needs to be freed after use using
 * _tpl_log_manager_search_hit_free
 */
GList *
_tpl_log_manager_get_events (TplLogManager *manager,
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
      GList *in, *j;

      in = _tpl_log_store_get_events (store, account);
      /* merge the lists avoiding duplicates */
      for (j = in; j != NULL; j = g_list_next (j))
        {
          TplLogSearchHit *hit = j->data;

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
_tpl_log_manager_search_in_identifier (TplLogManager *manager,
    TpAccount *account,
    gchar const *identifier,
    TplEventSearchType type,
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
          _tpl_log_store_search_in_identifier
          (store, account, identifier, type, text));
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


TplLogSearchHit *
_tpl_log_manager_search_hit_new (TpAccount *account,
    const gchar *id,
    TplEventSearchType type,
    GDate *date)
{
  TplLogSearchHit *hit = g_slice_new0 (TplLogSearchHit);

  g_assert (id != NULL);

  if (account != NULL)
    hit->account = g_object_ref (account);

  hit->id = g_strdup (id);
  hit->type = type;

  if (date != NULL)
    hit->date = g_date_new_dmy (g_date_get_day (date), g_date_get_month (date),
        g_date_get_year (date));

  return hit;
}

void
_tpl_log_manager_search_hit_free (TplLogSearchHit *hit)
{
  if (hit->account != NULL)
    g_object_unref (hit->account);

  if (hit->date != NULL)
    g_date_free (hit->date);

  g_free (hit->id);

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


static TplLogManagerEventInfo *
tpl_log_manager_event_info_new (void)
{
  return g_slice_new0 (TplLogManagerEventInfo);
}


static void
tpl_log_manager_event_info_free (TplLogManagerEventInfo *data)
{
  tp_clear_object (&data->account);
  tp_clear_object (&data->logevent);

  tp_clear_pointer (&data->id, g_free);
  tp_clear_pointer (&data->date, g_date_free);
  tp_clear_pointer (&data->search_text, g_free);
  g_slice_free (TplLogManagerEventInfo, data);
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


/* There is no g_date_copy() */
static GDate *
copy_date (const GDate *date)
{
  return g_date_new_julian (g_date_get_julian (date));
}


static void
_get_dates_async_result_free (gpointer data)
{
  GList *lst = data; /* list of (GDate *) */

  g_list_foreach (lst, (GFunc) g_date_free, NULL);
  g_list_free (lst);
}


static void
_get_dates_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerEventInfo *event_info;
  GList *lst = NULL;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  event_info = async_data->request;

  lst = _tpl_log_manager_get_dates (async_data->manager,
      event_info->account, event_info->id,
      event_info->type);

  g_simple_async_result_set_op_res_gpointer (simple, lst,
      _get_dates_async_result_free);
}


/**
 * tpl_log_manager_get_dates_async:
 * @manager: a #TplLogManager
 * @account: a #TpAccount
 * @id: the event identifier (can't be %NULL)
 * @type: the event type
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Retrieves a list of #GDate corresponding to each day
 * at least an event of type @type was sent to or received from @id.
 *
 * It applies for any registered TplLogStore with the #TplLogStore:readable
 * property %TRUE.
 */
void
tpl_log_manager_get_dates_async (TplLogManager *manager,
    TpAccount *account,
    const gchar *id,
    TplEventSearchType type,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerEventInfo *event_info = tpl_log_manager_event_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (manager));
  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (!TPL_STR_EMPTY (id));

  event_info->account = g_object_ref (account);
  event_info->id = g_strdup (id);
  event_info->type = type;

  async_data->manager = g_object_ref (manager);
  async_data->request = event_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_event_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (manager),
      _tpl_log_manager_async_operation_cb, async_data,
      tpl_log_manager_get_dates_async);

  g_simple_async_result_run_in_thread (simple, _get_dates_async_thread, 0,
      NULL);

  g_object_unref (simple);
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
    {
      GList *list, *l;

      *dates = NULL;
      list = g_simple_async_result_get_op_res_gpointer (simple);

      for (l = list; l != NULL; l = g_list_next (l))
        *dates = g_list_prepend (*dates, copy_date (l->data));

      *dates = g_list_reverse (*dates);
    }

  return TRUE;
}


static void
_get_events_for_date_async_result_free (gpointer data)
{
  GList *lst = data; /* list of TPL_EVENT */

  g_list_foreach (lst, (GFunc) g_object_unref, NULL);
  g_list_free (lst);
}


static void
_get_events_for_date_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerEventInfo *event_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  event_info = async_data->request;

  lst = _tpl_log_manager_get_events_for_date (async_data->manager,
      event_info->account,
      event_info->id,
      event_info->type,
      event_info->date);

  g_simple_async_result_set_op_res_gpointer (simple, lst,
      _get_events_for_date_async_result_free);
}


/**
 * tpl_log_manager_get_events_for_date_async
 * @manager: a #TplLogManager
 * @account: a #TpAccount
 * @id: the event identifier (can't be %NULL)
 * @type: the event type
 * @date: a #GDate
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Retrieve a list of #TplEvent exchanged at @date with @id.
 */
void
tpl_log_manager_get_events_for_date_async (TplLogManager *manager,
    TpAccount *account,
    const gchar *id,
    TplEventSearchType type,
    const GDate *date,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerEventInfo *event_info = tpl_log_manager_event_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (manager));
  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (!TPL_STR_EMPTY (id));
  g_return_if_fail (date != NULL);

  event_info->account = g_object_ref (account);
  event_info->id = g_strdup (id);
  event_info->type = type;
  event_info->date = copy_date (date);

  async_data->manager = g_object_ref (manager);
  async_data->request = event_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_event_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (manager),
      _tpl_log_manager_async_operation_cb, async_data,
      tpl_log_manager_get_events_for_date_async);

  g_simple_async_result_run_in_thread (simple,
      _get_events_for_date_async_thread, 0, NULL);

  g_object_unref (simple);
}


/**
 * tpl_log_manager_get_events_for_date_finish
 * @self: a #TplLogManager
 * @result: a #GAsyncResult
 * @events: a pointer to a #GList used to return the list of #GDate
 * @error: a #GError to fill
 *
 * Returns: #TRUE if the operation was successful, otherwise #FALSE
 */
gboolean
tpl_log_manager_get_events_for_date_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **events,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), tpl_log_manager_get_events_for_date_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  if (events != NULL)
    {
      *events = g_list_copy (g_simple_async_result_get_op_res_gpointer (
            simple));

      g_list_foreach (*events, (GFunc) g_object_ref, NULL);
    }

  return TRUE;
}


static void
_get_filtered_events_async_result_free (gpointer data)
{
  GList *lst = data; /* list of TPL_EVENT */

  g_list_foreach (lst, (GFunc) g_object_unref, NULL);
  g_list_free (lst);
}


static void
_get_filtered_events_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerEventInfo *event_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  event_info = async_data->request;

  lst = _tpl_log_manager_get_filtered_events (async_data->manager,
      event_info->account, event_info->id, event_info->type,
      event_info->num_events, event_info->filter, event_info->user_data);

  g_simple_async_result_set_op_res_gpointer (simple, lst,
      _get_filtered_events_async_result_free);
}


/**
 * tpl_log_manager_get_filtered_events_async:
 * @manager: a #TplLogManager
 * @account: a #TpAccount
 * @id: the event identifier (can't be %NULL)
 * @type: the event type
 * @num_event: number of maximum events to fetch
 * @filter: an optional filter function
 * @filter_user_data: user data to pass to @filter
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Retrieve the most recent @num_event events exchanged with @id.
 */
void
tpl_log_manager_get_filtered_events_async (TplLogManager *manager,
    TpAccount *account,
    const gchar *id,
    TplEventSearchType type,
    guint num_events,
    TplLogEventFilter filter,
    gpointer filter_user_data,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerEventInfo *event_info = tpl_log_manager_event_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (manager));
  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (!TPL_STR_EMPTY (id));
  g_return_if_fail (num_events > 0);

  event_info->account = g_object_ref (account);
  event_info->id = g_strdup (id);
  event_info->type = type;
  event_info->num_events = num_events;
  event_info->filter = filter;
  event_info->user_data = filter_user_data;

  async_data->manager = g_object_ref (manager);
  async_data->request = event_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_event_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (manager),
      _tpl_log_manager_async_operation_cb, async_data,
      tpl_log_manager_get_filtered_events_async);

  g_simple_async_result_run_in_thread (simple,
      _get_filtered_events_async_thread, 0, NULL);

  g_object_unref (simple);
}


/**
 * tpl_log_manager_get_filtered_events_finish:
 * @self: a #TplLogManager
 * @result: a #GAsyncResult
 * @events: a pointer to a #GList used to return the list #TplEvent
 * @error: a #GError to fill
 *
 * Returns: #TRUE if the operation was successful, otherwise #FALSE.
 */
gboolean
tpl_log_manager_get_filtered_events_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **events,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), tpl_log_manager_get_filtered_events_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  if (events != NULL)
    {
      *events = g_list_copy (g_simple_async_result_get_op_res_gpointer (
            simple));

      g_list_foreach (*events, (GFunc) g_object_ref, NULL);
    }

  return TRUE;
}


static GList *
copy_search_hit_list (GList *list)
{
  GList *result = NULL;
  GList *l;

  for (l = list; l != NULL; l = g_list_next (l))
    {
      result = g_list_prepend (result,
          _tpl_log_manager_search_hit_copy (l->data));
    }

  return g_list_reverse (result);
}


static void
_get_events_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerEventInfo *event_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  event_info = async_data->request;

  lst = _tpl_log_manager_get_events (async_data->manager, event_info->account);

  g_simple_async_result_set_op_res_gpointer (simple, lst,
      (GDestroyNotify) tpl_log_manager_search_free);
}


/**
 * tpl_log_manager_get_events_async:
 * @self: a #TplLogManager
 * @account: a #TpAccount
 * @callback: a callback to call when the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Start a query looking for all the events on @account.
 */
void
tpl_log_manager_get_events_async (TplLogManager *self,
    TpAccount *account,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerEventInfo *event_info = tpl_log_manager_event_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (self));
  g_return_if_fail (TP_IS_ACCOUNT (account));

  event_info->account = g_object_ref (account);

  async_data->manager = g_object_ref (self);
  async_data->request = event_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_event_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (self),
      _tpl_log_manager_async_operation_cb, async_data,
      tpl_log_manager_get_events_async);

  g_simple_async_result_run_in_thread (simple, _get_events_async_thread, 0,
      NULL);

  g_object_unref (simple);
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
tpl_log_manager_get_events_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **events,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), tpl_log_manager_get_events_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  if (events != NULL)
    {
      GList *list;

      list = g_simple_async_result_get_op_res_gpointer (simple);
      *events = copy_search_hit_list (list);
    }

  return TRUE;
}


static void
_search_in_identifier_async_result_free (gpointer data)
{
  GList *lst = data; /* list of TplSearchHit */

  g_list_foreach (lst, (GFunc) _tpl_log_manager_search_hit_free, NULL);
  g_list_free (lst);
}


static void
_search_in_identifier_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerEventInfo *event_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  event_info = async_data->request;

  lst = _tpl_log_manager_search_in_identifier (async_data->manager,
      event_info->account, event_info->id, event_info->type,
      event_info->search_text);

  g_simple_async_result_set_op_res_gpointer (simple, lst,
      _search_in_identifier_async_result_free);
}


void
_tpl_log_manager_search_in_identifier_async (TplLogManager *manager,
    TpAccount *account,
    gchar const *identifier,
    TplEventSearchType type,
    const gchar *text,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogManagerEventInfo *event_info = tpl_log_manager_event_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (manager));
  g_return_if_fail (TP_IS_ACCOUNT (account));

  event_info->account = g_object_ref (account);
  event_info->id = g_strdup (identifier);
  event_info->type = type;
  event_info->search_text = g_strdup (text);

  async_data->manager = g_object_ref (manager);
  async_data->request = event_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_event_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (manager),
      _tpl_log_manager_async_operation_cb, async_data,
      _tpl_log_manager_search_in_identifier_async);

  g_simple_async_result_run_in_thread (simple,
      _search_in_identifier_async_thread, 0, NULL);

  g_object_unref (simple);
}


gboolean
_tpl_log_manager_search_in_identifier_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **hits,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self),
        _tpl_log_manager_search_in_identifier_async),
      FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  if (hits != NULL)
    {
      GList *list;

      list = g_simple_async_result_get_op_res_gpointer (simple);
      *hits = copy_search_hit_list (list);
    }

  return TRUE;
}


static void
_search_new_async_result_free (gpointer data)
{
  GList *lst = data; /* list of TplSearchHit */

  g_list_foreach (lst, (GFunc) _tpl_log_manager_search_hit_free, NULL);
  g_list_free (lst);
}


static void
_search_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  TplLogManagerAsyncData *async_data;
  TplLogManagerEventInfo *event_info;
  GList *lst;

  async_data = g_async_result_get_user_data (G_ASYNC_RESULT (simple));
  event_info = async_data->request;

  lst = _tpl_log_manager_search (async_data->manager,
      event_info->search_text);

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
  TplLogManagerEventInfo *event_info = tpl_log_manager_event_info_new ();
  TplLogManagerAsyncData *async_data = tpl_log_manager_async_data_new ();
  GSimpleAsyncResult *simple;

  g_return_if_fail (TPL_IS_LOG_MANAGER (manager));

  event_info->search_text = g_strdup (text);

  async_data->manager = g_object_ref (manager);
  async_data->request = event_info;
  async_data->request_free =
    (TplLogManagerFreeFunc) tpl_log_manager_event_info_free;
  async_data->cb = callback;
  async_data->user_data = user_data;

  simple = g_simple_async_result_new (G_OBJECT (manager),
      _tpl_log_manager_async_operation_cb, async_data,
      tpl_log_manager_search_async);

  g_simple_async_result_run_in_thread (simple, _search_async_thread, 0,
      NULL);

  g_object_unref (simple);
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
    GList **hits,
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

  if (hits != NULL)
    {
      GList *list;

      list = g_simple_async_result_get_op_res_gpointer (simple);
      *hits = copy_search_hit_list (list);
    }

  return TRUE;
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


TplLogSearchHit *
_tpl_log_manager_search_hit_copy (TplLogSearchHit *hit)
{
  return _tpl_log_manager_search_hit_new (hit->account, hit->id,
      hit->type, hit->date);
}