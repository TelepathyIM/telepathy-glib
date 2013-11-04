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
#include "log-iter-internal.h"


G_DEFINE_TYPE (TplLogIter, tpl_log_iter, G_TYPE_OBJECT);


static void
tpl_log_iter_dispose (GObject *object)
{
  G_OBJECT_CLASS (tpl_log_iter_parent_class)->dispose (object);
}


static void
tpl_log_iter_finalize (GObject *object)
{
  G_OBJECT_CLASS (tpl_log_iter_parent_class)->finalize (object);
}


static void
tpl_log_iter_init (TplLogIter *self)
{
}


static void
tpl_log_iter_class_init (TplLogIterClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = tpl_log_iter_dispose;
  object_class->finalize = tpl_log_iter_finalize;
}


GList *
tpl_log_iter_get_events (TplLogIter *self,
    guint num_events,
    GError **error)
{
  TplLogIterClass *log_iter_class;

  g_return_val_if_fail (TPL_IS_LOG_ITER (self), NULL);

  log_iter_class = TPL_LOG_ITER_GET_CLASS (self);

  if (log_iter_class->get_events == NULL)
    return NULL;

  return log_iter_class->get_events (self, num_events, error);
}


void
tpl_log_iter_rewind (TplLogIter *self,
    guint num_events,
    GError **error)
{
  TplLogIterClass *log_iter_class;

  g_return_if_fail (TPL_IS_LOG_ITER (self));

  log_iter_class = TPL_LOG_ITER_GET_CLASS (self);

  if (log_iter_class->rewind == NULL)
    return;

  log_iter_class->rewind (self, num_events, error);
}
