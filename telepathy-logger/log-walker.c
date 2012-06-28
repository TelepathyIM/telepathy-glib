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
  GList *iters;
};


G_DEFINE_TYPE (TplLogWalker, tpl_log_walker, G_TYPE_OBJECT);


static void
tpl_log_walker_dispose (GObject *object)
{
  TplLogWalkerPriv *priv;

  priv = TPL_LOG_WALKER (object)->priv;

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
}
