/*
 * account-manager.h - proxy for the Telepathy account manager
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
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
 */

#ifndef TP_ACCOUNT_MANAGER_H
#define TP_ACCOUNT_MANAGER_H

#include <telepathy-glib/proxy.h>
#include <telepathy-glib/dbus.h>

G_BEGIN_DECLS

typedef struct _TpAccountManager TpAccountManager;
typedef struct _TpAccountManagerClass TpAccountManagerClass;
typedef struct _TpAccountManagerPrivate TpAccountManagerPrivate;
typedef struct _TpAccountManagerClassPrivate TpAccountManagerClassPrivate;

struct _TpAccountManager {
    /*<private>*/
    TpProxy parent;
    TpAccountManagerPrivate *priv;
};

struct _TpAccountManagerClass {
    /*<private>*/
    TpProxyClass parent_class;
    GCallback _padding[7];
    TpAccountManagerClassPrivate *priv;
};

GType tp_account_manager_get_type (void);

#define TP_TYPE_ACCOUNT_MANAGER \
  (tp_account_manager_get_type ())
#define TP_ACCOUNT_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_ACCOUNT_MANAGER, \
                               TpAccountManager))
#define TP_ACCOUNT_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_ACCOUNT_MANAGER, \
                            TpAccountManagerClass))
#define TP_IS_ACCOUNT_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_ACCOUNT_MANAGER))
#define TP_IS_ACCOUNT_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_ACCOUNT_MANAGER))
#define TP_ACCOUNT_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_ACCOUNT_MANAGER, \
                              TpAccountManagerClass))

TpAccountManager *tp_account_manager_new (TpDBusDaemon *bus_daemon);

void tp_account_manager_init_known_interfaces (void);

G_END_DECLS

#include <telepathy-glib/_gen/tp-cli-account-manager.h>

#endif
