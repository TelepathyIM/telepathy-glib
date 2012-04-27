/*
 * future-account.h - object for a currently non-existent account to create
 *
 * Copyright (C) 2012 Collabora Ltd. <http://www.collabora.co.uk/>
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

#ifndef TP_FUTURE_ACCOUNT_H
#define TP_FUTURE_ACCOUNT_H

#include <telepathy-glib/account-manager.h>

G_BEGIN_DECLS

typedef struct _TpFutureAccount TpFutureAccount;
typedef struct _TpFutureAccountClass TpFutureAccountClass;
typedef struct _TpFutureAccountPrivate TpFutureAccountPrivate;

struct _TpFutureAccount {
    /*<private>*/
    GObject parent;
    TpFutureAccountPrivate *priv;
};

struct _TpFutureAccountClass {
    /*<private>*/
    GObjectClass parent_class;
    GCallback _padding[7];
};

GType tp_future_account_get_type (void);

#define TP_TYPE_FUTURE_ACCOUNT \
  (tp_future_account_get_type ())
#define TP_FUTURE_ACCOUNT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_FUTURE_ACCOUNT, \
                               TpFutureAccount))
#define TP_FUTURE_ACCOUNT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_FUTURE_ACCOUNT, \
                            TpFutureAccountClass))
#define TP_IS_FUTURE_ACCOUNT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_FUTURE_ACCOUNT))
#define TP_IS_FUTURE_ACCOUNT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_FUTURE_ACCOUNT))
#define TP_FUTURE_ACCOUNT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_FUTURE_ACCOUNT, \
                              TpFutureAccountClass))

TpFutureAccount * tp_future_account_new (TpAccountManager *account_manager,
    const gchar *manager, const gchar *protocol) G_GNUC_WARN_UNUSED_RESULT;

void tp_future_account_set_display_name (TpFutureAccount *self,
    const gchar *name);

void tp_future_account_set_icon_name (TpFutureAccount *self,
    const gchar *icon);

void tp_future_account_set_nickname (TpFutureAccount *self,
    const gchar *nickname);

void tp_future_account_set_requested_presence (TpFutureAccount *self,
    TpConnectionPresenceType presence,
    const gchar *status, const gchar *message);

void tp_future_account_set_automatic_presence (TpFutureAccount *self,
    TpConnectionPresenceType presence,
    const gchar *status, const gchar *message);

void tp_future_account_set_enabled (TpFutureAccount *self,
    gboolean enabled);

void tp_future_account_set_connect_automatically (TpFutureAccount *self,
    gboolean connect_automatically);

void tp_future_account_add_supersedes (TpFutureAccount *self,
    const gchar *superseded_path);

/* parameters */
void tp_future_account_set_parameter (TpFutureAccount *self,
    const gchar *key, GVariant *value);

void tp_future_account_unset_parameter (TpFutureAccount *self,
    const gchar *key);

void tp_future_account_set_parameter_string (TpFutureAccount *self,
    const gchar *key, const gchar *value);

/* create it */
void tp_future_account_create_account_async (TpFutureAccount *self,
    GAsyncReadyCallback callback, gpointer user_data);

TpAccount * tp_future_account_create_account_finish (TpFutureAccount *self,
    GAsyncResult *result, GError **error);

G_END_DECLS

#endif
