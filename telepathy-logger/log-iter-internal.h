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

#ifndef __TPL_LOG_ITER_H__
#define __TPL_LOG_ITER_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_ITER (tpl_log_iter_get_type ())

#define TPL_LOG_ITER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   TPL_TYPE_LOG_ITER, TplLogIter))

#define TPL_LOG_ITER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   TPL_TYPE_LOG_ITER, TplLogIterClass))

#define TPL_IS_LOG_ITER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   TPL_TYPE_LOG_ITER))

#define TPL_IS_LOG_ITER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   TPL_TYPE_LOG_ITER))

#define TPL_LOG_ITER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   TPL_TYPE_LOG_ITER, TplLogIterClass))

typedef struct _TplLogIter        TplLogIter;
typedef struct _TplLogIterClass   TplLogIterClass;

struct _TplLogIter
{
  GObject parent_instance;
};

struct _TplLogIterClass
{
  GObjectClass parent_class;

  GList * (*get_events) (TplLogIter *self, guint num_events, GError **error);
  void (*rewind) (TplLogIter *self, guint num_events, GError **error);
};

GType tpl_log_iter_get_type (void) G_GNUC_CONST;

GList *tpl_log_iter_get_events (TplLogIter *self,
    guint num_events,
    GError **error);

void tpl_log_iter_rewind (TplLogIter *self,
    guint num_events,
    GError **error);

G_END_DECLS

#endif /* __TPL_LOG_ITER_H__ */
