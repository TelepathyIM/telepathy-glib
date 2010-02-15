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

#include "config.h"
#include "dbus-service.h"

#include <glib.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/account.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/log-entry-text.h>
#include <telepathy-logger/log-manager.h>
#include <telepathy-logger/util.h>

#include <extensions/extensions.h>

#define DEBUG_FLAG TPL_DEBUG_DBUS_SERVICE
#include <telepathy-logger/debug.h>

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
  priv->manager = tpl_log_manager_dup_singleton ();
}


TplDBusService *
tpl_dbus_service_new (void)
{
  return g_object_new (TPL_TYPE_DBUS_SERVICE, NULL);
}


static GPtrArray *
tpl_assu_marshal (GList *data)
{
  guint idx;
  GList *data_ptr;
  GPtrArray *retval;

  retval = g_ptr_array_new_with_free_func ((GDestroyNotify) g_value_array_free);

  DEBUG ("Marshalled a(ssu) data:");
  for (idx = 0, data_ptr = data;
      data_ptr != NULL;
      data_ptr = g_list_next (data_ptr), ++idx)
    {
      TplLogEntry *log = data_ptr->data;

      gchar *message = g_strdup (tpl_log_entry_text_get_message (
          TPL_LOG_ENTRY_TEXT (log)));
      gchar *sender = g_strdup (tpl_contact_get_identifier (
          tpl_log_entry_text_get_sender (TPL_LOG_ENTRY_TEXT (log))));
      guint timestamp = tpl_log_entry_get_timestamp (log);

      g_ptr_array_add (retval, tp_value_array_build (3,
          G_TYPE_STRING, sender,
          G_TYPE_STRING, message,
          G_TYPE_INT64, timestamp,
          G_TYPE_INVALID));

      DEBUG ("%d = %s / %s / %d", idx, sender, message, timestamp);
    }
  return retval;
}


static void
tpl_dbus_service_get_recent_messages (TplSvcLogger *self,
    const gchar *account_path,
    const gchar *identifier,
    gboolean is_chatroom,
    guint lines,
    DBusGMethodInvocation *context)
{
  TplDBusServicePriv *priv = GET_PRIV (self);
  TpAccount *account = NULL;
  TpDBusDaemon *tp_dbus = NULL;
  GList *ret = NULL;
  GPtrArray *packed = NULL;
  GList *dates = NULL;
  GList *dates_ptr = NULL;
  GError *error = NULL;
  guint left_lines = lines;

  g_return_if_fail (TPL_IS_DBUS_SERVICE (self));
  g_return_if_fail (context != NULL);

  tp_dbus = tp_dbus_daemon_dup (&error);
  if (tp_dbus == NULL)
    {
      DEBUG ("Unable to acquire the bus daemon: %s", error->message);
      goto out;
    }

  account = tp_account_new (tp_dbus, account_path, &error);
  if (account == NULL)
    {
      DEBUG ("Unable to acquire the account for %s: %s", account_path,
          error->message);
      dbus_g_method_return_error (context, error);
      goto out;
    }

  dates = tpl_log_manager_get_dates (priv->manager, account, identifier,
      is_chatroom);
  if (dates == NULL)
    {
      error = g_error_new_literal (TPL_DBUS_SERVICE_ERROR,
          TPL_DBUS_SERVICE_ERROR_FAILED, "Error during date list retrieving, "
          "probably the account path or the identifier does not exist");
      dbus_g_method_return_error (context, error);
      goto out;
    }
  /* for each date returned, get at most <lines> lines, then if needed
   * check the previous date for the missing ones, and so on until
   * <lines> is reached, most recent date first */
  for (dates_ptr = g_list_last (dates);
      dates_ptr != NULL && left_lines > 0;
      dates_ptr = g_list_previous (dates_ptr))
    {
      gchar *date = dates_ptr->data;
      GList *messages = tpl_log_manager_get_messages_for_date (priv->manager,
          account, identifier, is_chatroom, date);
      GList *messages_ptr;

      /* from the most recent message, backward */
      for (messages_ptr = g_list_last (messages);
          messages_ptr != NULL && left_lines > 0;
          messages_ptr = g_list_previous (messages_ptr))
        {
          TplLogEntry *log = messages_ptr->data;
              /* keeps the reference and add to the result */
              ret = g_list_prepend (ret, g_object_ref (log));
              left_lines -= 1;
        }
      g_list_foreach (messages, (GFunc) g_object_unref, NULL);
      g_list_free (messages);
    }
  g_list_foreach (dates, (GFunc) g_free, NULL);
  g_list_free (dates);

  packed = tpl_assu_marshal (ret);
  g_list_foreach (ret, (GFunc) g_object_unref, NULL);
  g_list_free (ret);

  tpl_svc_logger_return_from_get_recent_messages (context, packed);

out:
  if (account != NULL)
    g_object_unref (account);

  if (tp_dbus != NULL)
    g_object_unref (tp_dbus);
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
