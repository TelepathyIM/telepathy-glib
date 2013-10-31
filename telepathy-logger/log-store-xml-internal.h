/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2010 Collabora Ltd.
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 *          Jonny Lamb <jonny.lamb@collabora.co.uk>
 */

#ifndef __TPL_LOG_STORE_XML_H__
#define __TPL_LOG_STORE_XML_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS
#define TPL_TYPE_LOG_STORE_XML \
  (_tpl_log_store_xml_get_type ())
#define TPL_LOG_STORE_XML(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_LOG_STORE_XML, \
                               TplLogStoreXml))
#define TPL_LOG_STORE_XML_CLASS(vtable) \
  (G_TYPE_CHECK_CLASS_CAST ((vtable), TPL_TYPE_LOG_STORE_XML, \
                            TplLogStoreXmlClass))
#define TPL_IS_LOG_STORE_XML(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_LOG_STORE_XML))
#define TPL_IS_LOG_STORE_XML_CLASS(vtable) \
  (G_TYPE_CHECK_CLASS_TYPE ((vtable), TPL_TYPE_LOG_STORE_XML))
#define TPL_LOG_STORE_XML_GET_CLASS(inst) \
  (G_TYPE_INSTANCE_GET_CLASS ((inst), TPL_TYPE_LOG_STORE_XML, \
                              TplLogStoreXmlClass))

typedef struct _TplLogStoreXmlPriv TplLogStoreXmlPriv;

typedef struct TplLogStoreXml
{
  GObject parent;
  TplLogStoreXmlPriv *priv;
} TplLogStoreXml;

typedef struct
{
  GObjectClass parent;
} TplLogStoreXmlClass;

GType _tpl_log_store_xml_get_type (void);

G_END_DECLS
#endif /* __TPL_LOG_STORE_XML_H__ */
