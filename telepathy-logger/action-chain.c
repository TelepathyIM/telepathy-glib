/*
 * Copyright (C) 2009 Collabora Ltd.
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
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "config.h"
#include "action-chain-internal.h"

typedef struct {
  TplPendingAction action;
  gpointer user_data;
} TplActionLink;


TplActionChain *
_tpl_action_chain_new_async (GObject *obj,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  TplActionChain *ret = g_slice_new0 (TplActionChain);

  ret->chain = g_queue_new ();
  ret->simple = g_simple_async_result_new (obj, cb, user_data,
      _tpl_action_chain_new_async);

  g_object_set_data (G_OBJECT (ret->simple), "chain", ret);

  return ret;
}


static void
link_free (TplActionLink *l)
{
  g_slice_free (TplActionLink, l);
}


void
_tpl_action_chain_free (TplActionChain *self)
{
  g_queue_foreach (self->chain, (GFunc) link_free, NULL);
  g_queue_free (self->chain);
  g_object_unref (self->simple);
  g_slice_free (TplActionChain, self);
}


gpointer // FIXME GObject *
_tpl_action_chain_get_object (TplActionChain *self)
{
  GObject *obj;

  g_return_val_if_fail (self != NULL && self->simple != NULL, NULL);

  obj = g_async_result_get_source_object (G_ASYNC_RESULT (self->simple));
  g_object_unref (obj); /* don't want the extra ref */

  return obj;
}


void
_tpl_action_chain_prepend (TplActionChain *self,
    TplPendingAction func,
    gpointer user_data)
{
  TplActionLink *l;

  l = g_slice_new0 (TplActionLink);
  l->action = func;
  l->user_data = user_data;

  g_queue_push_head (self->chain, l);
}


void
_tpl_action_chain_append (TplActionChain *self,
    TplPendingAction func,
    gpointer user_data)
{
  TplActionLink *l;

  l = g_slice_new0 (TplActionLink);
  l->action = func;
  l->user_data = user_data;

  g_queue_push_tail (self->chain, l);
}


void
_tpl_action_chain_continue (TplActionChain *self)
{
  if (g_queue_is_empty (self->chain))
    g_simple_async_result_complete (self->simple);
  else
    {
      TplActionLink *l = g_queue_pop_head (self->chain);

      l->action (self, l->user_data);
      link_free (l);
    }
}


void
_tpl_action_chain_terminate (TplActionChain *self,
    const GError *error)
{
  GSimpleAsyncResult *simple = self->simple;

  g_assert (error != NULL);

  g_simple_async_result_set_from_error (simple, error);
  g_simple_async_result_complete (simple);
}


/**
 * _tpl_action_chain_new_finish:
 * @source: the #GObject pass to _tpl_action_chain_new_async()
 * @result: the #GAsyncResult pass in callback
 * @error: a pointer to a #GError that will be set on error, or NULL to ignore
 *
 * Get the result from running the action chain (%TRUE if the chain completed
 * successfully, %FALSE with @error set if it was terminated).
 *
 * This function also frees the chain.
 *
 * Returns: %TRUE on success, %FALSE with @error set on error.
 */
gboolean
_tpl_action_chain_new_finish (GObject *source,
    GAsyncResult *result,
    GError **error)
{
  TplActionChain *chain;

  g_return_val_if_fail (g_simple_async_result_is_valid (result, source,
        _tpl_action_chain_new_async), FALSE);

  chain = g_object_get_data (G_OBJECT (result), "chain");

  g_return_val_if_fail (chain != NULL, FALSE);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
        error))
    return FALSE;

  _tpl_action_chain_free (chain);
  return TRUE;
}
