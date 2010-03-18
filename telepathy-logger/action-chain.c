/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
#include "action-chain.h"

typedef struct {
  TplPendingAction action;
  gpointer user_data;
} TplActionLink;


TplActionChain *
tpl_action_chain_new (GObject *obj,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  TplActionChain *ret = g_slice_new0 (TplActionChain);

  ret->chain = g_queue_new ();
  ret->simple = g_simple_async_result_new (obj, cb, user_data,
      tpl_action_chain_finish);

  g_object_set_data (G_OBJECT (ret->simple), "chain", ret);

  return ret;
}


static void
link_free (TplActionLink *link)
{
  g_slice_free (TplActionLink, link);
}


void
tpl_action_chain_free (TplActionChain *self)
{
  g_queue_foreach (self->chain, (GFunc) link_free, NULL);
  g_queue_free (self->chain);
  g_object_unref (self->simple);
  g_slice_free (TplActionChain, self);
}


gpointer // FIXME GObject *
tpl_action_chain_get_object (TplActionChain *self)
{
  GObject *obj;

  g_return_val_if_fail (self != NULL && self->simple != NULL, NULL);

  obj = g_async_result_get_source_object (G_ASYNC_RESULT (self->simple));
  g_object_unref (obj); /* don't want the extra ref */

  return obj;
}


void
tpl_action_chain_prepend (TplActionChain *self,
    TplPendingAction func,
    gpointer user_data)
{
  TplActionLink *link;

  link = g_slice_new0 (TplActionLink);
  link->action = func;
  link->user_data = user_data;

  g_queue_push_head (self->chain, link);
}


void
tpl_action_chain_append (TplActionChain *self,
    TplPendingAction func,
    gpointer user_data)
{
  TplActionLink *link;

  link = g_slice_new0 (TplActionLink);
  link->action = func;
  link->user_data = user_data;

  g_queue_push_tail (self->chain, link);
}


void
tpl_action_chain_continue (TplActionChain *self)
{
  if (g_queue_is_empty (self->chain))
    {
      GSimpleAsyncResult *simple = self->simple;

      g_simple_async_result_set_op_res_gboolean (simple, TRUE);
      g_simple_async_result_complete (simple);
    }
  else
    {
      TplActionLink *link = g_queue_pop_head (self->chain);

      link->action (self, link->user_data);
      link_free (link);
    }
}


void
tpl_action_chain_terminate (TplActionChain *self)
{
  GSimpleAsyncResult *simple = self->simple;

  g_simple_async_result_set_op_res_gboolean (simple, FALSE);
  g_simple_async_result_complete (simple);
}


/**
 * tpl_action_chain_finish:
 *
 * Get the result from running the action chain (%TRUE if the chain completed
 * successfully, %FALSE if it was terminated).
 *
 * This function also frees the chain.
 */
gboolean
tpl_action_chain_finish (GAsyncResult *result)
{
  TplActionChain *chain;
  gboolean retval;

  chain = g_object_get_data (G_OBJECT (result), "chain");

  retval = g_simple_async_result_get_op_res_gboolean (
      G_SIMPLE_ASYNC_RESULT (result));

  tpl_action_chain_free (chain);

  return retval;
}
