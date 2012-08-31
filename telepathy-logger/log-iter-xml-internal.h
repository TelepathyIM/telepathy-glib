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

#ifndef __TPL_LOG_ITER_XML_H__
#define __TPL_LOG_ITER_XML_H__

#include <telepathy-glib/telepathy-glib.h>

#include <telepathy-logger/entity.h>
#include <telepathy-logger/log-iter-internal.h>
#include <telepathy-logger/log-manager.h>
#include <telepathy-logger/log-store-internal.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_ITER_XML (tpl_log_iter_xml_get_type ())

#define TPL_LOG_ITER_XML(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   TPL_TYPE_LOG_ITER_XML, TplLogIterXml))

#define TPL_LOG_ITER_XML_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   TPL_TYPE_LOG_ITER_XML, TplLogIterXmlClass))

#define TPL_IS_LOG_ITER_XML(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   TPL_TYPE_LOG_ITER_XML))

#define TPL_IS_LOG_ITER_XML_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   TPL_TYPE_LOG_ITER_XML))

#define TPL_LOG_ITER_XML_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   TPL_TYPE_LOG_ITER_XML, TplLogIterXmlClass))

typedef struct _TplLogIterXml        TplLogIterXml;
typedef struct _TplLogIterXmlClass   TplLogIterXmlClass;
typedef struct _TplLogIterXmlPriv    TplLogIterXmlPriv;

struct _TplLogIterXml
{
  TplLogIter parent_instance;
  TplLogIterXmlPriv *priv;
};

struct _TplLogIterXmlClass
{
  TplLogIterClass parent_class;
};

GType tpl_log_iter_xml_get_type (void) G_GNUC_CONST;

TplLogIter *tpl_log_iter_xml_new (TplLogStore *store,
    TpAccount *account,
    TplEntity *target,
    gint type_mask);

G_END_DECLS

#endif /* __TPL_LOG_ITER_XML_H__ */
