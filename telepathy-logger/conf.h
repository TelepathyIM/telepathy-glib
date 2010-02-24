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

#ifndef __TPL_CONF_H__
#define __TPL_CONF_H__

#include <gconf/gconf-client.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define TPL_TYPE_CONF                  (tpl_conf_get_type ())
#define TPL_CONF(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_CONF, TplConf))
#define TPL_CONF_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CONF, TplConfClass))
#define TPL_IS_CONF(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_CONF))
#define TPL_IS_CONF_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CONF))
#define TPL_CONF_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CONF, TplConfClass))

#define TPL_CONF_ERROR g_quark_from_static_string ("tpl-conf-error-quark")

typedef enum
{
  /* generic error */
  TPL_CONF_ERROR_FAILED,
  /* GCONF KEY ERROR */
  TPL_CONF_ERROR_GCONF_KEY
} TplConfError;

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

GType tpl_conf_get_type (void);

TplConf *tpl_conf_dup (void);

GConfClient *tpl_conf_get_gconf_client (TplConf *self);

gboolean  tpl_conf_is_globally_enabled (TplConf * self, GError **error);

gboolean tpl_conf_is_account_ignored (TplConf *self, const gchar *account_path, GError **error);

GSList *tpl_conf_get_accounts_ignorelist (TplConf * self, GError **error);

void tpl_conf_globally_enable (TplConf *self, gboolean enable, GError **error);

void tpl_conf_set_accounts_ignorelist (TplConf *self, GSList *newlist,
    GError **error);
G_END_DECLS

#endif // __TPL_CONF_H__
