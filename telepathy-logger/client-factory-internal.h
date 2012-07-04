/*
 * Copyright (C) 2012 Collabora Ltd.
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
 * Authors: Xavier Claessens <xavier.claessens@collabora.co.uk>
 */

#ifndef __TPL_CLIENT_FACTORY_H__
#define __TPL_CLIENT_FACTORY_H__

#include <telepathy-glib/telepathy-glib.h>

typedef struct _TplClientFactory TplClientFactory;
typedef struct _TplClientFactoryClass TplClientFactoryClass;

struct _TplClientFactoryClass {
    /*<public>*/
    TpAutomaticClientFactoryClass parent_class;
};

struct _TplClientFactory {
    /*<private>*/
    TpAutomaticClientFactory parent;
};

GType _tpl_client_factory_get_type (void);

#define TPL_TYPE_CLIENT_FACTORY \
  (_tpl_client_factory_get_type ())
#define TPL_CLIENT_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_CLIENT_FACTORY, \
                               TplClientFactory))
#define TPL_CLIENT_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CLIENT_FACTORY, \
                            TplClientFactoryClass))
#define TPL_IS_CLIENT_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_CLIENT_FACTORY))
#define TPL_IS_CLIENT_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CLIENT_FACTORY))
#define TPL_CLIENT_FACTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CLIENT_FACTORY, \
                              TplClientFactoryClass))

TpSimpleClientFactory *_tpl_client_factory_new (TpDBusDaemon *dbus);

#endif /* __TPL_CLIENT_FACTORY_H__ */
