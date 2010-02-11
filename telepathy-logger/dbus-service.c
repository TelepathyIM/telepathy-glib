/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
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
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "dbus-service.h"

#include <glib.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/account.h>

#include <telepathy-logger/log-entry-text.h>
#include <telepathy-logger/log-manager.h>

#include <extensions/extensions.h>

#define DEBUG_FLAG TPL_DEBUG_DBUS_SERVICE
#include <telepathy-logger/debug.h>

#define DBUS_STRUCT_STRING_STRING_UINT \
  (dbus_g_type_get_struct ("GValueArray", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_INVALID))

static void tpl_logger_iface_init (gpointer iface, gpointer iface_data);

#define GET_PRIV(obj) TPL_GET_PRIV (obj, TplDBusService)
struct _TplDBusServicePriv
{
  TplLogManager *manager;
};

G_DEFINE_TYPE_WITH_CODE (TplDBusService, tpl_dbus_service, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TPL_TYPE_SVC_LOGGER, tpl_logger_iface_init));

static void
tpl_dbus_service_class_init (TplDBusServiceClass *klass)
{
  GObjectClass* object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (object_class, sizeof (TplDBusServicePriv));
}


static void
tpl_dbus_service_init (TplDBusService *self)
{
  TplDBusServicePriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_DBUS_SERVICE, TplDBusServicePriv);

  g_return_if_fail (TPL_IS_DBUS_SERVICE (self));

  self->priv = priv;
  GET_PRIV (self)->manager = tpl_log_manager_dup_singleton ();
}


TplDBusService *
tpl_dbus_service_new (void)
{
  return g_object_new (TPL_TYPE_DBUS_SERVICE, NULL);
}


static gboolean
_pack_last_chats_answer (GList *data,
    GPtrArray **array)
{
  guint data_idx;
  GPtrArray *retval;

  (*array) = g_ptr_array_new_with_free_func ((GDestroyNotify) g_value_array_free);
  retval = *array;

  for (data_idx = 0; data_idx < g_list_length (data); ++data_idx)
    {
      TplLogEntry *log = g_list_nth_data (data, data_idx);

      GValue *value = g_new0 (GValue, 1);

      gchar *message = g_strdup (tpl_log_entry_text_get_message (
          TPL_LOG_ENTRY_TEXT (log)));
      gchar *sender = g_strdup (tpl_contact_get_identifier (
          tpl_log_entry_text_get_sender (TPL_LOG_ENTRY_TEXT (log))));
      guint timestamp = tpl_log_entry_get_timestamp (log);

      g_value_init (value, DBUS_STRUCT_STRING_STRING_UINT);
      g_value_take_boxed (value, dbus_g_type_specialized_construct (
          DBUS_STRUCT_STRING_STRING_UINT));

      dbus_g_type_struct_set (value, 0, sender, 1, message, 2, timestamp,
          G_MAXUINT);
      g_ptr_array_add (retval, g_value_get_boxed (value));
      g_free (value);

      DEBUG ("retval[%d]=\"[%d] <%s>: %s\"\n", data_idx,
          timestamp, sender, message);
    }
  return TRUE;
}

static void
tpl_dbus_service_get_recent_messages (TplSvcLogger *self,
    const gchar *account_path,
    const gchar *identifier,
    gboolean is_chatroom,
    guint lines,
    DBusGMethodInvocation *context)
{
  guint dates_idx;
  gint msgs_idx;
  GError *error = NULL;
  TpAccount *account;
  DBusGConnection *dbus;
  TpDBusDaemon *tp_dbus;
  GList *ret = NULL;
  GPtrArray *answer = NULL;
  GList *dates = NULL;
  guint left_lines = lines;
  TplDBusServicePriv *priv = GET_PRIV (self);

  g_return_if_fail (TPL_IS_DBUS_SERVICE (self));
  g_return_if_fail (context != NULL);

  dbus = tp_get_bus ();
  tp_dbus = tp_dbus_daemon_new (dbus);

  account = tp_account_new (tp_dbus, account_path, &error);
  if (error != NULL)
    {
      GError *loc_error = NULL;

      DEBUG ("TpAccount creation: %s", error->message);
      g_propagate_error (&loc_error, error);
      dbus_g_method_return_error (context, loc_error);

      g_error_free (error);
      g_error_free (loc_error);
      g_object_unref (tp_dbus);
      g_object_unref (dbus);
      return;
    }

  dates = tpl_log_manager_get_dates (priv->manager, account, identifier,
      is_chatroom);
  if (dates == NULL)
    {
      error = g_error_new_literal (TPL_DBUS_SERVICE_ERROR,
          TPL_DBUS_SERVICE_ERROR_FAILED, "Error during date list retrieving, "
          "probably the account path or the identifier are does not exist");
      dbus_g_method_return_error (context, error);

      g_object_unref (account);
      g_object_unref (tp_dbus);
      g_object_unref (dbus);
      return;
    }
  dates = g_list_reverse (dates);

  for (dates_idx = 0; dates_idx < g_list_length (dates) && left_lines > 0;
      ++dates_idx)
    {
      gchar *date = g_list_nth_data (dates, dates_idx);
      GList *messages = tpl_log_manager_get_messages_for_date (priv->manager,
          account, identifier, is_chatroom, date);
      guint msgs_len = g_list_length (messages);
      gint guard = (msgs_len>=left_lines ? left_lines : msgs_len);

      for (msgs_idx=msgs_len-1; guard>0 && left_lines>0; --guard, --msgs_idx)
        {
          TplLogEntry *log = g_list_nth_data (messages, msgs_idx);
          g_object_ref (log);
          ret = g_list_prepend (ret, log);
          left_lines-=1;
        }
      g_list_foreach (messages, (GFunc) g_object_unref, NULL);
    }
  g_list_foreach (dates, (GFunc) g_free, NULL);

  _pack_last_chats_answer (ret, &answer);
  g_list_foreach (ret, (GFunc) g_object_unref, NULL);

  tpl_svc_logger_return_from_get_recent_messages (context, answer);

  g_object_unref (account);
  g_object_unref (tp_dbus);
  g_object_unref (dbus);
}


static void
tpl_logger_iface_init (gpointer iface,
    gpointer iface_data)
{
  TplSvcLoggerClass *klass = (TplSvcLoggerClass *) iface;

#define IMPLEMENT(x) tpl_svc_logger_implement_##x (klass, tpl_dbus_service_##x)
  IMPLEMENT (get_recent_messages);
#undef IMPLEMENT
}
