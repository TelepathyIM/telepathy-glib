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

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>

#include <telepathy-glib/util.h>
#include <telepathy-glib/interfaces.h>

#include "empathy-log-manager.h"
#include "empathy-log-store-empathy.h"
#include "empathy-log-store.h"
#include "empathy-tp-chat.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include "empathy-debug.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyLogManager)
typedef struct
{
  GList *stores;
} EmpathyLogManagerPriv;

G_DEFINE_TYPE (EmpathyLogManager, empathy_log_manager, G_TYPE_OBJECT);

static EmpathyLogManager * manager_singleton = NULL;

static void
log_manager_finalize (GObject *object)
{
  EmpathyLogManagerPriv *priv;

  priv = GET_PRIV (object);

  g_list_foreach (priv->stores, (GFunc) g_object_unref, NULL);
  g_list_free (priv->stores);
}

static GObject *
log_manager_constructor (GType type,
                         guint n_props,
                         GObjectConstructParam *props)
{
  GObject *retval;
  EmpathyLogManagerPriv *priv;

  if (manager_singleton)
    {
      retval = g_object_ref (manager_singleton);
    }
  else
    {
      retval = G_OBJECT_CLASS (empathy_log_manager_parent_class)->constructor
          (type, n_props, props);

      manager_singleton = EMPATHY_LOG_MANAGER (retval);
      g_object_add_weak_pointer (retval, (gpointer *) &manager_singleton);

      priv = GET_PRIV (manager_singleton);

      priv->stores = g_list_append (priv->stores,
          g_object_new (EMPATHY_TYPE_LOG_STORE_EMPATHY, NULL));
    }

  return retval;
}

static void
empathy_log_manager_class_init (EmpathyLogManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = log_manager_constructor;
  object_class->finalize = log_manager_finalize;

  g_type_class_add_private (object_class, sizeof (EmpathyLogManagerPriv));
}

static void
empathy_log_manager_init (EmpathyLogManager *manager)
{
  EmpathyLogManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
      EMPATHY_TYPE_LOG_MANAGER, EmpathyLogManagerPriv);

  manager->priv = priv;
}

EmpathyLogManager *
empathy_log_manager_dup_singleton (void)
{
  return g_object_new (EMPATHY_TYPE_LOG_MANAGER, NULL);
}

gboolean
empathy_log_manager_add_message (EmpathyLogManager *manager,
                                 const gchar *chat_id,
                                 gboolean chatroom,
                                 EmpathyMessage *message,
                                 GError **error)
{
  EmpathyLogManagerPriv *priv;
  GList *l;
  gboolean out = FALSE;
  gboolean found = FALSE;

  /* TODO: When multiple log stores appear with add_message implementations
   * make this customisable. */
  const gchar *add_store = "Empathy";

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), FALSE);
  g_return_val_if_fail (chat_id != NULL, FALSE);
  g_return_val_if_fail (EMPATHY_IS_MESSAGE (message), FALSE);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      if (!tp_strdiff (empathy_log_store_get_name (
              EMPATHY_LOG_STORE (l->data)), add_store))
        {
          out = empathy_log_store_add_message (EMPATHY_LOG_STORE (l->data),
              chat_id, chatroom, message, error);
          found = TRUE;
          break;
        }
    }

  if (!found)
    DEBUG ("Failed to find chosen log store to write to.");

  return out;
}

gboolean
empathy_log_manager_exists (EmpathyLogManager *manager,
                            TpAccount *account,
                            const gchar *chat_id,
                            gboolean chatroom)
{
  GList *l;
  EmpathyLogManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), FALSE);
  g_return_val_if_fail (chat_id != NULL, FALSE);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      if (empathy_log_store_exists (EMPATHY_LOG_STORE (l->data),
            account, chat_id, chatroom))
        return TRUE;
    }

  return FALSE;
}

GList *
empathy_log_manager_get_dates (EmpathyLogManager *manager,
                               TpAccount *account,
                               const gchar *chat_id,
                               gboolean chatroom)
{
  GList *l, *out = NULL;
  EmpathyLogManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (chat_id != NULL, NULL);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      EmpathyLogStore *store = EMPATHY_LOG_STORE (l->data);
      GList *new;

      /* Insert dates of each store in the out list. Keep the out list sorted
       * and avoid to insert dups. */
      new = empathy_log_store_get_dates (store, account, chat_id, chatroom);
      while (new)
        {
          if (g_list_find_custom (out, new->data, (GCompareFunc) strcmp))
            g_free (new->data);
          else
            out = g_list_insert_sorted (out, new->data, (GCompareFunc) strcmp);

          new = g_list_delete_link (new, new);
        }
    }

  return out;
}

GList *
empathy_log_manager_get_messages_for_date (EmpathyLogManager *manager,
                                           TpAccount *account,
                                           const gchar *chat_id,
                                           gboolean chatroom,
                                           const gchar *date)
{
  GList *l, *out = NULL;
  EmpathyLogManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (chat_id != NULL, NULL);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      EmpathyLogStore *store = EMPATHY_LOG_STORE (l->data);

      out = g_list_concat (out, empathy_log_store_get_messages_for_date (
          store, account, chat_id, chatroom, date));
    }

  return out;
}

static gint
log_manager_message_date_cmp (gconstpointer a,
			      gconstpointer b)
{
	EmpathyMessage *one = (EmpathyMessage *) a;
	EmpathyMessage *two = (EmpathyMessage *) b;
	time_t one_time, two_time;

	one_time = empathy_message_get_timestamp (one);
	two_time = empathy_message_get_timestamp (two);

        /* Return -1 of message1 is older than message2 */
	return one_time < two_time ? -1 : one_time - two_time;
}

GList *
empathy_log_manager_get_filtered_messages (EmpathyLogManager *manager,
					   TpAccount *account,
					   const gchar *chat_id,
					   gboolean chatroom,
					   guint num_messages,
					   EmpathyLogMessageFilter filter,
					   gpointer user_data)
{
  EmpathyLogManagerPriv *priv;
  GList *out = NULL;
  GList *l;
  guint i = 0;

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (chat_id != NULL, NULL);

  priv = GET_PRIV (manager);

  /* Get num_messages from each log store and keep only the
   * newest ones in the out list. Keep that list sorted: Older first. */
  for (l = priv->stores; l; l = g_list_next (l))
    {
      EmpathyLogStore *store = EMPATHY_LOG_STORE (l->data);
      GList *new;

      new = empathy_log_store_get_filtered_messages (store, account, chat_id,
          chatroom, num_messages, filter, user_data);
      while (new)
        {
          if (i < num_messages)
            {
              /* We have less message than needed so far. Keep this message */
              out = g_list_insert_sorted (out, new->data,
                  (GCompareFunc) log_manager_message_date_cmp);
              i++;
            }
          else if (log_manager_message_date_cmp (new->data, out->data) > 0)
            {
              /* This message is newer than the oldest message we have in out
               * list. Remove the head of out list and insert this message */
              g_object_unref (out->data);
              out = g_list_delete_link (out, out);
              out = g_list_insert_sorted (out, new->data,
                  (GCompareFunc) log_manager_message_date_cmp);
            }
          else
            {
              /* This message is older than the oldest message we have in out
               * list. Drop it. */
              g_object_unref (new->data);
            }

          new = g_list_delete_link (new, new);
        }
    }

  return out;
}

GList *
empathy_log_manager_get_chats (EmpathyLogManager *manager,
                               TpAccount *account)
{
  GList *l, *out = NULL;
  EmpathyLogManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      EmpathyLogStore *store = EMPATHY_LOG_STORE (l->data);

      out = g_list_concat (out,
          empathy_log_store_get_chats (store, account));
    }

  return out;
}

GList *
empathy_log_manager_search_new (EmpathyLogManager *manager,
                                const gchar *text)
{
  GList *l, *out = NULL;
  EmpathyLogManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (!EMP_STR_EMPTY (text), NULL);

  priv = GET_PRIV (manager);

  for (l = priv->stores; l; l = g_list_next (l))
    {
      EmpathyLogStore *store = EMPATHY_LOG_STORE (l->data);

      out = g_list_concat (out,
          empathy_log_store_search_new (store, text));
    }

  return out;
}

void
empathy_log_manager_search_hit_free (EmpathyLogSearchHit *hit)
{
  if (hit->account != NULL)
    g_object_unref (hit->account);

  g_free (hit->date);
  g_free (hit->filename);
  g_free (hit->chat_id);

  g_slice_free (EmpathyLogSearchHit, hit);
}

void
empathy_log_manager_search_free (GList *hits)
{
  GList *l;

  for (l = hits; l; l = g_list_next (l))
    {
      empathy_log_manager_search_hit_free (l->data);
    }

  g_list_free (hits);
}

/* Format is just date, 20061201. */
gchar *
empathy_log_manager_get_date_readable (const gchar *date)
{
  time_t t;

  t = empathy_time_parse (date);

  return empathy_time_to_string_local (t, "%a %d %b %Y");
}

static void
log_manager_chat_received_message_cb (EmpathyTpChat *tp_chat,
                                      EmpathyMessage *message,
                                      EmpathyLogManager *log_manager)
{
  GError *error = NULL;
  TpHandleType handle_type;
  TpChannel *channel;

  channel = empathy_tp_chat_get_channel (tp_chat);
  tp_channel_get_handle (channel, &handle_type);

  if (!empathy_log_manager_add_message (log_manager,
        tp_channel_get_identifier (channel),
        handle_type == TP_HANDLE_TYPE_ROOM,
        message, &error))
    {
      DEBUG ("Failed to write message: %s",
          error ? error->message : "No error message");

      if (error != NULL)
        g_error_free (error);
    }
}

static void
log_manager_dispatcher_observe_cb (EmpathyDispatcher *dispatcher,
                                   EmpathyDispatchOperation *operation,
                                   EmpathyLogManager *log_manager)
{
  GQuark channel_type;

  channel_type = empathy_dispatch_operation_get_channel_type_id (operation);

  if (channel_type == TP_IFACE_QUARK_CHANNEL_TYPE_TEXT)
    {
      EmpathyTpChat *tp_chat;

      tp_chat = EMPATHY_TP_CHAT (
          empathy_dispatch_operation_get_channel_wrapper (operation));

      g_signal_connect (tp_chat, "message-received",
          G_CALLBACK (log_manager_chat_received_message_cb), log_manager);
    }
}


void
empathy_log_manager_observe (EmpathyLogManager *log_manager,
                             EmpathyDispatcher *dispatcher)
{
  g_signal_connect (dispatcher, "observe",
      G_CALLBACK (log_manager_dispatcher_observe_cb), log_manager);
}
