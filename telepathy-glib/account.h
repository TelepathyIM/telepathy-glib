/*
 * account-manager.h - proxy for an account in the Telepathy account manager
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

#ifndef TP_ACCOUNT_H
#define TP_ACCOUNT_H

#include <telepathy-glib/proxy.h>
#include <telepathy-glib/dbus.h>

G_BEGIN_DECLS

typedef struct _TpAccount TpAccount;
typedef struct _TpAccountClass TpAccountClass;
typedef struct _TpAccountPrivate TpAccountPrivate;
typedef struct _TpAccountClassPrivate TpAccountClassPrivate;

struct _TpAccount {
    /*<private>*/
    TpProxy parent;
    TpAccountPrivate *priv;
};

struct _TpAccountClass {
    /*<private>*/
    TpProxyClass parent_class;
    GCallback _padding[7];
    TpAccountClassPrivate *priv;
};

GType tp_account_get_type (void);

#define TP_TYPE_ACCOUNT \
  (tp_account_get_type ())
#define TP_ACCOUNT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_ACCOUNT, \
                               TpAccount))
#define TP_ACCOUNT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_ACCOUNT, \
                            TpAccountClass))
#define TP_IS_ACCOUNT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_ACCOUNT))
#define TP_IS_ACCOUNT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_ACCOUNT))
#define TP_ACCOUNT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_ACCOUNT, \
                              TpAccountClass))

TpAccount *tp_account_new (TpDBusDaemon *bus_daemon, const gchar *object_path,
    GError **error);

void tp_account_init_known_interfaces (void);

G_END_DECLS

#include <telepathy-glib/_gen/tp-cli-account.h>

#endif
