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

#ifndef __TPL_LOG_STORE_H__
#define __TPL_LOG_STORE_H__

#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>

#include <telepathy-logger/event.h>
#include <telepathy-logger/log-manager.h>

G_BEGIN_DECLS

#define TPL_TYPE_LOG_STORE (_tpl_log_store_get_type ())
#define TPL_LOG_STORE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
      TPL_TYPE_LOG_STORE, TplLogStore))
#define TPL_IS_LOG_STORE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
      TPL_TYPE_LOG_STORE))
#define TPL_LOG_STORE_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ( \
      (inst), TPL_TYPE_LOG_STORE, TplLogStoreInterface))

#define TPL_LOG_STORE_ERROR g_quark_from_static_string ("tpl-log-store-error-quark")
typedef enum
{
  /* generic error */
  TPL_LOG_STORE_ERROR_FAILED,
  /* generic failure for add_event() method, when nothing else applies */
  TPL_LOG_STORE_ERROR_ADD_EVENT,
  /* data is already present in the LogStore */
  TPL_LOG_STORE_ERROR_PRESENT,
  /* data is not present in the LogStore */
  TPL_LOG_STORE_ERROR_NOT_PRESENT,
  /* to be used in TplLogStoreIndexError as first value, so that value won't
   * overlap */
  TPL_LOG_STORE_ERROR_LAST
} TplLogStoreError;

typedef struct _TplLogStore TplLogStore;  /*dummy object */

typedef struct
{
  GTypeInterface parent;

  const gchar * (*get_name) (TplLogStore *self);
  gboolean (*exists) (TplLogStore *self, TpAccount *account,
      TplEntity *target, gint type_mask);
  gboolean (*add_event) (TplLogStore *self, TplEvent *event,
      GError **error);
  GList * (*get_dates) (TplLogStore *self, TpAccount *account,
      TplEntity *target, gint type_mask);
  GList * (*get_events_for_date) (TplLogStore *self, TpAccount *account,
      TplEntity *target, gint type_mask, const GDate *date);
  GList * (*get_recent_events) (TplLogStore *self, TpAccount *account,
      TplEntity *target, gint type_mask);
  GList * (*get_entities) (TplLogStore *self, TpAccount *account);
  GList * (*search_new) (TplLogStore *self, const gchar *text, gint type_mask);
  GList * (*get_filtered_events) (TplLogStore *self, TpAccount *account,
      TplEntity *target, gint type_mask, guint num_events,
      TplLogEventFilter filter, gpointer user_data);
  void (*clear) (TplLogStore *self);
  void (*clear_account) (TplLogStore *self, TpAccount *account);
  void (*clear_entity) (TplLogStore *self, TpAccount *account,
      TplEntity *entity);
} TplLogStoreInterface;

GType _tpl_log_store_get_type (void);

const gchar * _tpl_log_store_get_name (TplLogStore *self);
gboolean _tpl_log_store_exists (TplLogStore *self, TpAccount *account,
    TplEntity *target, gint type_mask);
gboolean _tpl_log_store_add_event (TplLogStore *self, TplEvent *event,
    GError **error);
GList * _tpl_log_store_get_dates (TplLogStore *self, TpAccount *account,
    TplEntity *target, gint type_mask);
GList * _tpl_log_store_get_events_for_date (TplLogStore *self,
    TpAccount *account, TplEntity *target, gint type_mask, const GDate *date);
GList * _tpl_log_store_get_recent_events (TplLogStore *self,
    TpAccount *account, TplEntity *target, gint type_mask);
GList * _tpl_log_store_get_entities (TplLogStore *self, TpAccount *account);
GList * _tpl_log_store_search_new (TplLogStore *self, const gchar *text,
    gint type_mask);
GList * _tpl_log_store_get_filtered_events (TplLogStore *self,
    TpAccount *account, TplEntity *target, gint type_mask, guint num_events,
    TplLogEventFilter filter, gpointer user_data);
void _tpl_log_store_clear (TplLogStore *self);
void _tpl_log_store_clear_account (TplLogStore *self, TpAccount *account);
void _tpl_log_store_clear_entity (TplLogStore *self, TpAccount *account,
    TplEntity *entity);
gboolean _tpl_log_store_is_writable (TplLogStore *self);
gboolean _tpl_log_store_is_readable (TplLogStore *self);

G_END_DECLS

#endif /*__TPL_LOG_STORE_H__ */
