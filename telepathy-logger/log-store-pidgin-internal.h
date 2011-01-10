/*
 * Copyright (C) 2008-2011 Collabora Ltd.
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
 * Authors: Jonny Lamb <jonny.lamb@collabora.co.uk>
 *          Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#ifndef __TPL_LOG_STORE_PIDGIN_H__
#define __TPL_LOG_STORE_PIDGIN_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_STORE_PIDGIN \
  (tpl_log_store_pidgin_get_type ())
#define TPL_LOG_STORE_PIDGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_LOG_STORE_PIDGIN, \
      TplLogStorePidgin))
#define TPL_LOG_STORE_PIDGIN_CLASS(vtable) \
  (G_TYPE_CHECK_CLASS_CAST ((vtable), TPL_TYPE_LOG_STORE_PIDGIN, \
      TplLogStorePidginClass))
#define TPL_IS_LOG_STORE_PIDGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_LOG_STORE_PIDGIN))
#define TPL_IS_LOG_STORE_PIDGIN_CLASS(vtable) \
  (G_TYPE_CHECK_CLASS_TYPE ((vtable), TPL_TYPE_LOG_STORE_PIDGIN))
#define TPL_LOG_STORE_PIDGIN_GET_CLASS(inst) \
  (G_TYPE_INSTANCE_GET_CLASS ((inst), TPL_TYPE_LOG_STORE_PIDGIN, \
      TplLogStorePidginClass))

typedef struct _TplLogStorePidginPriv TplLogStorePidginPriv;

typedef struct
{
  GObject parent;
  TplLogStorePidginPriv *priv;
} TplLogStorePidgin;

typedef struct
{
  GObjectClass parent;
} TplLogStorePidginClass;

GType tpl_log_store_pidgin_get_type (void);

G_END_DECLS

#endif /* __TPL_LOG_STORE_PIDGIN_H__ */
