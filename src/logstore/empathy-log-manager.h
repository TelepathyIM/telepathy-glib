/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_LOG_MANAGER_H__
#define __EMPATHY_LOG_MANAGER_H__

#include <glib-object.h>

#include "empathy-message.h"
#include "empathy-dispatcher.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_LOG_MANAGER (empathy_log_manager_get_type ())
#define EMPATHY_LOG_MANAGER(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_LOG_MANAGER, \
                               EmpathyLogManager))
#define EMPATHY_LOG_MANAGER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_LOG_MANAGER, \
                            EmpathyLogManagerClass))
#define EMPATHY_IS_LOG_MANAGER(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_LOG_MANAGER))
#define EMPATHY_IS_LOG_MANAGER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_LOG_MANAGER))
#define EMPATHY_LOG_MANAGER_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_LOG_MANAGER, \
                              EmpathyLogManagerClass))

typedef struct _EmpathyLogManager EmpathyLogManager;
typedef struct _EmpathyLogManagerClass EmpathyLogManagerClass;
typedef struct _EmpathyLogSearchHit EmpathyLogSearchHit;

struct _EmpathyLogManager
{
  GObject parent;
  gpointer priv;
};

struct _EmpathyLogManagerClass
{
  GObjectClass parent_class;
};

struct _EmpathyLogSearchHit
{
  TpAccount *account;
  gchar     *chat_id;
  gboolean   is_chatroom;
  gchar     *filename;
  gchar     *date;
};

typedef gboolean (*EmpathyLogMessageFilter) (EmpathyMessage *message,
    gpointer user_data);

GType empathy_log_manager_get_type (void) G_GNUC_CONST;
EmpathyLogManager *empathy_log_manager_dup_singleton (void);
gboolean empathy_log_manager_add_message (EmpathyLogManager *manager,
    const gchar *chat_id, gboolean chatroom, EmpathyMessage *message,
    GError **error);
gboolean empathy_log_manager_exists (EmpathyLogManager *manager,
    TpAccount *account, const gchar *chat_id, gboolean chatroom);
GList *empathy_log_manager_get_dates (EmpathyLogManager *manager,
    TpAccount *account, const gchar *chat_id, gboolean chatroom);
GList *empathy_log_manager_get_messages_for_date (EmpathyLogManager *manager,
    TpAccount *account, const gchar *chat_id, gboolean chatroom,
    const gchar *date);
GList *empathy_log_manager_get_filtered_messages (EmpathyLogManager *manager,
    TpAccount *account, const gchar *chat_id, gboolean chatroom,
    guint num_messages, EmpathyLogMessageFilter filter, gpointer user_data);
GList *empathy_log_manager_get_chats (EmpathyLogManager *manager,
    TpAccount *account);
GList *empathy_log_manager_search_new (EmpathyLogManager *manager,
    const gchar *text);
void empathy_log_manager_search_free (GList *hits);
gchar *empathy_log_manager_get_date_readable (const gchar *date);
void empathy_log_manager_search_hit_free (EmpathyLogSearchHit *hit);
void empathy_log_manager_observe (EmpathyLogManager *log_manager,
    EmpathyDispatcher *dispatcher);

G_END_DECLS

#endif /* __EMPATHY_LOG_MANAGER_H__ */
