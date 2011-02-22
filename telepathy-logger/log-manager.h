/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2011 Collabora Ltd.
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
 */

#ifndef __TPL_LOG_MANAGER_H__
#define __TPL_LOG_MANAGER_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <telepathy-glib/account.h>

#include <telepathy-logger/event.h>

G_BEGIN_DECLS
#define TPL_TYPE_LOG_MANAGER  (tpl_log_manager_get_type ())
#define TPL_LOG_MANAGER(o)  (G_TYPE_CHECK_INSTANCE_CAST ((o), TPL_TYPE_LOG_MANAGER, TplLogManager))
#define TPL_LOG_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_CAST ((k), TPL_TYPE_LOG_MANAGER, TplLogManagerClass))
#define TPL_IS_LOG_MANAGER(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), TPL_TYPE_LOG_MANAGER))
#define TPL_IS_LOG_MANAGER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), TPL_TYPE_LOG_MANAGER))
#define TPL_LOG_MANAGER_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), TPL_TYPE_LOG_MANAGER, TplLogManagerClass))

#define TPL_LOG_MANAGER_ERROR tpl_log_manager_errors_quark()

GQuark tpl_log_manager_errors_quark (void);

typedef enum
{
  TPL_LOG_MANAGER_ERROR_ADD_EVENT
} TplLogManagerError;


typedef struct
{
  GObject parent;

  gpointer priv;
} TplLogManager;

typedef struct
{
  GObjectClass parent_class;
} TplLogManagerClass;

typedef enum
{
  TPL_EVENT_SEARCH_TEXT       = 1 << 0,
  TPL_EVENT_SEARCH_TEXT_ROOM  = 1 << 1,
  TPL_EVENT_SEARCH_CALL       = 1 << 2,
  TPL_EVENT_SEARCH_ALL        = 0xffff
} TplEventSearchType;

typedef struct
{
  TpAccount *account;
  TplEntity *target;
  GDate *date;
} TplLogSearchHit;

typedef gboolean (*TplLogEventFilter) (TplEvent *event,
    gpointer user_data);

GType tpl_log_manager_get_type (void);

TplLogManager *tpl_log_manager_dup_singleton (void);

gboolean tpl_log_manager_exists (TplLogManager *manager,
    TpAccount *account,
    TplEntity *target);

void tpl_log_manager_get_dates_async (TplLogManager *manager,
    TpAccount *account,
    TplEntity *target,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tpl_log_manager_get_dates_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **dates,
    GError **error);

void tpl_log_manager_get_events_for_date_async (TplLogManager *manager,
    TpAccount *account,
    TplEntity *target,
    const GDate *date,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tpl_log_manager_get_events_for_date_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **events,
    GError **error);

void tpl_log_manager_get_filtered_events_async (TplLogManager *manager,
    TpAccount *account,
    TplEntity *target,
    guint num_events,
    TplLogEventFilter filter,
    gpointer filter_user_data,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tpl_log_manager_get_filtered_events_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **events,
    GError **error);

void tpl_log_manager_get_entities_async (TplLogManager *self,
    TpAccount *account,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tpl_log_manager_get_entities_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **entities,
    GError **error);

void tpl_log_manager_search_async (TplLogManager *manager,
    const gchar *text,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tpl_log_manager_search_finish (TplLogManager *self,
    GAsyncResult *result,
    GList **hits,
    GError **error);

void tpl_log_manager_search_free (GList *hits);

G_END_DECLS
#endif /* __TPL_LOG_MANAGER_H__ */
