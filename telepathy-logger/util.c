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

#include "util.h"

#include <telepathy-glib/util.h>

void
tpl_object_unref_if_not_null (gpointer data)
{
  if (data && G_IS_OBJECT (data))
    {
      g_object_unref (data);
    }
}

void
tpl_object_ref_if_not_null (gpointer data)
{
  if (data && G_IS_OBJECT (data))
    {
      g_object_ref (data);
    }
}

gboolean
tpl_strequal (const gchar *left, const gchar *right)
{
	return ! tp_strdiff(left, right);
}

TplActionChain *
tpl_actionchain_new (GObject *obj,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  TplActionChain *ret = g_slice_new0 (TplActionChain);

  ret->chain = g_queue_new ();
  ret->simple = g_simple_async_result_new (obj, cb, user_data,
      tpl_actionchain_finish);

  return ret;
}


gpointer
tpl_actionchain_get_object (TplActionChain *self)
{
  g_return_val_if_fail ( self != NULL && self->simple != NULL, NULL);

  return g_async_result_get_source_object (G_ASYNC_RESULT (self->simple));
}


void
tpl_actionchain_free (TplActionChain *self)
{
  g_queue_free (self->chain);
  /* TODO free self->simple, I canont understand how */
  g_free (self);
}


void
tpl_actionchain_append (TplActionChain *self,
    TplPendingAction func)
{
  g_queue_push_tail (self->chain, func);
}


void
tpl_actionchain_continue (TplActionChain *self)
{
  if (g_queue_is_empty (self->chain))
    {
      g_simple_async_result_complete (self->simple);
    }
  else
    {
      TplPendingAction next_action = g_queue_pop_head (self->chain);
      next_action(self);
    }
}


gboolean
tpl_actionchain_finish (GAsyncResult *result)
{
  /* TODO returnm the real result */
  return TRUE;
}
