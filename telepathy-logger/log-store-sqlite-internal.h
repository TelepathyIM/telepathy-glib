/*
 * Copyright (C) 2010 Collabora Ltd.
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
 * Authors: Danielle Madeley <danielle.madeley@collabora.co.uk>
 */

#ifndef __TPL_LOG_STORE_SQLITE_H__
#define __TPL_LOG_STORE_SQLITE_H__

#include <glib-object.h>

#include <telepathy-glib/telepathy-glib.h>

#include <telepathy-logger/log-store-internal.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_STORE_SQLITE \
  (_tpl_log_store_sqlite_get_type ())
#define TPL_LOG_STORE_SQLITE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_LOG_STORE_SQLITE, \
                               TplLogStoreSqlite))
#define TPL_LOG_STORE_SQLITE_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST ((obj), TPL_TYPE_LOG_STORE_SQLITE, \
                            TplLogStoreSqliteClass))
#define TPL_IS_LOG_STORE_SQLITE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_LOG_STORE_SQLITE))
#define TPL_IS_LOG_STORE_SQLITE_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE ((obj), TPL_TYPE_LOG_STORE_SQLITE))
#define TPL_LOG_STORE_SQLITE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_LOG_STORE_SQLITE, \
                              TplLogStoreSqliteClass))

#define TPL_LOG_STORE_SQLITE_CLEANUP_DELTA_LIMIT (60*60)
#define TPL_LOG_STORE_SQLITE_TIMESTAMP_FORMAT "%Y-%m-%d %H:%M:%S"
#define TPL_LOG_STORE_SQLITE_ERROR g_quark_from_static_string ( \
    "tpl-log-store-index-error-quark")
typedef enum
{
  /* generic error, avoids clashing with TPL_LOG_STORE_ERROR using its last
   * value */
  TPL_LOG_STORE_SQLITE_ERROR_FAILED = TPL_LOG_STORE_ERROR_LAST,
  /* generic _tpl_log_store_sqlite_get_pending_messages() error, to be used when
   * any other code cannot be use, including TPL_LOG_STORE_ERROR ones */
  TPL_LOG_STORE_SQLITE_ERROR_GET_PENDING_MESSAGES,
  TPL_LOG_STORE_SQLITE_ERROR_REMOVE_PENDING_MESSAGES,
  TPL_LOG_STORE_SQLITE_ERROR_ADD_PENDING_MESSAGE
} TplLogStoreSqliteError;

typedef struct _TplLogStoreSqlite TplLogStoreSqlite;
typedef struct _TplLogStoreSqlitePrivate TplLogStoreSqlitePrivate;
typedef struct _TplLogStoreSqliteClass TplLogStoreSqliteClass;
typedef struct _TplPendingMessage TplPendingMessage;

struct _TplLogStoreSqlite
{
  GObject parent;
  TplLogStoreSqlitePrivate *priv;
};

struct _TplLogStoreSqliteClass
{
  GObjectClass parent_class;
};

struct _TplPendingMessage
{
  guint id;
  gint64 timestamp;
};

GType _tpl_log_store_sqlite_get_type (void);
TplLogStore * _tpl_log_store_sqlite_dup (void);

GList * _tpl_log_store_sqlite_get_pending_messages (TplLogStore *self,
    TpChannel *channel, GError **error);
gboolean _tpl_log_store_sqlite_remove_pending_messages (TplLogStore *self,
    TpChannel *channel, GList *log_ids, GError **error);
gboolean _tpl_log_store_sqlite_add_pending_message (TplLogStore *self,
    TpChannel *channel, guint id, gint64 timestamp, GError **error);

gint64 _tpl_log_store_sqlite_get_most_recent (TplLogStoreSqlite *self,
    TpAccount *account, const char *identifier);
double _tpl_log_store_sqlite_get_frequency (TplLogStoreSqlite *self,
    TpAccount *account, const char *identifier);

G_END_DECLS

#endif
