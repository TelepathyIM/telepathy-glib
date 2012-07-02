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
 */

/**
 * TplLogWalker:
 *
 * An object used to iterate over the logs
 */

struct _TplLogWalkerPriv
{
  GList *caches;
  GList *iters;
};


G_DEFINE_TYPE (TplLogWalker, tpl_log_walker, G_TYPE_OBJECT);


static const gsize CACHE_SIZE = 5;

typedef struct
{
  GAsyncReadyCallback cb;
  gpointer user_data;
  guint num_events;
} TplLogWalkerAsyncData;


static TplLogWalkerAsyncData *
tpl_log_walker_async_data_new (void)
{
  return g_slice_new0 (TplLogWalkerAsyncData);
}


static void
tpl_log_walker_async_data_free (TplLogWalkerAsyncData *data)
{
  g_slice_free (TplLogWalkerAsyncData, data);
}


static void
tpl_log_walker_async_operation_cb (GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  TplLogWalkerAsyncData *async_data = (TplLogWalkerAsyncData *) user_data;

  if (async_data->cb)
    async_data->cb (source_object, result, async_data->user_data);

  tpl_log_walker_async_data_free (async_data);
}


static void
tpl_log_walker_caches_free_func (gpointer data)
{
  g_list_free_full ((GList *) data, g_object_unref);
}


static GList *
tpl_log_walker_get_events (TplLogWalker *walker,
    guint num_events,
    GError **error)
{
  TplLogWalkerPriv *priv;
  GList *events;
  guint i;

  g_return_val_if_fail (TPL_IS_LOG_WALKER (walker), NULL);

  priv = walker->priv;
  events = NULL;
  i = 0;

  while (i < num_events)
    {
      GList *k;
      GList *l;
      GList **latest_cache;
      GList *latest_event;
      gint64 latest_timestamp;

      latest_cache = NULL;
      latest_event = NULL;
      latest_timestamp = 0;

      for (k = priv->caches, l = priv->iters;
           k != NULL && l != NULL;
           k = g_list_next (k), l = g_list_next (l))
        {
          GList **cache;
          GList *event;
          TplLogIter *iter;
          gint64 timestamp;

          cache = (GList **) &k->data;
          iter = TPL_LOG_ITER (l->data);

          /* If the cache is empty, try to fill it up. */
          if (*cache == NULL)
            *cache = tpl_log_iter_get_events (iter, CACHE_SIZE, error);

          /* If it could not be filled, then the store must be empty. */
          if (*cache == NULL)
            continue;

          event = g_list_last (*cache);
          timestamp = tpl_event_get_timestamp (TPL_EVENT (event->data));
          if (timestamp > latest_timestamp)
            {
              latest_cache = cache;
              latest_event = event;
              latest_timestamp = timestamp;
            }
        }

      if (latest_event != NULL)
        {
          *latest_cache = g_list_remove_link (*latest_cache, latest_event);
          events = g_list_prepend (events, latest_event->data);
          i++;
        }
      else
        break;
    }

  return events;
}


static void
tpl_log_walker_get_events_async_thread (GSimpleAsyncResult *simple,
    GObject *object,
    GCancellable *cancellable)
{
  GError *error = NULL;
  GList *events;
  TplLogWalkerAsyncData *async_data;

  async_data = (TplLogWalkerAsyncData *) g_async_result_get_user_data (
      G_ASYNC_RESULT (simple));

  events = tpl_log_walker_get_events (TPL_LOG_WALKER (object),
      async_data->num_events, &error);

  if (error != NULL)
    {
      g_simple_async_result_set_from_error (simple, error);
      g_error_free (error);
    }

  g_simple_async_result_set_op_res_gpointer (simple, events, NULL);
}


static void
tpl_log_walker_dispose (GObject *object)
{
  TplLogWalkerPriv *priv;

  priv = TPL_LOG_WALKER (object)->priv;

  g_list_free_full (priv->caches, tpl_log_walker_caches_free_func);
  priv->caches = NULL;

  g_list_free_full (priv->iters, g_object_unref);
  priv->iters = NULL;

  G_OBJECT_CLASS (tpl_log_walker_parent_class)->dispose (object);
}


static void
tpl_log_walker_finalize (GObject *object)
{
  G_OBJECT_CLASS (tpl_log_walker_parent_class)->finalize (object);
}


static void
tpl_log_walker_init (TplLogWalker *walker)
{
  walker->priv = G_TYPE_INSTANCE_GET_PRIVATE (walker, TPL_TYPE_LOG_WALKER,
      TplLogWalkerPriv);
}


static void
tpl_log_walker_class_init (TplLogWalkerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = tpl_log_walker_dispose;
  object_class->finalize = tpl_log_walker_finalize;

  g_type_class_add_private (klass, sizeof (TplLogWalkerPriv));
}


TplLogWalker *
tpl_log_walker_new (void)
{
  return g_object_new (TPL_TYPE_LOG_WALKER, NULL);
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
 */
void
tpl_log_walker_get_events_async (TplLogWalker *walker,
    guint num_events,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *simple;
  TplLogWalkerAsyncData *async_data;

  g_return_if_fail (TPL_IS_LOG_WALKER (walker));

  async_data = tpl_log_walker_async_data_new ();
  async_data->cb = callback;
  async_data->user_data = user_data;
  async_data->num_events = num_events;

  simple = g_simple_async_result_new (G_OBJECT (walker),
      tpl_log_walker_async_operation_cb, async_data,
      tpl_log_walker_get_events_async);

  g_simple_async_result_run_in_thread (simple,
      tpl_log_walker_get_events_async_thread, G_PRIORITY_DEFAULT,
      NULL);

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
 */
gboolean
tpl_log_walker_get_events_finish (TplLogWalker *walker,
    GAsyncResult *result,
    GList **events,
    GError **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (TPL_IS_LOG_WALKER (walker), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (walker), tpl_log_walker_get_events_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  if (events != NULL)
    *events = (GList *) g_simple_async_result_get_op_res_gpointer (simple);

  return TRUE;
}
