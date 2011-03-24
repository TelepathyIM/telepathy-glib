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

#ifndef __TPL_CONF_H__
#define __TPL_CONF_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TPL_TYPE_CONF                  (_tpl_conf_get_type ())
#define TPL_CONF(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_CONF, TplConf))
#define TPL_CONF_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CONF, TplConfClass))
#define TPL_IS_CONF(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_CONF))
#define TPL_IS_CONF_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CONF))
#define TPL_CONF_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CONF, TplConfClass))

typedef struct
{
  GObject parent;

  /* private */
  gpointer priv;
} TplConf;

typedef struct
{
  GObjectClass parent_class;
} TplConfClass;

GType _tpl_conf_get_type (void);
TplConf *_tpl_conf_dup (void);

gboolean  _tpl_conf_is_globally_enabled (TplConf *self);
gboolean _tpl_conf_is_account_ignored (TplConf *self,
    const gchar *account_path);
// GSList *_tpl_conf_get_accounts_ignorelist (TplConf *self);

void _tpl_conf_globally_enable (TplConf *self, gboolean enable);
// void _tpl_conf_set_accounts_ignorelist (TplConf *self, GSList *newlist);
G_END_DECLS

#endif // __TPL_CONF_H__
