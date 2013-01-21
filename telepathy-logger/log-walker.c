/*
 * Copyright (C) 2012 Red Hat, Inc.
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
 * Author: Debarshi Ray <debarshir@freedesktop.org>
 */

#include "config.h"

#include "log-walker.h"
#include "log-walker-internal.h"

#include <telepathy-logger/event.h>
#include <telepathy-logger/log-iter-internal.h>

/**
 * SECTION:log-walker
 * @title: TplLogWalker
 * @short_description: Iterate over the logs
 *
 * The #TplLogWalker object allows the user to sequentially iterate
 * over the logs.
 *
 * <example>
 *   <title>Using a TplLogWalker to fetch text events from the logs.</title>
 *   <programlisting>
 *   #include <telepathy-glib/telepathy-glib.h>
 *   #include <telepathy-logger/telepathy-logger.h>
 *
 *   static GMainLoop * loop = NULL;
 *
 *   static void
 *   events_foreach (gpointer data, gpointer user_data)
 *   {
 *     TplEvent *event = TPL_EVENT (data);
 *     const gchar *message;
 *     gint64 timestamp;
 *
 *     timestamp = tpl_event_get_timestamp (event);
 *     message = tpl_text_event_get_message (TPL_TEXT_EVENT (event));
 *     g_message ("%" G_GINT64_FORMAT " %s", timestamp, message);
 *   }
 *
 *   static void
 *   log_walker_get_events_cb (GObject *source_object,
 *       GAsyncResult *res,
 *       gpointer user_data)
 *   {
 *     TplLogWalker *walker = TPL_LOG_WALKER (source_object);
 *     GList *events;
 *
 *     if (!tpl_log_walker_get_events_finish (walker, res, &events, NULL))
 *       {
 *         g_main_loop_quit (loop);
 *         return;
 *       }
 *
 *     g_list_foreach (events, events_foreach, NULL);
 *     g_list_free_full (events, g_object_unref);
 *     if (tpl_log_walker_is_end (walker))
 *       {
 *         g_main_loop_quit (loop);
 *         return;
 *       }
 *
 *     g_message ("");
 *     tpl_log_walker_get_events_async (walker,
 *         5,
 *         log_walker_get_events_cb,
 *         NULL);
 *   }
 *
 *   static void
 *   accounts_foreach (gpointer data, gpointer user_data)
 *   {
 *     TpAccount **account_out = (TpAccount **) user_data;
 *     TpAccount *account = TP_ACCOUNT (data);
 *     const gchar *display_name;
 *
 *     display_name = tp_account_get_display_name (account);
 *     if (0 != g_strcmp0 (display_name, "alice@bar.net"))
 *       return;
 *
 *     g_object_ref (account);
 *     *account_out = account;
 *   }
 *
 *   static void
 *   account_manager_prepare_cb (GObject * source_object,
 *       GAsyncResult * res,
 *       gpointer user_data)
 *   {
 *     TpAccountManager *account_manager = TP_ACCOUNT_MANAGER (source_object);
 *     GList *accounts;
 *     TpAccount *account = NULL;
 *     TplLogManager *log_manager;
 *     TplLogWalker *walker;
 *     TplEntity *target;
 *
 *     if (!tp_proxy_prepare_finish (source_object, res, NULL))
 *       return;
 *
 *     accounts = tp_account_manager_get_valid_accounts (account_manager);
 *     g_list_foreach (accounts, accounts_foreach, &account);
 *     g_list_free_full (accounts, g_object_unref);
 *     if (account == NULL)
 *       {
 *         g_main_loop_quit (loop);
 *         return;
 *       }
 *
 *     log_manager = tpl_log_manager_dup_singleton ();
 *
 *     target = tpl_entity_new ("bob@foo.net", TPL_ENTITY_CONTACT, NULL, NULL);
 *
 *     walker = tpl_log_manager_walk_filtered_events (log_manager,
 *         account,
 *         target,
 *         TPL_EVENT_MASK_TEXT,
 *         NULL,
 *         NULL);
 *
 *     tpl_log_walker_get_events_async (walker,
 *         5,
 *         log_walker_get_events_cb,
 *         NULL);
 *
 *     g_object_unref (walker);
 *     g_object_unref (target);
 *     g_object_unref (log_manager);
 *     g_object_unref (account);
 *   }
 *
 *   int
 *   main (int argc,
 *       char *argv[])
 *   {
 *     GQuark features[] = { TP_ACCOUNT_MANAGER_FEATURE_CORE, 0 };
 *     TpAccountManager * account_manager;
 *
 *     g_type_init ();
 *     loop = g_main_loop_new (NULL, FALSE);
 *
 *     account_manager = tp_account_manager_dup ();
 *     tp_proxy_prepare_async (account_manager,
 *         features,
 *         account_manager_prepare_cb,
 *         NULL);
 *
 *     g_main_loop_run (loop);
 *
 *     g_object_unref (account_manager);
 *     g_main_loop_unref (loop);
 *     return 0;
 *   }
 *   </programlisting>
 * </example>
 *
 * Since: 0.8.0
 */

/**
 * TplLogWalker:
 *
 * An object used to iterate over the logs
 *
 * Since: 0.8.0
 */

struct _TplLogWalkerPriv
{
  GList *caches;
  GList *history;
  GList *iters;
  GQueue *queue;
  TplLogEventFilter filter;
  gboolean is_start;
  gboolean is_end;
  gpointer filter_data;
};

enum
{
  PROP_FILTER = 1,
  PROP_FILTER_DATA
};


G_DEFINE_TYPE (TplLogWalker, tpl_log_walker, G_TYPE_OBJECT);


static const gsize CACHE_SIZE = 5;

typedef enum
{
  TPL_LOG_WALKER_OP_GET_EVENTS,
  TPL_LOG_WALKER_OP_REWIND
} TplLogWalkerOpType;

typedef struct
{
  GAsyncReadyCallback cb;
  GList *events;
  GList *fill_cache;
  GList *fill_iter;
  GList *latest_cache;
  GList *latest_event;
  GList *latest_iter;
  TplLogWalkerOpType op_type;
  gint64 latest_timestamp;
  guint num_events;
} TplLogWalkerAsyncData;

typedef struct
{
  TplLogIter *iter;
  gboolean skip;
  guint count;
} TplLogWalkerHistoryData;

static void tpl_log_walker_op_run (TplLogWalker *walker);


static TplLogWalkerAsyncData *
tpl_log_walker_async_data_new (void)
{
  return g_slice_new0 (TplLogWalkerAsyncData);
}


static void
tpl_log_walker_async_data_free (TplLogWalkerAsyncData *data)
{
  g_list_free_full (data->events, g_object_unref);
  g_slice_free (TplLogWalkerAsyncData, data);
}


static TplLogWalkerHistoryData *
tpl_log_walker_history_data_new (void)
{
  return g_slice_new0 (TplLogWalkerHistoryData);
}


static void
tpl_log_walker_history_data_free (TplLogWalkerHistoryData *data)
{
  g_object_unref (data->iter);
  g_slice_free (TplLogWalkerHistoryData, data);
}


static void
tpl_log_walker_async_operation_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  TplLogWalker *walker;
  TplLogWalkerPriv *priv;
  GSimpleAsyncResult *simple;
  TplLogWalkerAsyncData *async_data;

  walker = TPL_LOG_WALKER (source_object);
  priv = walker->priv;

  simple = G_SIMPLE_ASYNC_RESULT (result);
  async_data = (TplLogWalkerAsyncData *)
      g_simple_async_result_get_op_res_gpointer (simple);

  if (async_data->cb)
    async_data->cb (source_object, result, user_data);

  g_object_unref (g_queue_pop_head (priv->queue));
  tpl_log_walker_op_run (walker);
}


static void
tpl_log_walker_caches_free_func (gpointer data)
{
  g_list_free_full ((GList *) data, g_object_unref);
}


static void
tpl_log_walker_fill_cache_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  GError *error = NULL;
  TplLogWalkerAsyncData *async_data;

  async_data = (TplLogWalkerAsyncData *)
      g_simple_async_result_get_op_res_gpointer (simple);

  async_data->fill_cache->data = tpl_log_iter_get_events (
      TPL_LOG_ITER (async_data->fill_iter->data), CACHE_SIZE, &error);

  if (error != NULL)
    g_simple_async_result_take_error (simple, error);
}


static void
tpl_log_walker_fill_cache_async (TplLogWalker *walker,
    GList *cache,
    GList *iter,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *simple;
  TplLogWalkerAsyncData *async_data;

  g_return_if_fail (TPL_IS_LOG_WALKER (walker));

  async_data = tpl_log_walker_async_data_new ();
  async_data->fill_cache = cache;
  async_data->fill_iter = iter;

  simple = g_simple_async_result_new (G_OBJECT (walker), callback, user_data,
      tpl_log_walker_fill_cache_async);

  g_simple_async_result_set_op_res_gpointer (simple, async_data,
      (GDestroyNotify) tpl_log_walker_async_data_free);

  g_simple_async_result_run_in_thread (simple,
      tpl_log_walker_fill_cache_async_thread, G_PRIORITY_DEFAULT,
      NULL);

  g_object_unref (simple);
}


static gboolean
tpl_log_walker_fill_cache_finish (TplLogWalker *walker,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_WALKER (walker), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (walker), tpl_log_walker_fill_cache_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  return TRUE;
}


static void
tpl_log_walker_get_events (GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
  GSimpleAsyncResult *simple;
  TplLogWalker *walker;
  TplLogWalkerPriv *priv;
  TplLogWalkerAsyncData *async_data;
  guint i;

  walker = TPL_LOG_WALKER (source_object);
  priv = walker->priv;

  simple = G_SIMPLE_ASYNC_RESULT (user_data);
  async_data = (TplLogWalkerAsyncData *)
      g_simple_async_result_get_op_res_gpointer (simple);

  /* If we are returning from a prior call to
   * tpl_log_walker_fill_cache_async then finish it.
   */
  if (result != NULL)
    tpl_log_walker_fill_cache_finish (walker, result, NULL);

  if (priv->is_end == TRUE)
    goto out;

  i = g_list_length (async_data->events);

  while (i < async_data->num_events && priv->is_end == FALSE)
    {
      GList *cache;
      GList *iter;

      /* Continue the loop from where we left, or start from the
       * beginning as the case maybe.
       */

      cache = (async_data->fill_cache != NULL) ?
          async_data->fill_cache : priv->caches;

      iter = (async_data->fill_iter != NULL) ?
          async_data->fill_iter : priv->iters;

      for (; cache != NULL && iter != NULL;
           cache = g_list_next (cache), iter = g_list_next (iter))
        {
          GList *event;
          gint64 timestamp;

          if (cache->data == NULL)
            {
              /* If the cache could not be filled, then the store
               * must be empty.
               */
              if (cache == async_data->fill_cache)
                continue;

              /* Otherwise, try to fill it up. */
              async_data->fill_cache = cache;
              async_data->fill_iter = iter;
              tpl_log_walker_fill_cache_async (walker, cache, iter,
                  tpl_log_walker_get_events, simple);
              return;
            }

          event = g_list_last (cache->data);
          timestamp = tpl_event_get_timestamp (TPL_EVENT (event->data));
          if (timestamp > async_data->latest_timestamp)
            {
              async_data->latest_cache = cache;
              async_data->latest_event = event;
              async_data->latest_iter = iter;
              async_data->latest_timestamp = timestamp;
            }
        }

      /* These are used to maintain the continuity of the for loop
       * which can get interrupted by the calls to
       * tpl_log_walker_fill_cache_async(). Now that we are out of the
       * loop we should reset them.
       */
      async_data->fill_cache = NULL;
      async_data->fill_iter = NULL;
      async_data->latest_timestamp = 0;

      if (async_data->latest_event != NULL)
        {
          TplEvent *event;
          TplLogWalkerHistoryData *data;
          gboolean skip;

          event = async_data->latest_event->data;
          skip = TRUE;

          if (priv->filter == NULL ||
              (*priv->filter) (event, priv->filter_data))
            {
              async_data->events = g_list_prepend (async_data->events, event);
              i++;
              skip = FALSE;
            }

          async_data->latest_cache->data = g_list_delete_link (
                  async_data->latest_cache->data, async_data->latest_event);

          data = (priv->history != NULL) ?
              (TplLogWalkerHistoryData *) priv->history->data : NULL;

          if (data == NULL ||
              data->iter != async_data->latest_iter->data ||
              data->skip != skip)
            {
              data = tpl_log_walker_history_data_new ();
              data->iter = g_object_ref (async_data->latest_iter->data);
              data->skip = skip;
              priv->history = g_list_prepend (priv->history, data);
            }

          data->count++;

          /* Now that the event has been inserted into the list we can
           * forget about it.
           */
          async_data->latest_event = NULL;
        }
      else
        priv->is_end = TRUE;
    }

  /* We are still at the beginning if all the log stores were empty. */
  if (priv->history != NULL)
    priv->is_start = FALSE;

 out:
  g_simple_async_result_complete_in_idle (simple);
}


static void
tpl_log_walker_rewind (TplLogWalker *walker,
    guint num_events,
    GError **error)
{
  TplLogWalkerPriv *priv;
  GList *k;
  GList *l;
  guint i;

  g_return_if_fail (TPL_IS_LOG_WALKER (walker));

  priv = walker->priv;
  i = 0;

  if (priv->is_start == TRUE || num_events == 0)
    return;

  priv->is_end = FALSE;

  for (k = priv->caches, l = priv->iters;
       k != NULL && l != NULL;
       k = g_list_next (k), l = g_list_next (l))
    {
      GList **cache;
      TplLogIter *iter;
      guint length;

      cache = (GList **) &k->data;
      iter = TPL_LOG_ITER (l->data);

      /* Flush the cache. */
      length = g_list_length (*cache);
      tpl_log_iter_rewind (iter, length, error);
      g_list_free_full (*cache, g_object_unref);
      *cache = NULL;
    }

  while (i < num_events && priv->is_start == FALSE)
    {
      TplLogWalkerHistoryData *data;

      data = (TplLogWalkerHistoryData *) priv->history->data;
      tpl_log_iter_rewind (data->iter, 1, error);
      data->count--;
      if (!data->skip)
        i++;

      if (data->count == 0)
        {
          tpl_log_walker_history_data_free (data);
          priv->history = g_list_delete_link (priv->history, priv->history);
          if (priv->history == NULL)
            priv->is_start = TRUE;
        }
    }
}


static void
tpl_log_walker_rewind_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  GError *error = NULL;
  TplLogWalkerAsyncData *async_data;

  async_data = (TplLogWalkerAsyncData *)
      g_simple_async_result_get_op_res_gpointer (simple);

  tpl_log_walker_rewind (TPL_LOG_WALKER (object),
      async_data->num_events, &error);

  if (error != NULL)
    g_simple_async_result_take_error (simple, error);
}


static void
tpl_log_walker_op_run (TplLogWalker *walker)
{
  TplLogWalkerPriv *priv;
  GSimpleAsyncResult *simple;
  TplLogWalkerAsyncData *async_data;

  priv = walker->priv;

  if (g_queue_is_empty (priv->queue))
    return;

  simple = G_SIMPLE_ASYNC_RESULT (g_queue_peek_head (priv->queue));
  async_data = (TplLogWalkerAsyncData *)
      g_simple_async_result_get_op_res_gpointer (simple);

  switch (async_data->op_type)
    {
    case TPL_LOG_WALKER_OP_GET_EVENTS:
      tpl_log_walker_get_events (G_OBJECT (walker), NULL, simple);
      break;

    case TPL_LOG_WALKER_OP_REWIND:
      g_simple_async_result_run_in_thread (simple,
          tpl_log_walker_rewind_async_thread, G_PRIORITY_DEFAULT, NULL);
      break;
    }
}


static void
tpl_log_walker_dispose (GObject *object)
{
  TplLogWalkerPriv *priv;

  priv = TPL_LOG_WALKER (object)->priv;

  g_list_free_full (priv->caches, tpl_log_walker_caches_free_func);
  priv->caches = NULL;

  g_list_free_full (priv->history,
      (GDestroyNotify) tpl_log_walker_history_data_free);
  priv->history = NULL;

  g_list_free_full (priv->iters, g_object_unref);
  priv->iters = NULL;

  G_OBJECT_CLASS (tpl_log_walker_parent_class)->dispose (object);
}


static void
tpl_log_walker_finalize (GObject *object)
{
  TplLogWalkerPriv *priv;

  priv = TPL_LOG_WALKER (object)->priv;
  g_queue_free_full (priv->queue, g_object_unref);

  G_OBJECT_CLASS (tpl_log_walker_parent_class)->finalize (object);
}


static void
tpl_log_walker_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplLogWalkerPriv *priv;

  priv = TPL_LOG_WALKER (object)->priv;

  switch (param_id)
    {
    case PROP_FILTER:
      g_value_set_pointer (value, priv->filter);
      break;

    case PROP_FILTER_DATA:
      g_value_set_pointer (value, priv->filter_data);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
tpl_log_walker_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplLogWalkerPriv *priv;

  priv = TPL_LOG_WALKER (object)->priv;

  switch (param_id)
    {
    case PROP_FILTER:
      priv->filter = g_value_get_pointer (value);
      break;

    case PROP_FILTER_DATA:
      priv->filter_data = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
tpl_log_walker_init (TplLogWalker *walker)
{
  TplLogWalkerPriv *priv;

  walker->priv = G_TYPE_INSTANCE_GET_PRIVATE (walker, TPL_TYPE_LOG_WALKER,
      TplLogWalkerPriv);
  priv = walker->priv;

  priv->queue = g_queue_new ();
  priv->is_start = TRUE;
  priv->is_end = FALSE;
}


static void
tpl_log_walker_class_init (TplLogWalkerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->dispose = tpl_log_walker_dispose;
  object_class->finalize = tpl_log_walker_finalize;
  object_class->get_property = tpl_log_walker_get_property;
  object_class->set_property = tpl_log_walker_set_property;

  param_spec = g_param_spec_pointer ("filter",
      "Filter",
      "An optional filter function",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_FILTER, param_spec);

  param_spec = g_param_spec_pointer ("filter-data",
      "Filter Data",
      "User data to pass to the filter function",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_FILTER_DATA, param_spec);

  g_type_class_add_private (klass, sizeof (TplLogWalkerPriv));
}


TplLogWalker *
tpl_log_walker_new (TplLogEventFilter filter, gpointer filter_data)
{
  return g_object_new (TPL_TYPE_LOG_WALKER,
      "filter", filter,
      "filter-data", filter_data,
      NULL);
}


void
tpl_log_walker_add_iter (TplLogWalker *walker, TplLogIter *iter)
{
  TplLogWalkerPriv *priv;

  g_return_if_fail (TPL_IS_LOG_WALKER (walker));
  g_return_if_fail (TPL_IS_LOG_ITER (iter));

  priv = walker->priv;

  priv->iters = g_list_prepend (priv->iters, g_object_ref (iter));
  priv->caches = g_list_prepend (priv->caches, NULL);
}


/**
 * tpl_log_walker_get_events_async:
 * @walker: a #TplLogWalker
 * @num_events: number of maximum events to fetch
 * @callback: (scope async) (allow-none): a callback to call when
 * the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Walk the logs to retrieve the next most recent @num_event events.
 *
 * Since: 0.8.0
 */
void
tpl_log_walker_get_events_async (TplLogWalker *walker,
    guint num_events,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogWalkerPriv *priv;
  GSimpleAsyncResult *simple;
  TplLogWalkerAsyncData *async_data;

  g_return_if_fail (TPL_IS_LOG_WALKER (walker));

  priv = walker->priv;

  async_data = tpl_log_walker_async_data_new ();
  async_data->cb = callback;
  async_data->num_events = num_events;
  async_data->op_type = TPL_LOG_WALKER_OP_GET_EVENTS;

  simple = g_simple_async_result_new (G_OBJECT (walker),
      tpl_log_walker_async_operation_cb, user_data,
      tpl_log_walker_get_events_async);

  g_simple_async_result_set_op_res_gpointer (simple, async_data,
      (GDestroyNotify) tpl_log_walker_async_data_free);

  g_queue_push_tail (priv->queue, g_object_ref (simple));
  if (g_queue_get_length (priv->queue) == 1)
    tpl_log_walker_op_run (walker);

  g_object_unref (simple);
}


/**
 * tpl_log_walker_get_events_finish:
 * @walker: a #TplLogWalker
 * @result: a #GAsyncResult
 * @events: (out) (transfer full) (element-type TelepathyLogger.Event):
 *  a pointer to a #GList used to return the list #TplEvent
 * @error: a #GError to fill
 *
 * Returns: #TRUE if the operation was successful, otherwise #FALSE.
 *
 * Since: 0.8.0
 */
gboolean
tpl_log_walker_get_events_finish (TplLogWalker *walker,
    GAsyncResult *result,
    GList **events,
    GError **error)
{
  GSimpleAsyncResult *simple;
  TplLogWalkerAsyncData *async_data;

  g_return_val_if_fail (TPL_IS_LOG_WALKER (walker), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (walker), tpl_log_walker_get_events_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);
  async_data = (TplLogWalkerAsyncData *)
      g_simple_async_result_get_op_res_gpointer (simple);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  if (events != NULL)
    {
      *events = async_data->events;
      async_data->events = NULL;
    }

  return TRUE;
}


/**
 * tpl_log_walker_rewind_async:
 * @walker: a #TplLogWalker
 * @num_events: number of events to move back
 * @callback: (scope async) (allow-none): a callback to call when
 * the request is satisfied
 * @user_data: data to pass to @callback
 *
 * Move the @walker back by the last @num_event events that were
 * returned by tpl_log_walker_get_events_async().
 *
 * Since: 0.8.0
 */
void
tpl_log_walker_rewind_async (TplLogWalker *walker,
    guint num_events,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TplLogWalkerPriv *priv;
  GSimpleAsyncResult *simple;
  TplLogWalkerAsyncData *async_data;

  g_return_if_fail (TPL_IS_LOG_WALKER (walker));

  priv = walker->priv;

  async_data = tpl_log_walker_async_data_new ();
  async_data->cb = callback;
  async_data->num_events = num_events;
  async_data->op_type = TPL_LOG_WALKER_OP_REWIND;

  simple = g_simple_async_result_new (G_OBJECT (walker),
      tpl_log_walker_async_operation_cb, user_data,
      tpl_log_walker_rewind_async);

  g_simple_async_result_set_op_res_gpointer (simple, async_data,
      (GDestroyNotify) tpl_log_walker_async_data_free);

  g_queue_push_tail (priv->queue, g_object_ref (simple));
  if (g_queue_get_length (priv->queue) == 1)
    tpl_log_walker_op_run (walker);

  g_object_unref (simple);
}


/**
 * tpl_log_walker_rewind_finish:
 * @walker: a #TplLogWalker
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Returns: #TRUE if the operation was successful, otherwise #FALSE.
 *
 * Since: 0.8.0
 */
gboolean
tpl_log_walker_rewind_finish (TplLogWalker *walker,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_WALKER (walker), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (walker), tpl_log_walker_rewind_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  return TRUE;
}


/**
 * tpl_log_walker_is_start:
 * @walker: a #TplLogWalker
 *
 * Determines whether @walker is pointing at the most recent event in
 * the logs. This is the case when @walker has not yet returned any
 * events or has been rewound completely.
 *
 * Returns: #TRUE if @walker is pointing at the most recent event,
 * otherwise #FALSE.
 *
 * Since: 0.8.0
 */
gboolean
tpl_log_walker_is_start (TplLogWalker *walker)
{
  TplLogWalkerPriv *priv;

  priv = walker->priv;
  return priv->is_start;
}


/**
 * tpl_log_walker_is_end:
 * @walker: a #TplLogWalker
 *
 * Determines whether @walker has run out of events. This is the case
 * when @walker has returned all the events from the logs.
 *
 * Returns: #TRUE if @walker has run out of events, otherwise #FALSE.
 *
 * Since: 0.8.0
 */
gboolean
tpl_log_walker_is_end (TplLogWalker *walker)
{
  TplLogWalkerPriv *priv;

  priv = walker->priv;
  return priv->is_end;
}
