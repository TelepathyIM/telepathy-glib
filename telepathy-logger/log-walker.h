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

#ifndef __TPL_LOG_WALKER_H__
#define __TPL_LOG_WALKER_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_WALKER (tpl_log_walker_get_type ())

#define TPL_LOG_WALKER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   TPL_TYPE_LOG_WALKER, TplLogWalker))

#define TPL_LOG_WALKER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   TPL_TYPE_LOG_WALKER, TplLogWalkerClass))

#define TPL_IS_LOG_WALKER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   TPL_TYPE_LOG_WALKER))

#define TPL_IS_LOG_WALKER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   TPL_TYPE_LOG_WALKER))

#define TPL_LOG_WALKER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   TPL_TYPE_LOG_WALKER, TplLogWalkerClass))

typedef struct _TplLogWalker        TplLogWalker;
typedef struct _TplLogWalkerClass   TplLogWalkerClass;
typedef struct _TplLogWalkerPriv    TplLogWalkerPriv;

struct _TplLogWalker
{
  GObject parent_instance;
  TplLogWalkerPriv *priv;
};

struct _TplLogWalkerClass
{
  /*< private >*/
  GObjectClass parent_class;
};

GType tpl_log_walker_get_type (void) G_GNUC_CONST;

void tpl_log_walker_get_events_async (TplLogWalker *walker,
    guint num_events,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tpl_log_walker_get_events_finish (TplLogWalker *walker,
    GAsyncResult *result,
    GList **events,
    GError **error);

void tpl_log_walker_rewind_async (TplLogWalker *walker,
    guint num_events,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tpl_log_walker_rewind_finish (TplLogWalker *walker,
    GAsyncResult *result,
    GError **error);

gboolean tpl_log_walker_is_start (TplLogWalker *walker);

gboolean tpl_log_walker_is_end (TplLogWalker *walker);

G_END_DECLS

#endif /* __TPL_LOG_WALKER_H__ */
